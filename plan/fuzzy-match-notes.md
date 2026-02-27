# Fuzzy Match in TXZ: Analysis

---

## What You Asked For

Search for a string, and even if parts don't match, return the top N closest results. This is approximate/fuzzy string matching.

## Honest Assessment

A trie is **good for some fuzzy operations and bad for others**. Here's the breakdown:

### What Tries Are Good At

| Operation | Trie support | Notes |
|-----------|-------------|-------|
| **Prefix search** | Excellent | Walk to a node, enumerate descendants. Free with the trie structure. |
| **Bounded edit-distance search** (Levenshtein <= d) | Feasible for small d | Walk the trie exploring all paths within d edits. Exponential in d but pruned by the trie structure. Practical for d=1, tolerable for d=2, painful for d=3+. |
| **Completion / autocomplete** | Excellent | Prefix search + optional ranking by stored value (frequency). The original TXZ use case. |
| **Suffix search** | Good (with suffix trie) | TXZ has a suffix trie -- can search for keys ending with a given string. |

### What Tries Are Bad At

| Operation | Trie support | Notes |
|-----------|-------------|-------|
| **Arbitrary substring search** | Poor | Would need to walk every path. No better than linear scan. |
| **Phonetic similarity** ("night" ~ "nite") | Not supported | Requires a phonetic encoding layer (Soundex, Metaphone) on top. |
| **Semantic similarity** ("car" ~ "automobile") | Not supported | Requires embeddings, completely different domain. |
| **Top-N by edit distance globally** | Expensive | Must explore the full trie with bounded edits, collect all matches, sort by distance. Works, but not what tries are optimized for. |

### The Core Problem with Tries and Fuzzy Match

Edit-distance search in a trie works by walking the trie while maintaining an edit-distance state machine. At each node, you consider:
- Match (consume input char + advance in trie: cost 0)
- Substitution (consume input char + advance in trie: cost 1)
- Insertion (consume input char + stay in trie: cost 1)
- Deletion (don't consume input + advance in trie: cost 1)

This is effectively a trie-guided dynamic programming walk. The trie prunes the search space (you never explore branches that don't exist in the dictionary), but the branching factor at each edit still explodes:

```
d=0: O(key_length)                -- exact match, trivial
d=1: O(key_length * alphabet)     -- manageable (~26x for English)
d=2: O(key_length * alphabet^2)   -- starting to hurt (~676x)
d=3: O(key_length * alphabet^3)   -- impractical for large dictionaries
```

For a 100K word English dictionary with d=1, this is fast (milliseconds). With d=2, it's seconds. With d=3, it's "go get coffee."

## Recommendation

### Tier 1: Build Into TXZ (Trie-Native)

These operations are natural for the trie and should be part of the TXZ query API:

1. **Prefix search**: `txz_find_prefix("wor") -> iterator over matching keys`
2. **Bounded edit-distance search (d <= 2)**: `txz_find_fuzzy("wrold", max_distance=1) -> [(key, distance, value), ...]`
3. **Suffix search** (leveraging suffix trie): `txz_find_suffix("ing") -> iterator`
4. **Prefix + suffix combined**: keys starting with X and ending with Y

These are implemented as trie walk algorithms with no additional data structures beyond what TXZ already has.

### Tier 2: Separate Index Layer (If Needed)

If faster or deeper fuzzy match is needed, build a separate index that references TXZ entries:

- **BK-tree** or **VP-tree** over the key set, using edit distance as the metric. Store key indices (not the strings themselves) and look up values in the TXZ. This gives O(log N) fuzzy lookup for any edit distance.
- **n-gram index**: break keys into character n-grams, build an inverted index. Fast approximate candidate generation, then verify with exact edit distance.
- **Precomputed Soundex/Metaphone**: store phonetic encodings as additional keys in the TXZ itself (e.g. key "night" also indexed under Soundex code "N230").

These are separate data structures that can live alongside a TXZ or reference into it. They don't change the TXZ format.

### Tier 3: Not TXZ's Job

Semantic similarity, embedding-based search, etc. -- different problem domain entirely.

## Implementation Sketch: Trie Edit-Distance Walk

For Tier 1, the algorithm is well-known (sometimes called "Levenshtein automaton over a trie"):

```
function fuzzy_search(trie, query, max_distance):
    results = []
    // row[i] = edit distance between query[0..i] and current trie path
    initial_row = [0, 1, 2, ..., len(query)]

    for each child of trie root:
        _fuzzy_recurse(child, query, initial_row, max_distance, "", results)

    return results sorted by distance

function _fuzzy_recurse(node, query, prev_row, max_distance, current_word, results):
    cols = len(query) + 1
    current_row = [prev_row[0] + 1]  // deletion

    for i in 1..cols:
        insert_cost  = current_row[i-1] + 1
        delete_cost  = prev_row[i] + 1
        replace_cost = prev_row[i-1] + (0 if query[i-1] == node.symbol else 1)
        current_row.append(min(insert_cost, delete_cost, replace_cost))

    // If last entry in row is <= max_distance, this is a match
    if current_row[cols-1] <= max_distance and node.is_terminal:
        results.append((current_word + node.symbol, current_row[cols-1], node.value))

    // If any entry in row is <= max_distance, there's hope -- recurse into children
    if min(current_row) <= max_distance:
        for each child of node:
            _fuzzy_recurse(child, query, current_row, max_distance, current_word + node.symbol, results)
```

This is a direct DP walk over the trie. The trie structure prunes branches that can't possibly match within the distance bound. For d=1 on a well-structured trie, it's fast.

### Interaction with Suffix Trie

When the walk hits a SUFFIX jump, the algorithm needs to continue the DP into the suffix trie. This is tractable: the remaining query characters are compared against the suffix string, extending the edit-distance row. But it means the suffix trie must be navigable (not just a flat lookup) for fuzzy search to work through suffixes.

## Summary

| Feature | Feasibility in TXZ | Recommendation |
|---------|--------------------|-----------------|
| Prefix search | Trivial | Build in (Tier 1) |
| Autocomplete | Trivial + ranking | Build in (Tier 1) |
| Edit distance d=1 | Fast | Build in (Tier 1) |
| Edit distance d=2 | Tolerable | Build in (Tier 1), warn about perf |
| Edit distance d=3+ | Slow | Separate index (Tier 2) |
| Suffix search | Natural (have suffix trie) | Build in (Tier 1) |
| Phonetic match | Needs encoding layer | Tier 2 (Soundex/Metaphone keys) |
| Substring search | Expensive | Tier 2 (n-gram index) |
| Semantic search | Wrong tool | Tier 3 (not TXZ's job) |
