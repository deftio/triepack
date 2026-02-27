// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

use triepack;

#[test]
fn test_encode_returns_err() {
    let data = vec![("hello", "world")];
    let result = triepack::encode(&data);
    assert!(result.is_err());
    assert_eq!(result.unwrap_err(), "not implemented");
}
