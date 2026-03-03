#!/usr/bin/env python3
# Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.
"""Generate benchmark_100k.json — a ~100KB synthetic product catalog."""

import json
import os
import random
import sys

random.seed(42)  # deterministic output

ADJECTIVES = [
    "Advanced", "Basic", "Classic", "Deluxe", "Elite", "Fast", "Global",
    "High-End", "Industrial", "Junior", "Keen", "Lightweight", "Modern",
    "Next-Gen", "Original", "Premium", "Quality", "Robust", "Smart", "Turbo",
    "Ultra", "Versatile", "Wireless", "Xtreme", "Young", "Zero-Lag",
]

NOUNS = [
    "Adapter", "Battery", "Cable", "Display", "Engine", "Filter", "Gateway",
    "Hub", "Interface", "Jack", "Keyboard", "Lens", "Module", "Node",
    "Optimizer", "Processor", "Router", "Sensor", "Terminal", "Unit",
    "Valve", "Widget", "Xpander", "Yielder", "Zipper",
]

CATEGORIES = [
    "Electronics", "Hardware", "Software", "Networking", "Storage",
    "Peripherals", "Accessories", "Audio", "Video", "Power",
]

TAGS_POOL = [
    "sale", "new", "bestseller", "limited", "eco-friendly", "refurbished",
    "clearance", "featured", "exclusive", "bundle", "warranty", "free-shipping",
]


def make_product(i):
    adj = ADJECTIVES[i % len(ADJECTIVES)]
    noun = NOUNS[i % len(NOUNS)]
    name = f"{adj} {noun} {i + 1}"
    price = round(random.uniform(4.99, 999.99), 2)
    rating = round(random.uniform(1.0, 5.0), 1)
    in_stock = random.choice([True, False])
    quantity = random.randint(0, 500) if in_stock else 0
    category = CATEGORIES[i % len(CATEGORIES)]
    n_tags = random.randint(1, 4)
    tags = random.sample(TAGS_POOL, n_tags)
    n_reviews = random.randint(0, 3)
    reviews = []
    for r in range(n_reviews):
        reviews.append({
            "author": f"user_{random.randint(1000, 9999)}",
            "score": random.randint(1, 5),
            "text": f"Review {r + 1} for {name}. " + "Great product! " * random.randint(1, 3),
            "verified": random.choice([True, False]),
        })

    return {
        "id": i + 1,
        "sku": f"SKU-{i + 1:05d}",
        "name": name,
        "description": f"The {name} delivers outstanding performance with "
                       f"cutting-edge technology. Perfect for professionals "
                       f"and enthusiasts alike.",
        "price": price,
        "currency": "USD",
        "rating": rating,
        "in_stock": in_stock,
        "quantity": quantity,
        "category": category,
        "tags": tags,
        "weight_kg": round(random.uniform(0.1, 25.0), 2) if random.random() > 0.2 else None,
        "dimensions": {
            "width_cm": round(random.uniform(1, 100), 1),
            "height_cm": round(random.uniform(1, 100), 1),
            "depth_cm": round(random.uniform(1, 50), 1),
        },
        "manufacturer": {
            "name": f"Corp-{chr(65 + (i % 26))}",
            "country": random.choice(["US", "DE", "JP", "CN", "KR", "TW"]),
            "warranty_months": random.choice([6, 12, 24, 36]),
        },
        "reviews": reviews,
    }


def main():
    catalog = {
        "catalog_version": "2.1.0",
        "generated": "2026-01-15T00:00:00Z",
        "total_products": 200,
        "metadata": {
            "store": "TriePack Benchmark Store",
            "api_version": 3,
            "features_enabled": ["search", "filter", "sort", "pagination"],
            "pagination": {
                "page": 1,
                "per_page": 200,
                "total_pages": 1,
            },
        },
        "products": [make_product(i) for i in range(200)],
    }

    out = json.dumps(catalog, indent=2, ensure_ascii=False)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    dest = os.path.join(script_dir, "..", "tests", "data", "benchmark_100k.json")
    dest = os.path.normpath(dest)
    with open(dest, "w", encoding="utf-8") as f:
        f.write(out)
        f.write("\n")

    print(f"Wrote {os.path.getsize(dest):,} bytes to {dest}")


if __name__ == "__main__":
    main()
