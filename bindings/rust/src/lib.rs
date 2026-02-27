// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

//! triepack — Native Rust implementation of the Triepack .trp binary format.

/// Encode key-value data into the .trp binary format.
///
/// # Errors
///
/// Returns `Err("not implemented")` — this function is a stub.
pub fn encode(_data: &[(&str, &str)]) -> Result<Vec<u8>, &'static str> {
    Err("not implemented")
}

/// Decode a .trp binary buffer into key-value data.
///
/// # Errors
///
/// Returns `Err("not implemented")` — this function is a stub.
pub fn decode(_buffer: &[u8]) -> Result<Vec<(String, String)>, &'static str> {
    Err("not implemented")
}
