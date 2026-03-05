// swift-tools-version: 5.9
// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

import PackageDescription

let package = Package(
    name: "Triepack",
    products: [
        .library(name: "Triepack", targets: ["Triepack"]),
    ],
    targets: [
        .target(
            name: "Triepack",
            path: "Sources/Triepack"
        ),
        .testTarget(
            name: "TriepackTests",
            dependencies: ["Triepack"],
            path: "Tests/TriepackTests",
            resources: [
                .copy("fixtures"),
            ]
        ),
    ]
)
