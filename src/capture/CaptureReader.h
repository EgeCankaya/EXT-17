// EXT-17 — the conformant reader for `n8ro-capture/1`.
//
// Written from `contract/capture-format-v1.md` alone. Links nothing: not EXT-08, not the N8RO
// SDK, not a third-party JSON library. See Capture.h for the boundary and the severities.
//
// It is **streaming**: one `getline` at a time, per §2, so it reads a capture larger than
// memory. What it retains is the header, the trailer, per-segment statistics and per-occupancy
// state — never the samples. A consumer that needs the samples themselves (M4's content
// comparison, M6's assertions) supplies a `RecordSink` and sees each record as it goes by.
#pragma once

#include "Capture.h"
#include "../common/JsonParse.h"

#include <string>
#include <vector>

namespace ext17::capture {

// One record, as it passes. Valid only for the duration of the callback: the reader reuses its
// parse buffer for the next line, which is what makes it streaming.
struct RecordView {
    std::string type;
    const json::Value* record = nullptr;
    std::size_t line = 0;
    SegmentKey segment;          // the enclosing segment; {part, -1} for header and trailer
};

class RecordSink {
public:
    virtual ~RecordSink() = default;
    virtual void onRecord(const RecordView& view) = 0;
};

struct ReadOptions {
    // Check each `sample.fields` against the header's schema declaration: keys the schema does
    // not declare, the normative declaration order (§8.2), the encoding implied by each
    // declared type, and array lengths. On by default — it is most of what "conformant" means.
    bool verifyFields = true;

    // Diagnostics are capped per code, because a systematic producer defect would otherwise
    // produce one entry per sample and a 50 000-line report nobody reads. The *counts* are
    // never capped: `diagnosticCounts` always states how many there really were.
    std::size_t maxDiagnosticsPerCode = 8;
};

// Read one capture file. A rotated run is a set of files; see CaptureSet.h.
ReadResult readFile(const std::string& path,
                    const ReadOptions& options = {},
                    RecordSink* sink = nullptr);

// Read a capture already in memory, line by line. This is what the conformance suite uses for
// the synthetic micro-captures: a capture small enough to read in the test that asserts on it
// is worth more than one hidden in a fixture file.
ReadResult readLines(const std::vector<std::string>& lines,
                     const std::string& label,
                     const ReadOptions& options = {},
                     RecordSink* sink = nullptr);

// §3 step 2, and CR-CAP-3's first acceptance criterion, taken literally: `format_version` is
// the first key of the first line so that it can be checked *before any other parsing*. This
// does that scan and nothing else. Returns false when the line is not shaped like a capture
// header at all, which the caller then reports as `not_a_capture`.
bool peekFormatVersion(const std::string& firstLine, std::string& versionOut);

} // namespace ext17::capture
