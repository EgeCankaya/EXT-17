// EXT-17 — `n8ro-capture`, the conformant reader as a command.
//
// **This binary links nothing.** Not EXT-08, not the N8RO SDK, not a third-party JSON library.
// `build.cmd` beside this file names no include path and no library, and that is deliberate:
// CR-CAP-2's first acceptance criterion is a claim about a binary, and a build script anyone
// can read in ten seconds is a better proof of it than an argument about translation units.
//
// It is also the artifact behind CR-CAP-1's first half. It takes a *stored* capture and reports
// on it, having started no host and made no bus subscription — there is no code in this program
// that could. The other half, re-judging a stored run against a condition file, needs conditions
// to judge and arrives at M6.
//
// Never throws (constraint C3): every failure is a named error and an exit code.
#include "../../src/capture/CaptureReader.h"
#include "../../src/capture/CaptureSet.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using namespace ext17::capture;

const char* kHelp =
"n8ro-capture - EXT-17 conformant reader for n8ro-capture/1\n"
"\n"
"usage: n8ro-capture read <capture-file> [options]\n"
"       n8ro-capture read-set <first-part-file> [options]\n"
"       n8ro-capture campaign <campaign-dir> [options]\n"
"       n8ro-capture --help\n"
"\n"
"commands:\n"
"  read                     read one capture file and report on it.\n"
"  read-set                 read a rotated capture as the set it is, following\n"
"                           trailer.continued_in from the named first part. A capture\n"
"                           that was never rotated reads as a one-part set.\n"
"  campaign                 read every run's capture under <campaign-dir>/runs/NNN.\n"
"\n"
"options:\n"
"  --quiet                  one line per capture; omit the header, trailer and segment\n"
"                           detail. The verdict and the diagnostics are still printed.\n"
"  --no-verify-fields       skip checking each sample's fields against the header's\n"
"                           schema: undeclared keys, declaration order, encoding by\n"
"                           declared type, array lengths. Faster, and less of a check.\n"
"  --max-diagnostics <n>    keep at most n entries per diagnostic code. Default 8. The\n"
"                           counts are never capped, only the listed entries.\n"
"  --help                   this text.\n"
"\n"
"what it reports:\n"
"  Every statistic is scoped to a segment, and the segment it belongs to is named as the\n"
"  pair (part, segment) - ordinals restart at 0 in every part of a rotated set. A\n"
"  segment's clock is one of three values, never two: running (the format's exact test\n"
"  fired and the maximum samples any one (entity, occupancy) carries at a single\n"
"  sim_time_s is 1), frozen (that maximum is greater than 1, so the segment cannot be\n"
"  aligned against another run at all), or indeterminate (too few samples for the test\n"
"  to fire - which is what an empty teardown segment is, and calling it running would\n"
"  assert the result of a test that never ran).\n"
"\n"
"exit codes:\n"
"  0  every capture read is conformant\n"
"  1  a capture was read and something was found wrong with it\n"
"  2  a capture was rejected - unreadable, not a capture, or a format_version this\n"
"     reader does not implement - or a usage error\n"
"  4  an exception escaped, which nothing here should ever produce - rule 7 of the\n"
"     brief is \"Never throw\". It is a defect in the harness, reported as one and\n"
"     never as a result about anything this tool was asked to read\n";

struct Options {
    bool quiet = false;
    ReadOptions read;
};

std::string dbl(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.17g", v);
    return buf;
}

void printOptional(const char* label, const OptionalCount& c, bool& firstOnLine) {
    // Present-and-zero and absent are different claims (§11), and a report that prints both as
    // "0" is the report telling the reader something it does not know. Absent prints as "-".
    std::printf("%s%s %s", firstOnLine ? "" : "  ", label,
                c.present ? std::to_string(c.value).c_str() : "-");
    firstOnLine = false;
}

void reportSegments(const std::vector<SegmentStats>& segments) {
    if (segments.empty()) {
        std::printf("  segments      none - a capture with no segments is valid and complete (7)\n");
        return;
    }
    std::printf("  segments\n");
    for (const SegmentStats& s : segments) {
        std::printf("    (part %lld, segment %lld) %-14s %8lld samples  %5lld distinct sim_time_s",
                    s.key.part, s.key.segment, name(s.clock), s.samples, s.distinctSimTimes);
        if (s.hasSamples) {
            std::printf("  [%s .. %s]", dbl(s.firstSampleTimeS).c_str(),
                        dbl(s.lastSampleTimeS).c_str());
        } else {
            std::printf("  [no samples]");
        }
        std::printf("  max/key/t %lld  %lld entity keys  adds %lld  removes %lld  verdicts %lld\n",
                    s.maxSamplesPerEntityPerSimTime, s.distinctEntityKeys, s.entityAdds,
                    s.entityRemoves, s.verdicts);
        std::printf("        scenario \"%s\"  opened %s  closed %s%s%s\n",
                    s.scenario.c_str(), s.opened ? "yes" : "NO", s.closed ? "yes" : "NO",
                    s.closeReason.empty() ? "" : "  reason ", s.closeReason.c_str());
    }
}

void reportDiagnostics(const ReadResult& r) {
    for (const auto& kv : r.diagnosticCounts) {
        std::printf("  ! %-32s x%lld\n", name(kv.first), kv.second);
    }
    for (const Diagnostic& d : r.diagnostics) {
        std::printf("      line %-8zu %s: %s\n", d.line, name(d.code), d.detail.c_str());
    }
}

// Returns the process exit code this file contributes: 0 conformant, 1 defects, 2 rejected.
int reportFile(const ReadResult& r, const Options& opt) {
    std::printf("%s\n", r.path.c_str());
    if (r.rejected) {
        std::printf("  REJECTED      %s: %s\n", name(r.rejectCode), r.rejectDetail.c_str());
        std::printf("                %lld line(s) were read before the rejection.\n", r.tallies.lines);
        return 2;
    }

    if (!opt.quiet) {
        const Header& h = r.header;
        std::printf("  format        %s, producer %s %s\n", h.formatVersion.c_str(),
                    h.producerName.c_str(), h.producerVersion.c_str());
        std::printf("  platform      %s, runtime \"%s\", schema \"%s\" from %s\n",
                    h.engineConfig.c_str(), h.runtimeVersion.c_str(), h.schemaVersion.c_str(),
                    h.schemaFile.c_str());
        std::printf("  subscription  %s, %s, queue %lld\n", h.topic.c_str(),
                    h.backpressurePolicy.c_str(), h.queueSize);
        std::printf("  sample_form   %s\n",
                    h.hasSampleForm ? h.sampleForm.c_str()
                                    : "absent - unknown, and not \"predicted\" (6.3a)");
        if (h.hasLimits) {
            std::printf("  limits        max_bytes %lld, max_samples %lld, on_size_limit %s\n",
                        h.maxBytes, h.maxSamples, h.onSizeLimit.c_str());
        } else {
            std::printf("  limits        absent - unknown, and not \"unbounded\" (6.6)\n");
        }
        std::printf("  part          %lld%s%s\n", h.part,
                    h.hasContinuesFrom ? ", continues_from " : "",
                    h.hasContinuesFrom ? h.continuesFrom.c_str() : "");
        std::printf("  attached_mid_run %s\n", h.attachedMidRun ? "true" : "false");
        std::printf("  schemas       %zu message type(s)\n", h.schemas.size());

        if (r.hasTrailer) {
            std::printf("  trailer       end_reason %s, sim_time_s %s%s%s\n",
                        r.trailer.endReason.c_str(), dbl(r.trailer.simTimeS).c_str(),
                        r.trailer.hasContinuedIn ? ", continued_in " : "",
                        r.trailer.hasContinuedIn ? r.trailer.continuedIn.c_str() : "");
        } else {
            std::printf("  trailer       absent - the capture is truncated (3)\n");
        }
        std::printf("  counts        trailer segments %lld samples %lld adds %lld removes %lld "
                    "verdicts %lld\n",
                    r.trailer.counts.segments, r.trailer.counts.samples,
                    r.trailer.counts.entitiesAdded, r.trailer.counts.entitiesRemoved,
                    r.trailer.counts.verdicts);
        std::printf("                ours    segments %lld samples %lld adds %lld removes %lld "
                    "verdicts %lld\n",
                    r.tallies.segmentsOpened, r.tallies.samples, r.tallies.entityAdds,
                    r.tallies.entityRemoves, r.tallies.verdicts);

        bool first = true;
        std::printf("  drops         ");
        printOptional("samples_not_recorded", r.trailer.drops.samplesNotRecorded, first);
        printOptional("events_not_recorded", r.trailer.drops.eventsNotRecorded, first);
        printOptional("orphaned", r.trailer.drops.samplesOrphaned, first);
        printOptional("unnamed", r.trailer.drops.samplesUnnamed, first);
        printOptional("untimed", r.trailer.drops.samplesUntimed, first);
        std::printf("\n");

        first = true;
        std::printf("  bus_metrics   ");
        printOptional("schema_hash_drops", r.trailer.busMetrics.schemaHashDrops, first);
        printOptional("message_id_drops", r.trailer.busMetrics.messageIdDrops, first);
        printOptional("decode_failures", r.trailer.busMetrics.decodeFailures, first);
        printOptional("missing_schema", r.trailer.busMetrics.missingSchemaPassthrough, first);
        printOptional("legacy_payload", r.trailer.busMetrics.legacyPayloadPassthrough, first);
        std::printf("\n                ");
        first = true;
        printOptional("messages_dropped", r.trailer.busMetrics.messagesDropped, first);
        printOptional("backpressure", r.trailer.busMetrics.droppedByBackpressure, first);
        printOptional("queue_overflow", r.trailer.busMetrics.droppedByQueueOverflow, first);
        printOptional("rate_limiting", r.trailer.busMetrics.droppedByRateLimiting, first);
        std::printf("\n");
        // Every counter reading zero is not proof that nothing was lost (14, "Known loss"), and
        // this project measured 0.38% of samples missing across twenty runs with all nine
        // reading zero. A report that printed the zeros without the caveat would be inviting
        // exactly the conclusion tenet 3 forbids.
        std::printf("                all-zero counters are not proof that nothing was lost (14)\n");

        reportSegments(r.segments);
    }

    reportDiagnostics(r);
    if (r.conformant()) {
        std::printf("  CONFORMS      %lld lines, %lld bytes\n", r.tallies.lines, r.bytes);
        return 0;
    }
    std::printf("  DEFECTS       %lld finding(s) over %lld lines\n", r.diagnosticTotal(),
                r.tallies.lines);
    return 1;
}

int reportSet(const SetResult& set, const Options& opt) {
    int worst = 0;
    for (const ReadResult& p : set.parts) {
        worst = std::max(worst, reportFile(p, opt));
    }
    std::printf("set of %zu part(s)\n", set.parts.size());
    std::printf("  counts        summed across parts: segments %lld samples %lld adds %lld "
                "removes %lld verdicts %lld\n",
                set.counts.segments, set.counts.samples, set.counts.entitiesAdded,
                set.counts.entitiesRemoved, set.counts.verdicts);
    std::printf("                no single part states the run's totals (6.7)\n");
    // Both numbers, because they differ and the specification names only the first. A segment
    // cut by a rotation is closed in one part and opened in the next, so the sum counts it
    // twice. See Capture.h and escalation E-3.
    std::printf("  segments      %lld summed across parts, but the RUN has %lld: %lld segment(s) "
                "were cut by a rotation\n                and counted in two parts. The sum is "
                "what 6.7 says to compute; the run count is what is true.\n",
                set.counts.segments, set.runSegments, set.segmentsCutByRotation);
    if (!opt.quiet) reportSegments(set.segments);
    for (const auto& kv : set.diagnosticCounts) {
        std::printf("  ! %-32s x%lld\n", name(kv.first), kv.second);
    }
    for (const Diagnostic& d : set.diagnostics) {
        std::printf("      %s: %s\n", name(d.code), d.detail.c_str());
    }
    if (!set.diagnosticCounts.empty()) worst = std::max(worst, 1);
    return worst;
}

// Within one run directory the first part is the capture whose name carries no `.partNNN`.
// That is a naming convention rather than a format rule (2), so it is used only to *find* a
// starting point; everything about the set itself comes from the files' own link keys.
bool isPartFile(const std::string& name) {
    return name.find(".part") != std::string::npos;
}

int campaign(const std::string& dir, const Options& opt) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path runs = fs::path(dir) / "runs";
    if (!fs::is_directory(runs, ec)) {
        std::fprintf(stderr, "n8ro-capture: no runs directory under %s\n", dir.c_str());
        return 2;
    }
    std::vector<fs::path> runDirs;
    for (const fs::directory_entry& e : fs::directory_iterator(runs, ec)) {
        if (e.is_directory(ec)) runDirs.push_back(e.path());
    }
    std::sort(runDirs.begin(), runDirs.end());

    int worst = 0;
    long long captures = 0;
    long long conformant = 0;
    long long totalBytes = 0;
    for (const fs::path& run : runDirs) {
        std::vector<fs::path> firstParts;
        for (const fs::directory_entry& e : fs::directory_iterator(run, ec)) {
            const std::string name = e.path().filename().string();
            if (name.size() > 14 && name.rfind(".n8rocap.jsonl") == name.size() - 14
                && !isPartFile(name)) {
                firstParts.push_back(e.path());
            }
        }
        std::sort(firstParts.begin(), firstParts.end());
        for (const fs::path& p : firstParts) {
            const SetResult set = readSet(p.string(), SetOptions{opt.read});
            ++captures;
            for (const ReadResult& part : set.parts) totalBytes += part.bytes;
            const int rc = set.parts.size() == 1 ? reportFile(set.parts.front(), opt)
                                                 : reportSet(set, opt);
            if (rc == 0) ++conformant;
            worst = std::max(worst, rc);
        }
    }
    std::printf("campaign %s\n", dir.c_str());
    std::printf("  %lld capture(s), %lld conformant, %lld bytes\n", captures, conformant, totalBytes);
    if (captures == 0) {
        std::fprintf(stderr, "n8ro-capture: no captures found under %s\n", runs.string().c_str());
        return 2;
    }
    return worst;
}

} // namespace

int runMain(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "%s", kHelp);
        return 2;
    }
    const std::string command = argv[1];
    if (command == "--help" || command == "-h") {
        std::printf("%s", kHelp);
        return 0;
    }

    Options opt;
    std::string target;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--quiet") opt.quiet = true;
        else if (a == "--no-verify-fields") opt.read.verifyFields = false;
        else if (a == "--max-diagnostics") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "n8ro-capture: --max-diagnostics needs a value\n");
                return 2;
            }
            opt.read.maxDiagnosticsPerCode = static_cast<std::size_t>(std::strtoull(argv[++i], nullptr, 10));
        } else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "n8ro-capture: unrecognised option %s\n", a.c_str());
            return 2;
        } else if (target.empty()) {
            target = a;
        } else {
            std::fprintf(stderr, "n8ro-capture: one target at a time (%s)\n", a.c_str());
            return 2;
        }
    }
    if (target.empty()) {
        std::fprintf(stderr, "n8ro-capture: %s needs a target\n", command.c_str());
        return 2;
    }

    if (command == "read") return reportFile(readFile(target, opt.read), opt);
    if (command == "read-set") return reportSet(readSet(target, SetOptions{opt.read}), opt);
    if (command == "campaign") return campaign(target, opt);

    std::fprintf(stderr, "n8ro-capture: unrecognised command %s\n", command.c_str());
    return 2;
}


// --- [B]'s rule 7, enforced at the boundary --------------------------------------------------
//
// "Never throw." Nothing in this project throws: every failure is a return value plus a named
// error. This wrapper is what turns that from a habit into a property. Without it any exception
// the standard library can raise - std::bad_alloc on a hostile file, a filesystem_error from a
// directory that changes underneath a scan - reaches std::terminate, which prints nothing an
// operator can act on and returns an exit code nothing documents. Catching here converts the one
// thing this project promised would never happen into a named error and a documented exit code,
// so that even the unreachable case is reported rather than silent.
int main(int argc, char** argv) {
    try {
        return runMain(argc, argv);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s: an exception escaped - %s%s", "n8ro-capture", e.what(), "\n");
    } catch (...) {
        std::fprintf(stderr, "%s: a non-standard exception escaped%s", "n8ro-capture", "\n");
    }
    std::fprintf(stderr, "%s: this is a defect in the harness, not a result about anything it "
                         "was asked to read.%s", "n8ro-capture", "\n");
    return 4;
}
