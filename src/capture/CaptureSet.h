// EXT-17 — reading a rotated capture as the set it is (§6.7).
//
// A run recorded with `--on-size-limit rotate` produces a numbered set of files rather than one
// file. Every part is a complete, independently valid capture with its own header, its own
// schemas and its own trailer, so `readFile` reads any part correctly and completely — it
// simply does not know the part has siblings.
//
// This is what knows. Two rules from §6.7 are the whole reason it exists:
//
//   1. **Segment ordinals restart at 0 in every part.** They are unique within a file, which is
//      all §7 promises. A per-segment statistic across a set must key on `(part, segment)`.
//   2. **`counts` in each trailer is that file's own.** The run's totals are the sum across
//      parts; no part states them, because part 0's trailer is written long before the run ends.
//
// Written from the specification alone, like the rest of `src/capture/`. Links nothing.
#pragma once

#include "CaptureReader.h"

#include <string>

namespace ext17::capture {

struct SetOptions {
    ReadOptions read;
    // A guard, not a policy: a corrupt `continued_in` that points at an already-visited file
    // would otherwise walk forever. A cycle is detected directly; this bounds the pathological
    // case where it is not a cycle but is unreasonably long.
    std::size_t maxParts = 4096;
};

// Read a rotated set starting from its first part. Handed a capture that was never rotated —
// no `continued_in` — this returns a one-part set, which is the correct answer and is why a
// caller never has to know in advance which it has.
//
// `sink` is handed to every part in turn, so a consumer that needs the records themselves sees
// the whole run's stream in file order and still never has the reader retain any of it. Each
// `RecordView` carries `(part, segment)` already, which is the key a per-segment statistic over
// a set has to use (§6.7). Added at M4 for the determinism comparison: without it the comparison
// would have to read every capture twice, once for its structure and once for its samples.
SetResult readSet(const std::string& firstPartPath, const SetOptions& options = {},
                  RecordSink* sink = nullptr);

} // namespace ext17::capture
