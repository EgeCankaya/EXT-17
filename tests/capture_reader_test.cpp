// EXT-17 — the capture reader's conformance suite.
//
// This is the artifact CR-CAP-2 is checked against, and it links nothing: not EXT-08, not the
// N8RO SDK. `tests\build.cmd` builds and runs it, and it needs no N8RO install to pass.
//
// Five tiers, and the reason for five rather than one is that no single fixture can exercise
// everything the specification requires:
//
//   1. **The vendored 0.5.0 fixture, untouched.** `contract/` is read-only, so nothing here
//      writes into it. This tier is also the `contract/` drift check the PRD asks be re-run at
//      the start of every milestone (R4). It is the fixture that predates `sample_form`,
//      `limits` and `part`, and three of its checks assert those keys are reported ABSENT
//      rather than defaulted (§6.3a, §6.6) — which is why it was not replaced at the fifth pin.
//   1b. **The vendored 0.9.0 fixture**, added at the fifth pin. The same scenario, written by
//      the producer release the pin actually names, so the three keys above are exercised
//      PRESENT. Before it existed, that path ran only in tier 4 and therefore only on the
//      development machine: on a fresh clone nothing read a real 0.9.0 capture at all. Found by
//      the clean-room pair test (F-49), not by anything here.
//   2. **Mutations, generated at test time into `build/tests/mutations/`.** Copies, edited in
//      the build tree. Nothing mutated is committed, so no mutant can go stale against a
//      re-pinned fixture.
//   3. **Synthetic micro-captures**, written inline. Each isolates one rule no fixture can
//      reach — an empty segment, a rotated set, a non-finite double, a name re-used at a higher
//      occupancy. A capture small enough to read in the test that asserts on it is worth more
//      than one hidden in a file.
//   4. **The real 0.9.0 captures** under `campaigns/m2-oq1/`, when they are present. R4's own
//      mitigation says so in as many words: *"test against a capture written by the pinned
//      producer, not only against the vendored fixture"*. Tier 1b now covers that claim on
//      every machine; tier 4 remains what covers it across TWENTY runs rather than one, which
//      is where the roster-lifecycle invariants live. Skipped **with a printed message** when
//      the directory is absent, never silently.
//
// Never throws, like everything else here: a failure is a printed line and a non-zero exit.
#include "../src/capture/CaptureReader.h"
#include "../src/capture/CaptureSet.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ext17::capture;

namespace {

int g_failures = 0;
int g_checks = 0;

// Tier 4 runs only where the untracked 569 MB of real 0.9.0 captures happen to sit, so the
// number of checks this suite runs is a property of the MACHINE and not of the suite. That
// broke tests\build.cmd's check-count golden the first time this repository was built
// anywhere else: 475 on the development machine, 469 on a clean runner, zero failures both
// times. F-41 added that golden to stop a total drifting silently, and a golden that only
// holds on one machine is the same defect it was built to prevent.
//
// So the optional checks are counted separately and the golden pins the mandatory total.
// They still RUN, and a failure in one still fails the suite through g_failures - it is only
// the COUNT that is held apart.
int g_optionalChecks = 0;

void ok(const std::string& what, bool condition, const std::string& detail = {}) {
    ++g_checks;
    if (condition) {
        std::printf("  ok   %s\n", what.c_str());
    } else {
        ++g_failures;
        std::printf("  FAIL %s\n", what.c_str());
        if (!detail.empty()) std::printf("       %s\n", detail.c_str());
    }
}

long long countOf(const ReadResult& r, Code c) {
    auto it = r.diagnosticCounts.find(c);
    return it == r.diagnosticCounts.end() ? 0 : it->second;
}

std::string codesIn(const ReadResult& r) {
    std::string s;
    for (const auto& kv : r.diagnosticCounts) {
        if (!s.empty()) s += ", ";
        s += std::string(name(kv.first)) + " x" + std::to_string(kv.second);
    }
    return s.empty() ? "(none)" : s;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

const SegmentStats* segment(const ReadResult& r, long long part, long long ordinal) {
    for (const SegmentStats& s : r.segments) {
        if (s.key.part == part && s.key.segment == ordinal) return &s;
    }
    return nullptr;
}

// --- Tier 3's builder ---------------------------------------------------------------------
//
// One message type, five fields, chosen to cover every encoding rule in §8.3: a scalar double,
// a scalar int, a string, a bool, and a three-element double array.
const char* kSchemas =
    R"("schemas":[{"message_name":"m","topic":"t","schema_hash":7,"message_id":8,)"
    R"("wire_version":1,"fields":[{"name":"a","type":"double","size":1},)"
    R"({"name":"b","type":"int","size":1},{"name":"c","type":"string","size":1},)"
    R"({"name":"d","type":"bool","size":1},{"name":"v","type":"double","size":3}]}])";

struct Cap {
    std::vector<std::string> lines;
    long long segs = 0, samples = 0, adds = 0, removes = 0, verdicts = 0;

    void header(const std::string& extraKeys = {}) {
        std::string h = R"({"format_version":"n8ro-capture/1","type":"header",)"
                        R"("producer":{"name":"p","version":"9.9.9"},)"
                        R"("platform":{"engine_config":"e","model_path":"m","schema_file":"s",)"
                        R"("schema_version":"","runtime_version":"unknown"},)"
                        R"("attached_mid_run":false,)";
        if (!extraKeys.empty()) h += extraKeys + ",";
        h += R"("subscription":{"topic":"t","backpressure_policy":"FIFO_DROP","queue_size":8},)";
        h += kSchemas;
        h += "}";
        lines.push_back(h);
    }
    void open(long long seg, double t, const char* scenario = "S") {
        lines.push_back(R"({"type":"segment_open","sim_time_s":)" + std::to_string(t)
                        + R"(,"segment":)" + std::to_string(seg) + R"(,"scenario":")" + scenario + "\"}");
        ++segs;
    }
    void close(long long seg, double t, const char* reason = "shutdown") {
        lines.push_back(R"({"type":"segment_close","sim_time_s":)" + std::to_string(t)
                        + R"(,"segment":)" + std::to_string(seg)
                        + R"(,"scenario":"S","reason":")" + reason + "\"}");
    }
    void add(long long seg, double t, const char* e, long long occ) {
        lines.push_back(R"({"type":"entity_add","sim_time_s":)" + std::to_string(t)
                        + R"(,"segment":)" + std::to_string(seg) + R"(,"entity":")" + e
                        + R"(","occupancy":)" + std::to_string(occ) + "}");
        ++adds;
    }
    void remove(long long seg, double t, const char* e, long long occ,
                const char* reason = "destroyed") {
        lines.push_back(R"({"type":"entity_remove","sim_time_s":)" + std::to_string(t)
                        + R"(,"segment":)" + std::to_string(seg) + R"(,"entity":")" + e
                        + R"(","occupancy":)" + std::to_string(occ) + R"(,"reason":")" + reason + "\"}");
        ++removes;
    }
    void sample(long long seg, const std::string& t, const char* e, long long occ,
                const std::string& fields, const char* message = "m") {
        lines.push_back(R"({"type":"sample","sim_time_s":)" + t + R"(,"segment":)"
                        + std::to_string(seg) + R"(,"entity":")" + e + R"(","occupancy":)"
                        + std::to_string(occ) + R"(,"message":")" + message + R"(","fields":)"
                        + fields + "}");
        ++samples;
    }
    void raw(const std::string& line) { lines.push_back(line); }
    void trailer(const char* endReason = "shutdown", const std::string& extraKeys = {},
                 long long segsOverride = -1, long long samplesOverride = -1) {
        std::string t = R"({"type":"trailer","sim_time_s":0,"end_reason":")";
        t += endReason;
        t += R"(","counts":{"segments":)"
             + std::to_string(segsOverride < 0 ? segs : segsOverride) + R"(,"samples":)"
             + std::to_string(samplesOverride < 0 ? samples : samplesOverride)
             + R"(,"entities_added":)" + std::to_string(adds) + R"(,"entities_removed":)"
             + std::to_string(removes) + R"(,"verdicts":)" + std::to_string(verdicts) + "}";
        t += R"(,"drops":{"samples_not_recorded":0,"events_not_recorded":0,"samples_orphaned":0,)"
             R"("samples_unnamed":0,"samples_untimed":0})";
        t += R"(,"bus_metrics":{"schema_hash_drops":0,"message_id_drops":0,"decode_failures":0,)"
             R"("missing_schema_passthrough":0,"legacy_payload_passthrough":0})";
        if (!extraKeys.empty()) t += "," + extraKeys;
        t += "}";
        lines.push_back(t);
    }
};

// A complete `fields` object in the schema's declared order.
std::string fieldsAll(const char* a = "1.5", const char* b = "2", const char* c = "\"x\"",
                      const char* d = "true", const char* v = "[1,2,3]") {
    return std::string(R"({"a":)") + a + R"(,"b":)" + b + R"(,"c":)" + c + R"(,"d":)" + d
           + R"(,"v":)" + v + "}";
}

// --- Tier 1 --------------------------------------------------------------------------------

ReadResult g_fixture;

void tier1Fixture(const fs::path& root) {
    std::printf("\ntier 1 - the vendored fixture, untouched (contract/ is read-only)\n");
    const fs::path path = root / "contract" / "capture-atacama-air-defense-sample.n8rocap.jsonl";
    g_fixture = readFile(path.string());
    const ReadResult& r = g_fixture;

    ok("the fixture parses completely and conformantly", r.conformant(), codesIn(r));
    ok("format_version is n8ro-capture/1", r.header.formatVersion == kFormatVersion,
       r.header.formatVersion);
    ok("7180 lines read", r.tallies.lines == 7180, std::to_string(r.tallies.lines));

    // CR-CAP-2's second acceptance criterion, and the numbers the PRD states as re-derivable.
    ok("our tally agrees with trailer.counts",
       r.tallies.samples == r.trailer.counts.samples
           && r.tallies.entityAdds == r.trailer.counts.entitiesAdded
           && r.tallies.entityRemoves == r.trailer.counts.entitiesRemoved
           && r.tallies.verdicts == r.trailer.counts.verdicts
           && r.tallies.segmentsOpened == r.trailer.counts.segments,
       "ours " + std::to_string(r.tallies.samples) + "/" + std::to_string(r.tallies.entityAdds)
           + "/" + std::to_string(r.tallies.entityRemoves) + "/"
           + std::to_string(r.tallies.verdicts));
    ok("6945 samples, 132 adds, 90 removes, 7 verdicts, 2 segments",
       r.tallies.samples == 6945 && r.tallies.entityAdds == 132 && r.tallies.entityRemoves == 90
           && r.tallies.verdicts == 7 && r.tallies.segmentsOpened == 2,
       codesIn(r));

    // CR-CAP-4. One ordinary run is two segments; the second is the teardown reload.
    const SegmentStats* s0 = segment(r, 0, 0);
    const SegmentStats* s1 = segment(r, 0, 1);
    ok("two segments, both keyed on (part, segment)", s0 != nullptr && s1 != nullptr,
       std::to_string(r.segments.size()) + " segment(s)");
    if (s0 && s1) {
        ok("segment 0 is running by the format's exact test (max samples per key per sim_time_s is 1)",
           s0->clock == ClockClass::Running && s0->maxSamplesPerEntityPerSimTime == 1,
           std::string(name(s0->clock)) + ", max "
               + std::to_string(s0->maxSamplesPerEntityPerSimTime));
        ok("segment 1 is frozen by that same test, at 11",
           s1->clock == ClockClass::Frozen && s1->maxSamplesPerEntityPerSimTime == 11,
           std::string(name(s1->clock)) + ", max "
               + std::to_string(s1->maxSamplesPerEntityPerSimTime));
        // §5.1: a segment's extent is [first sample, last sample], never its boundary records —
        // both of which read 0.0 on a reloaded scenario. Computing it from the boundaries gives
        // zero for a run of any length, which is the error the format warns about first.
        ok("segment 0's extent comes from its samples, not its boundary records",
           s0->hasSamples && s0->lastSampleTimeS > 200.0,
           std::to_string(s0->firstSampleTimeS) + " .. " + std::to_string(s0->lastSampleTimeS));
        ok("the frozen segment carries 42 entity keys - the roster re-created at occupancy 2",
           s1->distinctEntityKeys == 42 && s1->entityAdds == 42,
           std::to_string(s1->distinctEntityKeys) + " keys, " + std::to_string(s1->entityAdds)
               + " adds");
    }

    // §6.3a / §6.6: this fixture is producer 0.5.0 and predates both keys. Absent means
    // *unknown*, and the reader must not substitute a value for either.
    ok("sample_form is absent and is reported as absent, not as \"predicted\"",
       !r.header.hasSampleForm && r.header.sampleForm.empty(), r.header.sampleForm);
    ok("limits is absent and is reported as absent, not as \"unbounded\"", !r.header.hasLimits);
    ok("part is absent and reads as 0", r.header.part == 0, std::to_string(r.header.part));
    ok("runtime_version \"unknown\" is carried, not treated as an error",
       r.header.runtimeVersion == "unknown", r.header.runtimeVersion);
    ok("schema_version is empty and that is legal", r.header.schemaVersion.empty());

    // §8.2 and §6.5. The declaration order is normative; `activeAnimation` is declared and, on
    // runtime 2.1.328, published by nobody. A reader that defaulted it would invent data.
    ok("one message type declared, with twelve fields in declaration order",
       r.header.schemas.size() == 1 && r.header.schemas[0].fields.size() == 12
           && r.header.schemas[0].fields.front().name == "simulationTime"
           && r.header.schemas[0].fields.back().name == "activeAnimation",
       std::to_string(r.header.schemas.size()) + " schema(s)");
}

// --- Tier 1b -------------------------------------------------------------------------------
//
// The SECOND vendored fixture, producer 0.9.0, added at the fifth pin (F-49).
//
// WHY A SECOND FIXTURE RATHER THAN A REPLACED ONE. Tier 1's fixture is producer 0.5.0 and
// three of its checks assert that `sample_form`, `limits` and `part` are *absent* and are
// reported as absent rather than defaulted - §6.3a and §6.6's rule that an absent key means
// unknown. Swapping the fixture for a 0.9.0 one would delete that coverage to buy this, which
// is a trade and not a fix. Both files are real captures of the same scenario written by two
// real producer releases, so the pair is exactly the compatibility question the format's §13
// exists for: the same reader, the same version, one file that predates three keys and one
// that carries them.
//
// WHAT IT CLOSES. Until the fifth pin the 0.9.0 keys were exercised only by tier 4, which
// reads the untracked 569 MB under campaigns/m2-oq1 and is skipped everywhere else - so on a
// FRESH CLONE nothing read a real 0.9.0 capture at all, and the suite said so in its own
// skip line. Found by the clean-room pair test, not by any check here. These are mandatory
// checks: they run on every machine, because their input is committed.
void tier1bFixture090(const fs::path& root) {
    std::printf("\ntier 1b - the vendored 0.9.0 fixture (the fifth pin; tier 1 is 0.5.0)\n");
    const fs::path path =
        root / "contract" / "capture-atacama-air-defense-sample-0.9.0.n8rocap.jsonl";
    const ReadResult r = readFile(path.string());

    ok("the 0.9.0 fixture parses completely and conformantly", r.conformant(), codesIn(r));
    ok("format_version is n8ro-capture/1 - a producer release is NOT a version change",
       r.header.formatVersion == kFormatVersion, r.header.formatVersion);
    ok("producer is 0.9.0", r.header.producerVersion == "0.9.0", r.header.producerVersion);
    ok("11150 lines read", r.tallies.lines == 11150, std::to_string(r.tallies.lines));
    ok("our tally agrees with trailer.counts",
       r.tallies.samples == r.trailer.counts.samples
           && r.tallies.entityAdds == r.trailer.counts.entitiesAdded
           && r.tallies.entityRemoves == r.trailer.counts.entitiesRemoved
           && r.tallies.verdicts == r.trailer.counts.verdicts
           && r.tallies.segmentsOpened == r.trailer.counts.segments,
       "ours " + std::to_string(r.tallies.samples) + "/" + std::to_string(r.tallies.entityAdds)
           + "/" + std::to_string(r.tallies.entityRemoves));
    ok("10915 samples, 132 adds, 90 removes, 7 verdicts, 2 segments",
       r.tallies.samples == 10915 && r.tallies.entityAdds == 132 && r.tallies.entityRemoves == 90
           && r.tallies.verdicts == 7 && r.tallies.segmentsOpened == 2,
       codesIn(r));

    // The three keys tier 1's fixture predates, here PRESENT and read - which is the whole
    // reason this fixture is committed. §6.3a: `published` and not `predicted`, stated by the
    // file rather than inferred by us.
    ok("sample_form is present and reads \"published\"",
       r.header.hasSampleForm && r.header.sampleForm == "published", r.header.sampleForm);
    ok("limits is present and reads as unbounded-with-stop, not as absent",
       r.header.hasLimits && r.header.maxBytes == 0 && r.header.maxSamples == 0
           && r.header.onSizeLimit == "stop",
       std::to_string(r.header.maxBytes) + "|" + std::to_string(r.header.maxSamples) + "|"
           + r.header.onSizeLimit);
    ok("part is present and reads 0", r.header.part == 0, std::to_string(r.header.part));

    // §14's host-dependent list, widened at this pin (E-7). An UNROTATED capture omits both
    // keys entirely, which is what makes `platform.model_path` the only exclusion that can
    // ever bite a capture this project produces - OQ-6 decided `stop`.
    ok("an unrotated capture carries neither continues_from nor continued_in",
       r.header.continuesFrom.empty() && r.trailer.continuedIn.empty(),
       "\"" + r.header.continuesFrom + "\" / \"" + r.trailer.continuedIn + "\"");

    // CR-CAP-4 again, on a different file: the two-segment shape is the platform's, not the
    // fixture's, and it survived a producer release and a regeneration.
    const SegmentStats* s0 = segment(r, 0, 0);
    const SegmentStats* s1 = segment(r, 0, 1);
    ok("two segments, both keyed on (part, segment)", s0 != nullptr && s1 != nullptr,
       std::to_string(r.segments.size()) + " segment(s)");
    if (s0 && s1) {
        ok("segment 0 is running by the format's exact test",
           s0->clock == ClockClass::Running && s0->maxSamplesPerEntityPerSimTime == 1,
           std::string(name(s0->clock)) + ", max "
               + std::to_string(s0->maxSamplesPerEntityPerSimTime));
        ok("segment 1 is not running, and is excluded either way",
           s1->clock != ClockClass::Running, name(s1->clock));
        ok("segment 0's extent comes from its samples, not its boundary records",
           s0->hasSamples && s0->lastSampleTimeS > 100.0,
           std::to_string(s0->firstSampleTimeS) + " .. " + std::to_string(s0->lastSampleTimeS));
    }
    ok("the same twelve fields in the same declaration order as the 0.5.0 fixture",
       r.header.schemas.size() == 1 && r.header.schemas[0].fields.size() == 12
           && r.header.schemas[0].fields.front().name == "simulationTime"
           && r.header.schemas[0].fields.back().name == "activeAnimation",
       std::to_string(r.header.schemas.size()) + " schema(s)");
}

// --- Tier 2 --------------------------------------------------------------------------------

std::vector<std::string> readAllLines(const fs::path& path) {
    std::vector<std::string> lines;
    std::ifstream in(path, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

bool writeAllLines(const fs::path& path, const std::vector<std::string>& lines) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    for (const std::string& l : lines) {
        out.write(l.data(), static_cast<std::streamsize>(l.size()));
        out.put('\n');   // LF, never CRLF (§2) - which is why the stream is binary
    }
    return true;
}

std::size_t findLine(const std::vector<std::string>& lines, const std::string& needle,
                     std::size_t from = 0) {
    for (std::size_t i = from; i < lines.size(); ++i) {
        if (contains(lines[i], needle)) return i;
    }
    return lines.size();
}

void tier2Mutations(const fs::path& root, const fs::path& outDir) {
    std::printf("\ntier 2 - mutations, generated into build/tests/mutations (never into contract/)\n");
    const fs::path fixture = root / "contract" / "capture-atacama-air-defense-sample.n8rocap.jsonl";
    std::vector<std::string> base = readAllLines(fixture);
    if (base.size() < 100) {
        ok("the fixture was readable for mutation", false,
           "only " + std::to_string(base.size()) + " lines");
        return;
    }
    std::error_code ec;
    fs::create_directories(outDir, ec);

    // --- (a) an unrecognised format_version, with the rest of the file booby-trapped --------
    //
    // CR-CAP-3 requires a named rejection and NO partial parse. Asserting "it rejected" is easy
    // and proves only half of that. So line 2 of this mutant is deliberate garbage: if the
    // reader ever reaches it, the test fails with a *different* error. "No partial parse" is
    // therefore proved by construction rather than asserted, and `tallies.lines == 1` says how
    // far it got.
    {
        std::vector<std::string> m = base;
        const std::size_t at = m[0].find("n8ro-capture/1");
        m[0].replace(at, 14, "n8ro-capture/2");
        m[1] = "{ this line is not JSON at all and never gets looked at";
        const fs::path p = outDir / "version-bumped.n8rocap.jsonl";
        writeAllLines(p, m);
        const ReadResult r = readFile(p.string());
        ok("(a) a bumped format_version is rejected", r.rejected, codesIn(r));
        ok("(a) the error is named unsupported_format_version",
           r.rejectCode == Code::UnsupportedFormatVersion, name(r.rejectCode));
        ok("(a) it names both the version found and the version supported",
           contains(r.rejectDetail, "n8ro-capture/2") && contains(r.rejectDetail, "n8ro-capture/1"),
           r.rejectDetail);
        ok("(a) NO PARTIAL PARSE: exactly one line was read", r.tallies.lines == 1,
           std::to_string(r.tallies.lines) + " lines");
        ok("(a) the booby-trapped line 2 was never parsed - no malformed_line was reported",
           r.diagnosticCounts.empty(), codesIn(r));
        ok("(a) nothing was produced: no segments, no counts", r.segments.empty()
               && r.tallies.samples == 0 && !r.hasTrailer);
    }

    // --- (b) truncation: the prefix stays usable -------------------------------------------
    {
        std::vector<std::string> m(base.begin(), base.end() - 40);
        const fs::path p = outDir / "truncated.n8rocap.jsonl";
        writeAllLines(p, m);
        const ReadResult r = readFile(p.string());
        ok("(b) a truncated file is not rejected - the prefix is still valid (§3)", !r.rejected);
        ok("(b) the error is named truncated_no_trailer", countOf(r, Code::TruncatedNoTrailer) == 1,
           codesIn(r));
        ok("(b) the records before the truncation point were read and counted",
           r.tallies.samples > 6000, std::to_string(r.tallies.samples) + " samples");
        ok("(b) no counts_disagree is invented: there is no trailer to disagree with",
           countOf(r, Code::CountsDisagree) == 0, codesIn(r));
    }

    // --- (c) a malformed line ---------------------------------------------------------------
    {
        std::vector<std::string> m = base;
        const std::size_t at = findLine(m, "\"type\":\"sample\"");
        m[at] = R"({"type":"sample","sim_time_s":,,,})";
        const fs::path p = outDir / "malformed-line.n8rocap.jsonl";
        writeAllLines(p, m);
        const ReadResult r = readFile(p.string());
        ok("(c) the error is named malformed_line", countOf(r, Code::MalformedLine) == 1, codesIn(r));
        ok("(c) it names the line it was found on",
           !r.diagnostics.empty() && r.diagnostics.front().line == at + 1,
           r.diagnostics.empty() ? "no diagnostics" : std::to_string(r.diagnostics.front().line));
        // The consequence is asserted rather than tolerated: one lost record means our tally no
        // longer matches the trailer, and the reader says both things rather than one.
        ok("(c) and the lost record makes our tally disagree with the trailer, which is reported",
           countOf(r, Code::CountsDisagree) == 1, codesIn(r));
        ok("(c) the rest of the file still parsed", r.tallies.samples == 6944,
           std::to_string(r.tallies.samples));
    }

    // --- (d) a fields key the schema does not declare ----------------------------------------
    {
        std::vector<std::string> m = base;
        const std::size_t at = findLine(m, "\"health\":");
        m[at].replace(m[at].find("\"health\":"), 9, "\"healthz\":");
        const fs::path p = outDir / "undeclared-field.n8rocap.jsonl";
        writeAllLines(p, m);
        const ReadResult r = readFile(p.string());
        ok("(d) the error is named undeclared_field", countOf(r, Code::UndeclaredField) == 1,
           codesIn(r));
        ok("(d) it is the only finding - a renamed key is not also an order or type error",
           r.diagnosticCounts.size() == 1, codesIn(r));
        ok("(d) it names the field and the message",
           !r.diagnostics.empty() && contains(r.diagnostics.front().detail, "healthz")
               && contains(r.diagnostics.front().detail, "simEntityStateUpdate"),
           r.diagnostics.empty() ? "" : r.diagnostics.front().detail);
    }

    // --- (e) a trailer count that disagrees with the records ---------------------------------
    {
        std::vector<std::string> m = base;
        std::string& t = m.back();
        t.replace(t.find("\"samples\":6945"), 14, "\"samples\":6944");
        const fs::path p = outDir / "count-mismatch.n8rocap.jsonl";
        writeAllLines(p, m);
        const ReadResult r = readFile(p.string());
        ok("(e) the error is named counts_disagree", countOf(r, Code::CountsDisagree) == 1,
           codesIn(r));
        ok("(e) it is the only finding", r.diagnosticCounts.size() == 1, codesIn(r));
        ok("(e) it names both numbers",
           !r.diagnostics.empty() && contains(r.diagnostics.front().detail, "samples=6944")
               && contains(r.diagnostics.front().detail, "samples=6945"),
           r.diagnostics.empty() ? "" : r.diagnostics.front().detail);
    }

    // --- (f) the positive mutation: keys from a producer that does not exist yet -------------
    //
    // §13's non-breaking rule, and R4's own mitigation: *"it is worth a test that feeds a header
    // carrying a key the reader has never heard of."* Every drift so far — 0.8.0's `sample_form`,
    // 0.9.0's four keys — was exactly this, and this is why the version has held across three
    // producer releases. Reject on the version; ignore unknown keys.
    {
        std::vector<std::string> m = base;
        m[0].insert(m[0].size() - 1, R"(,"unheard_of":{"nested":[1,2,3]},"another":"x")");
        const std::size_t at = findLine(m, "\"type\":\"sample\"");
        m[at].insert(m[at].size() - 1, R"(,"future_key":42)");
        const std::size_t so = findLine(m, "\"type\":\"segment_open\"");
        m[so].insert(m[so].size() - 1, R"(,"future_flag":true)");
        m.back().insert(m.back().size() - 1, R"(,"future_counter":99)");
        const fs::path p = outDir / "future-keys.n8rocap.jsonl";
        writeAllLines(p, m);
        const ReadResult r = readFile(p.string());
        ok("(f) unknown keys in known record types are IGNORED, not rejected", r.conformant(),
           codesIn(r));
        ok("(f) and everything else still reads identically",
           r.tallies.samples == 6945 && r.segments.size() == 2,
           std::to_string(r.tallies.samples));
    }

    std::printf("       mutants written to %s\n", outDir.string().c_str());
}

// --- Tier 3 --------------------------------------------------------------------------------

void tier3Synthetic() {
    std::printf("\ntier 3 - synthetic micro-captures, one rule each\n");

    // A capture may legitimately contain zero segments (§7).
    {
        Cap c;
        c.header();
        c.trailer();
        const ReadResult r = readLines(c.lines, "empty");
        ok("a header and a trailer is a valid, complete, empty capture", r.conformant(), codesIn(r));
        ok("it has no segments and says so", r.segments.empty());
    }

    // THE M2 BUG. A segment opened and closed with no sample in it is a real segment. A list
    // built from sample records alone loses it and then disagrees with trailer.counts.segments
    // for a reason that has nothing to do with the file. Measured in 15 of 20 runs at M2.
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.sample(0, "0.05", "E", 1, fieldsAll());
        c.close(0, 0.0, "scenario_unloaded");
        c.open(1, 0.0);
        c.close(1, 0.0, "host_lost");
        c.trailer("host_lost");
        const ReadResult r = readLines(c.lines, "empty-segment");
        ok("an empty segment is conformant", r.conformant(), codesIn(r));
        ok("the segment list is built from segment_open, so the empty segment is in it",
           r.segments.size() == 2, std::to_string(r.segments.size()) + " segment(s)");
        ok("and the segment count therefore agrees with the trailer",
           countOf(r, Code::CountsDisagree) == 0);
        const SegmentStats* s1 = segment(r, 0, 1);
        // Three-valued, not boolean. The frozen-clock test needs more than one sample for one
        // key at one sim_time_s before it can fire; with no samples it cannot. Calling this
        // segment "running" would assert the result of a test that never ran.
        ok("an empty segment is indeterminate - not running, and not frozen",
           s1 && s1->clock == ClockClass::Indeterminate,
           s1 ? name(s1->clock) : "missing");
    }

    // The frozen-clock test, firing. §5.1's exact test rather than a heuristic.
    {
        Cap c;
        c.header();
        c.open(0, 0.0);
        for (int i = 0; i < 3; ++i) c.sample(0, "0", "E", 2, fieldsAll());
        c.close(0, 0.0, "host_lost");
        c.trailer("host_lost");
        const ReadResult r = readLines(c.lines, "frozen");
        const SegmentStats* s = segment(r, 0, 0);
        ok("three samples for one key at one sim_time_s is a frozen segment",
           s && s->clock == ClockClass::Frozen && s->maxSamplesPerEntityPerSimTime == 3,
           s ? std::string(name(s->clock)) + " max "
                   + std::to_string(s->maxSamplesPerEntityPerSimTime) : "missing");
    }

    // The test counts per KEY, not per line: 42 entities sharing one sim_time_s is an ordinary
    // frame, not a frozen clock, and a reader that counted records per sim_time_s would call
    // every running segment frozen.
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.sample(0, "0.05", "A", 1, fieldsAll());
        c.sample(0, "0.05", "B", 1, fieldsAll());
        c.sample(0, "0.05", "C", 1, fieldsAll());
        c.sample(0, "0.1", "A", 1, fieldsAll());
        c.sample(0, "0.1", "B", 1, fieldsAll());
        c.close(0, 0.0);
        c.trailer();
        const ReadResult r = readLines(c.lines, "one-frame-many-entities");
        const SegmentStats* s = segment(r, 0, 0);
        ok("many entities at one sim_time_s is a running segment, not a frozen one",
           s && s->clock == ClockClass::Running && s->maxSamplesPerEntityPerSimTime == 1,
           s ? name(s->clock) : "missing");
        ok("distinct sim_time_s values are counted, not sample records",
           s && s->distinctSimTimes == 2,
           s ? std::to_string(s->distinctSimTimes) : "missing");
    }

    // §8.1: identity is (entity, occupancy). A name re-created at a higher occupancy is a
    // different thing that happens to share a label, and samples resuming under it are legal.
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.add(0, 0.05, "E", 1);
        c.sample(0, "0.05", "E", 1, fieldsAll());
        c.remove(0, 0.1, "E", 1, "destroyed");
        c.add(0, 0.15, "E", 2);
        c.sample(0, "0.15", "E", 2, fieldsAll());
        c.sample(0, "0.2", "E", 2, fieldsAll());
        c.close(0, 0.0);
        c.trailer();
        const ReadResult r = readLines(c.lines, "occupancy-reuse");
        ok("a name re-created at a higher occupancy is conformant", r.conformant(), codesIn(r));
        const SegmentStats* s = segment(r, 0, 0);
        ok("the two tenures of one name are two distinct entity keys",
           s && s->distinctEntityKeys == 2,
           s ? std::to_string(s->distinctEntityKeys) : "missing");
    }

    // The invariant §8.1 says is the one worth asserting: within one pair, no sample ever
    // appears after that pair's entity_remove.
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.add(0, 0.05, "E", 1);
        c.remove(0, 0.1, "E", 1, "expended");
        c.sample(0, "0.15", "E", 1, fieldsAll());
        c.close(0, 0.0);
        c.trailer();
        const ReadResult r = readLines(c.lines, "sample-after-remove");
        ok("a sample under a closed occupancy is named sample_after_remove",
           countOf(r, Code::SampleAfterRemove) == 1, codesIn(r));
    }

    // §9: `entity_remove.reason` is verbatim from the platform and the list is explicitly NOT
    // closed. A supplier-specific value the reader has never seen must be accepted.
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.add(0, 0.05, "E", 1);
        c.remove(0, 0.1, "E", 1, "swallowed_by_a_whale");
        c.close(0, 0.0);
        c.trailer();
        const ReadResult r = readLines(c.lines, "open-reason-vocabulary");
        ok("an unheard-of entity_remove reason is accepted - that vocabulary is open (§9)",
           r.conformant(), codesIn(r));
    }

    // §7 and §11, by contrast: these two vocabularies ARE closed, and changing one is a version
    // bump (§13). So an unexpected value in a version-matched file is a producer defect.
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.close(0, 0.0, "ran_out_of_ideas");
        c.trailer("gave_up");
        const ReadResult r = readLines(c.lines, "closed-vocabulary");
        ok("an unexpected segment_close.reason and end_reason are both named violations",
           countOf(r, Code::ClosedVocabularyViolation) == 2, codesIn(r));
    }

    // §8.3's encodings.
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        // `5` is a valid encoding of 5.0; the type comes from the schema, never from the token.
        c.sample(0, "0.05", "E", 1, fieldsAll("5", "0", R"("")", "false", "[0,1,2]"));
        // The three quoted tokens for non-finite doubles, accepted in a double field only.
        c.sample(0, "0.1", "E", 1, fieldsAll(R"("nan")", "1", R"("y")", "true", R"(["inf","-inf",0])"));
        c.close(0, 0.0);
        c.trailer();
        const ReadResult r = readLines(c.lines, "encodings");
        ok("an integral token in a double field, and nan/inf/-inf, are all accepted (§8.3)",
           r.conformant(), codesIn(r));
    }
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.sample(0, "0.05", "E", 1, fieldsAll("1.5", "2.5", R"("x")", "true", "[1,2,3]"));
        c.sample(0, "0.1", "E", 1, fieldsAll("1.5", "2", R"("x")", "true", R"("not-an-array")"));
        c.sample(0, "0.15", "E", 1, fieldsAll("1.5", "2", R"("x")", "true", "[1,2]"));
        c.sample(0, "0.2", "E", 1, fieldsAll(R"("banana")", "2", R"("x")", "true", "[1,2,3]"));
        c.close(0, 0.0);
        c.trailer();
        const ReadResult r = readLines(c.lines, "bad-encodings");
        ok("a fractional value in an int field is a named type mismatch",
           countOf(r, Code::FieldTypeMismatch) == 3, codesIn(r));
        ok("a short array is a named length mismatch, reported and tolerated (§8.3)",
           countOf(r, Code::ArrayLengthMismatch) == 1, codesIn(r));
        ok("a mismatch is reported, never fatal: all four samples were still counted",
           r.tallies.samples == 4, std::to_string(r.tallies.samples));
    }

    // §8.2's sparse rule. Declared-and-never-sent is normal; the fixture's own `activeAnimation`
    // is the worked example. Only a key the schema does not declare is a defect.
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.sample(0, "0.05", "E", 1, R"({"a":1,"c":"x"})");
        c.close(0, 0.0);
        c.trailer();
        const ReadResult r = readLines(c.lines, "sparse");
        ok("a declared field the publisher did not send is simply absent, not an error",
           r.conformant(), codesIn(r));
    }

    // §8.2's order rule, which is checkable only with an order-preserving parse.
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.sample(0, "0.05", "E", 1, R"({"c":"x","a":1})");
        c.close(0, 0.0);
        c.trailer();
        const ReadResult r = readLines(c.lines, "field-order");
        ok("fields out of the schema's declaration order is a named mismatch (§8.2)",
           countOf(r, Code::FieldOrderMismatch) == 1, codesIn(r));
    }

    // §4: the record vocabulary is closed within a version, so a ninth type in a version-matched
    // file is a producer defect — reported, and skipped rather than fatal (§3).
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.raw(R"({"type":"telemetry_hint","sim_time_s":0.05,"segment":0})");
        c.close(0, 0.0);
        c.trailer();
        const ReadResult r = readLines(c.lines, "unknown-record-type");
        ok("a ninth record type is named unknown_record_type, not ignored silently",
           countOf(r, Code::UnknownRecordType) == 1, codesIn(r));
        ok("and it is skipped rather than fatal - the trailer still read",
           r.hasTrailer && countOf(r, Code::CountsDisagree) == 0, codesIn(r));
    }

    // §7: no sample record appears outside an open segment.
    {
        Cap c;
        c.header();
        c.sample(0, "0.05", "E", 1, fieldsAll());
        c.trailer("shutdown", {}, 0);
        const ReadResult r = readLines(c.lines, "sample-outside-segment");
        ok("a sample with no segment open is named sample_outside_segment",
           countOf(r, Code::SampleOutsideSegment) == 1, codesIn(r));
    }

    // §5.1's positive rule, checked rather than assumed - because the distinct-time count is
    // derived from it.
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.sample(0, "0.2", "E", 1, fieldsAll());
        c.sample(0, "0.1", "E", 1, fieldsAll());
        c.close(0, 0.0);
        c.trailer();
        const ReadResult r = readLines(c.lines, "time-decreased");
        ok("a sample going backwards in sim_time_s within a segment is named",
           countOf(r, Code::SampleTimeDecreased) == 1, codesIn(r));
    }

    // §7: ordinals strictly increase and are never reused within a file.
    {
        Cap c;
        c.header();
        c.open(0, 0.05);
        c.close(0, 0.0);
        c.open(0, 0.0);
        c.close(0, 0.0);
        c.trailer();
        const ReadResult r = readLines(c.lines, "reused-ordinal");
        ok("a reused segment ordinal is named segment_ordinal_not_increasing",
           countOf(r, Code::SegmentOrdinalNotIncreasing) == 1, codesIn(r));
    }

    // §3 step 1: if the first line's type is not `header`, this is not a capture.
    {
        Cap c;
        c.open(0, 0.05);
        const ReadResult r = readLines(c.lines, "no-header");
        ok("a file whose first record is not a header is rejected as not_a_capture",
           r.rejected && r.rejectCode == Code::NotACapture, name(r.rejectCode));
    }
}

// --- Tier 3b: rotation ----------------------------------------------------------------------

void tier3Rotation(const fs::path& outDir) {
    std::printf("\ntier 3b - a rotated set: segment ordinals restart in every part (§6.7)\n");
    std::error_code ec;
    fs::create_directories(outDir, ec);

    // Part 0: a segment cut by the rotation - closed with `size_limit`, and a `continued_in`.
    Cap p0;
    p0.header(R"("limits":{"max_bytes":4096,"max_samples":0,"on_size_limit":"rotate"},"part":0)");
    p0.open(0, 0.05);
    p0.sample(0, "0.05", "E", 1, fieldsAll());
    p0.sample(0, "0.1", "E", 1, fieldsAll());
    p0.close(0, 0.1, "size_limit");
    p0.trailer("size_limit", R"("continued_in":"set.part001.n8rocap.jsonl")");

    // Part 1: the SAME segment of the run, continuing - and numbered 0 again, because ordinals
    // are unique within a file and restart in every part.
    Cap p1;
    p1.header(R"("limits":{"max_bytes":4096,"max_samples":0,"on_size_limit":"rotate"},)"
              R"("part":1,"continues_from":"set.n8rocap.jsonl")");
    p1.open(0, 0.15);
    p1.sample(0, "0.15", "E", 1, fieldsAll());
    p1.close(0, 0.0, "host_lost");
    p1.trailer("host_lost");

    writeAllLines(outDir / "set.n8rocap.jsonl", p0.lines);
    writeAllLines(outDir / "set.part001.n8rocap.jsonl", p1.lines);

    const SetResult set = readSet((outDir / "set.n8rocap.jsonl").string());
    ok("the set reads conformantly", set.conformant(), std::to_string(set.diagnosticTotal())
        + " finding(s)");
    ok("two parts were walked, following trailer.continued_in", set.parts.size() == 2,
       std::to_string(set.parts.size()));
    ok("both parts number their segment 0, and the set keys them (0,0) and (1,0)",
       set.segments.size() == 2 && set.segments[0].key.part == 0
           && set.segments[0].key.segment == 0 && set.segments[1].key.part == 1
           && set.segments[1].key.segment == 0,
       std::to_string(set.segments.size()) + " segment(s)");
    ok("the run's totals are the sum across parts - no part states them (§6.7)",
       set.counts.samples == 3 && set.counts.segments == 2,
       std::to_string(set.counts.samples) + " samples");
    // The one place §6.7's "the sum across parts" is not the run's total. One segment, cut once,
    // sums to two. Measured on a real four-part capture at M3 as five for a two-segment run, and
    // raised with EXT-08 as E-3 rather than worked around silently.
    ok("a segment cut by a rotation is counted in both parts, so the run has ONE segment "
       "where the sum says two",
       set.runSegments == 1 && set.segmentsCutByRotation == 1,
       "runSegments " + std::to_string(set.runSegments) + ", cuts "
           + std::to_string(set.segmentsCutByRotation));
    ok("each part alone is a complete, independently valid capture",
       set.parts[0].conformant() && set.parts[1].conformant(),
       codesIn(set.parts[0]) + " / " + codesIn(set.parts[1]));

    // An unrotated capture read as a set is a one-part set, which is the correct answer.
    {
        Cap solo;
        solo.header();
        solo.open(0, 0.05);
        solo.sample(0, "0.05", "E", 1, fieldsAll());
        solo.close(0, 0.0);
        solo.trailer();
        writeAllLines(outDir / "solo.n8rocap.jsonl", solo.lines);
        const SetResult s = readSet((outDir / "solo.n8rocap.jsonl").string());
        ok("a capture that never rotated reads as a one-part set", s.conformant()
               && s.parts.size() == 1, std::to_string(s.parts.size()) + " part(s)");
    }

    // A part that says the run continues but does not say it stopped at its bound would silently
    // lose the rest of a run.
    {
        Cap bad;
        bad.header(R"("part":0)");
        bad.open(0, 0.05);
        bad.close(0, 0.0, "host_lost");
        bad.trailer("host_lost", R"("continued_in":"nowhere.n8rocap.jsonl")");
        writeAllLines(outDir / "bad.n8rocap.jsonl", bad.lines);
        const SetResult s = readSet((outDir / "bad.n8rocap.jsonl").string());
        ok("a continued_in on a part whose end_reason is not size_limit is named part_link_broken",
           s.diagnosticCounts.count(Code::PartLinkBroken) == 1,
           std::to_string(s.diagnosticTotal()) + " finding(s)");
    }
}

// --- Tier 4 ---------------------------------------------------------------------------------

void tier4RealCaptures(const fs::path& root) {
    std::printf("\ntier 4 - the real producer-0.9.0 captures (R4: not only the 0.5.0 fixture)\n");
    const fs::path runs = root / "campaigns" / "m2-oq1" / "runs";
    std::error_code ec;
    if (!fs::is_directory(runs, ec)) {
        // Skipped out loud. A tier that disappears quietly is a tier nobody notices is gone.
        std::printf("  SKIP campaigns/m2-oq1/runs is not present (it is untracked, 569 MB).\n"
                    "       Regenerate with: n8ro-campaign repeat --count 2 --out-dir campaigns/m2-oq1\n");
        return;
    }
    std::vector<fs::path> captures;
    for (const fs::directory_entry& d : fs::directory_iterator(runs, ec)) {
        if (!d.is_directory(ec)) continue;
        for (const fs::directory_entry& f : fs::directory_iterator(d.path(), ec)) {
            const std::string n = f.path().filename().string();
            if (n.size() > 14 && n.rfind(".n8rocap.jsonl") == n.size() - 14) {
                captures.push_back(f.path());
            }
        }
    }
    std::sort(captures.begin(), captures.end());
    if (captures.empty()) {
        std::printf("  SKIP no captures under %s\n", runs.string().c_str());
        return;
    }

    long long conformant = 0, sawLimits = 0, sawSampleForm = 0;
    long long rosterMatches = 0, occupancySplit = 0, seg0Running = 0, seg1NotRunning = 0;
    for (const fs::path& p : captures) {
        const ReadResult r = readFile(p.string());
        if (r.conformant()) ++conformant;
        if (r.header.hasLimits) ++sawLimits;
        if (r.header.hasSampleForm && r.header.sampleForm == "published") ++sawSampleForm;
        if (r.tallies.entityAdds == 89 && r.tallies.entityRemoves == 47) ++rosterMatches;
        const SegmentStats* s0 = segment(r, 0, 0);
        const SegmentStats* s1 = segment(r, 0, 1);
        if (s0 && s1 && s0->entityAdds == 47 && s1->entityAdds == 42) ++occupancySplit;
        if (s0 && s0->clock == ClockClass::Running) ++seg0Running;
        if (s1 && s1->clock != ClockClass::Running) ++seg1NotRunning;
    }
    const long long n = static_cast<long long>(captures.size());
    ok("every capture written by the pinned producer is conformant", conformant == n,
       std::to_string(conformant) + " of " + std::to_string(n));
    ok("the 0.9.0 header keys the fixture predates are present and read",
       sawLimits == n && sawSampleForm == n,
       "limits " + std::to_string(sawLimits) + ", sample_form published "
           + std::to_string(sawSampleForm));
    // M2's twenty-run finding, re-derived by the real reader rather than by the throwaway
    // script: the roster lifecycle is identical in every run while the sample counts are not.
    ok("the roster lifecycle is identical in every run: 89 adds, 47 removes", rosterMatches == n,
       std::to_string(rosterMatches) + " of " + std::to_string(n));
    ok("and it splits 47 at occupancy 1 and 42 at occupancy 2, every time", occupancySplit == n,
       std::to_string(occupancySplit) + " of " + std::to_string(n));
    ok("segment 0 is running in every run by the format's exact test", seg0Running == n,
       std::to_string(seg0Running) + " of " + std::to_string(n));
    ok("segment 1 is never running - it is frozen or indeterminate, and is excluded either way",
       seg1NotRunning == n, std::to_string(seg1NotRunning) + " of " + std::to_string(n));
    std::printf("       %lld capture(s) read\n", n);
}

} // namespace

int main(int argc, char** argv) {
    const fs::path root = argc > 1 ? fs::path(argv[1]) : fs::path("..") / "..";
    const fs::path outDir = root / "build" / "tests" / "mutations";

    std::printf("capture_reader_test - the conformance suite for n8ro-capture/1\n");
    std::printf("repo root: %s\n", root.string().c_str());

    tier1Fixture(root);
    tier1bFixture090(root);
    tier2Mutations(root, outDir);
    tier3Synthetic();
    tier3Rotation(outDir);
    const int beforeTier4 = g_checks;
    tier4RealCaptures(root);
    g_optionalChecks = g_checks - beforeTier4;

    std::printf("\n%d check(s), %d failure(s)\n", g_checks, g_failures);
    // Read by tests\build.cmd, which subtracts it before comparing against the golden.
    std::printf("%d optional check(s) - tier 4, which runs only where the untracked "
                "0.9.0 captures are\n", g_optionalChecks);
    if (g_failures != 0) {
        std::printf("capture_reader_test: FAILED\n");
        return 1;
    }
    std::printf("capture_reader_test: all checks passed\n");
    return 0;
}
