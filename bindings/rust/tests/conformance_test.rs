// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

//! Cross-language conformance suite.
//!
//! Reads `tests/conformance/cases.txt` -- the corpus every implementation
//! shares -- and checks this binding against the C-generated fixtures: decoding
//! a fixture must produce the corpus values, and encoding the corpus values
//! must produce the fixture byte for byte.
//!
//! See `tests/conformance/README.md`.

use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

use triepack::{decode, encode, Value};

fn conformance_dir() -> PathBuf {
    let mut p = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    p.push("..");
    p.push("..");
    p.push("tests");
    p.push("conformance");
    p
}

// -- Corpus parsing --------------------------------------------------------

/// Decode a corpus token ("~" for empty, %XX escapes) into bytes.
fn token_bytes(tok: &str) -> Vec<u8> {
    if tok == "~" {
        return Vec::new();
    }
    let b = tok.as_bytes();
    let mut out = Vec::with_capacity(b.len());
    let mut i = 0;
    while i < b.len() {
        if b[i] == b'%' {
            let hi = (b[i + 1] as char).to_digit(16).expect("bad escape");
            let lo = (b[i + 2] as char).to_digit(16).expect("bad escape");
            out.push((hi * 16 + lo) as u8);
            i += 3;
        } else {
            out.push(b[i]);
            i += 1;
        }
    }
    out
}

fn token_string(tok: &str) -> String {
    String::from_utf8(token_bytes(tok)).expect("corpus token is not UTF-8")
}

fn hex_bytes(tok: &str) -> Vec<u8> {
    if tok == "~" {
        return Vec::new();
    }
    (0..tok.len())
        .step_by(2)
        .map(|i| u8::from_str_radix(&tok[i..i + 2], 16).expect("bad hex"))
        .collect()
}

struct Case {
    name: String,
    encode_expected: bool,
    entries: Vec<(String, String, Option<String>)>,
}

impl Case {
    fn fixture(&self) -> Vec<u8> {
        let mut p = conformance_dir();
        p.push("fixtures");
        p.push(format!("{}.trp", self.name));
        fs::read(&p).unwrap_or_else(|e| panic!("case {}: {}: {}", self.name, p.display(), e))
    }

    fn data(&self) -> HashMap<String, Value> {
        self.entries
            .iter()
            .map(|(k, t, arg)| (k.clone(), corpus_value(t, arg.as_deref())))
            .collect()
    }
}

/// Build the Rust value a corpus entry describes.
fn corpus_value(type_name: &str, arg: Option<&str>) -> Value {
    match type_name {
        "null" => Value::Null,
        "bool" => Value::Bool(arg == Some("1")),
        "int" => Value::Int(arg.unwrap().parse().expect("bad int")),
        "uint" => Value::UInt(arg.unwrap().parse().expect("bad uint")),
        "f64" => Value::Float64(f64::from_bits(
            u64::from_str_radix(arg.unwrap(), 16).expect("bad f64"),
        )),
        "f32" => Value::Float64(
            f32::from_bits(u32::from_str_radix(arg.unwrap(), 16).expect("bad f32")) as f64,
        ),
        "str" => Value::String(token_string(arg.unwrap())),
        "blob" => Value::Blob(hex_bytes(arg.unwrap())),
        other => panic!("unknown corpus type: {}", other),
    }
}

fn parse_cases() -> Vec<Case> {
    let mut p = conformance_dir();
    p.push("cases.txt");
    let text = fs::read_to_string(&p).unwrap_or_else(|e| panic!("{}: {}", p.display(), e));

    let mut cases: Vec<Case> = Vec::new();
    for line in text.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let parts: Vec<&str> = line.split(' ').collect();
        match parts[0] {
            "case" => cases.push(Case {
                name: parts[1].to_string(),
                encode_expected: !parts.contains(&"encode=no"),
                entries: Vec::new(),
            }),
            "key" => {
                let cur = cases.last_mut().expect("key outside a case");
                cur.entries.push((
                    token_string(parts[1]),
                    parts[2].to_string(),
                    parts.get(3).map(|s| s.to_string()),
                ));
            }
            other => panic!("unknown corpus directive: {}", other),
        }
    }
    cases
}

/// Compare a decoded value with the expected one. Doubles compare by bit
/// pattern so -0.0 is distinguished, with all NaNs treated as equal.
fn values_match(got: &Value, want: &Value) -> bool {
    match (got, want) {
        (Value::Float64(g), Value::Float64(w)) => {
            (g.is_nan() && w.is_nan()) || g.to_bits() == w.to_bits()
        }
        _ => got == want,
    }
}

// -- The suite -------------------------------------------------------------

#[test]
fn conformance_corpus_and_fixtures_present() {
    let cases = parse_cases();
    assert!(!cases.is_empty(), "corpus is empty");
    for c in &cases {
        let mut p = conformance_dir();
        p.push("fixtures");
        p.push(format!("{}.trp", c.name));
        assert!(p.exists(), "case {}: missing fixture", c.name);
    }
}

#[test]
fn conformance_decodes_c_fixtures() {
    for c in parse_cases() {
        let result = decode(&c.fixture())
            .unwrap_or_else(|e| panic!("case {}: decode failed: {}", c.name, e));
        let want = c.data();
        assert_eq!(result.len(), want.len(), "case {}: key count", c.name);
        for (k, v) in &want {
            let got = result
                .get(k)
                .unwrap_or_else(|| panic!("case {}: missing key {:?}", c.name, k));
            assert!(
                values_match(got, v),
                "case {}: key {:?}: got {:?}, want {:?}",
                c.name,
                k,
                got,
                v
            );
        }
    }
}

#[test]
fn conformance_encodes_like_c_reference() {
    for c in parse_cases() {
        // Cases marked encode=no describe values this binding cannot
        // reproduce exactly (float32, or doubles some languages read back as
        // integers).
        if !c.encode_expected {
            continue;
        }
        let got = encode(&c.data());
        let want = c.fixture();
        assert_eq!(
            got,
            want,
            "case {}: encoded {} bytes, C reference is {}",
            c.name,
            got.len(),
            want.len()
        );
    }
}

// -- Malformed inputs ------------------------------------------------------

/// Each file is a valid fixture with one field damaged. Damage inside the data
/// is re-sealed with a correct CRC, so the reader has to catch it rather than
/// being handed a checksum failure.
#[test]
fn conformance_rejects_malformed() {
    let mut dir = conformance_dir();
    dir.push("malformed");
    let mut seen = 0;
    for entry in fs::read_dir(&dir).expect("cannot read malformed corpus") {
        let path = entry.expect("bad dir entry").path();
        if path.extension().and_then(|e| e.to_str()) != Some("trp") {
            continue;
        }
        seen += 1;
        let buf = fs::read(&path).expect("cannot read fixture");
        assert!(
            decode(&buf).is_err(),
            "{}: accepted malformed input",
            path.display()
        );
    }
    assert!(seen > 0, "malformed corpus is empty");
}
