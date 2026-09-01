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

    // The longest line this reader will hold in memory. It exists because without it the reader
    // accumulates until it meets an LF, so a file with no LF is read entirely into one string -
    // and the format's reject rule (3 step 2) cannot fire until that has happened. Measured
    // before the bound existed: rejecting a 512 MB file with no newline cost 1 008 MB of peak
    // working set. That is an allocation failure waiting for a large enough file, and an
    // allocation failure is an exception, which [B]'s rule 7 forbids outright.
    //
    // 16 MiB is about thirteen thousand times the longest line in any capture measured here
    // (1 227 bytes, a header carrying 42 entities' schemas), so no real record can reach it.
    // It is settable so that the tests can manufacture the case with a small file instead of a
    // large one - a bound that can only be exercised by writing 16 MB is a bound nobody tests.
    std::size_t maxLineBytes = 16u * 1024u * 1024u;
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
