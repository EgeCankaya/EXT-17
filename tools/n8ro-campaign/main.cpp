// EXT-17 — n8ro-campaign, the campaign runner's CLI.
//
// At M4 it exposes three commands: `run-once`, `repeat` and `self-test`. None judges anything —
// there are no conditions until M6 — so a run that reaches its stop predicate is `completed`, not
// `pass`. Collapsing those two would be the mistake tenet 2 exists to prevent, two milestones
// early.
//
// **`repeat` runs the determinism self-test before run 000, and stops the campaign if it does not
// pass.** That is CR-DET-1 ("not as a separate command someone remembers to run") and [B]'s step 4
// ("Do not build further until it passes") implemented as what they say. `self-test` exists as a
// command as well, for running the gate on its own; it is not how the campaign gets it.
//
// The option spelling is fixed by tools/n8ro-campaign/help.golden.txt, which build.cmd compares
// against this binary's own `--help` on every build. The PRD deliberately does not enumerate
// the options in prose: a list nobody executes is exactly what drifted upstream.
//
// Never throws (constraint C3).

#include "../../src/common/Log.h"
#include "../../src/proc/Process.h"
#include "../../src/run/RunOnce.h"
#include "../../src/run/RunRecord.h"
#include "../../src/run/SelfTest.h"
#include "../../src/run/StopPredicate.h"
#include "../../src/common/Json.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

using ext17::log::line;

void printHelp() {
    std::puts(
"n8ro-campaign - EXT-17 headless campaign runner (milestone 4: execution and the\n"
"                determinism gate; nothing is judged until milestone 6)\n"
"\n"
"usage: n8ro-campaign run-once  --out-dir <dir> [options]\n"
"       n8ro-campaign repeat    --out-dir <dir> --count <n> [options]\n"
"       n8ro-campaign self-test --out-dir <dir> [options]\n"
"       n8ro-campaign --help\n"
"\n"
"commands:\n"
"  run-once                 execute one run into <out-dir>/runs/<run-id>. One run is not a\n"
"                           campaign, so it does not self-test.\n"
"  repeat                   run the determinism self-test, then execute --count runs of one\n"
"                           configuration, continuing past a run that fails, and write a\n"
"                           campaign summary carrying the self-test's result.\n"
"  self-test                run the determinism self-test alone and stop. The campaign does\n"
"                           NOT get its gate from this command - repeat runs it itself, so\n"
"                           that it is never something anyone has to remember (CR-DET-1).\n"
"\n"
"the determinism self-test (CR-DET-1, and step 4 of the brief - a HARD GATE):\n"
"  Two runs of one configuration, executed into <out-dir>/selftest/runs/000 and 001 - which\n"
"  are NOT campaign runs and are never counted among the campaign's outcomes - and compared\n"
"  two ways. Both comparisons always run and both are always reported:\n"
"\n"
"    content   per (entity, occupancy) value sequences aligned on sim_time_s, over RUNNING\n"
"              segments only. A sample present in one run and absent from the other is NOT\n"
"              a difference: this platform publishes a slightly different subset of frames\n"
"              every run. It is counted and reported.\n"
"    bytes     byte for byte, with platform.model_path excluded - the one field the capture\n"
"              format names as legitimately host-dependent. EXPECTED TO FAIL here, and never\n"
"              engineered to pass.\n"
"\n"
"  Which of the two decides is OQ-2, out with the owner of the brief and UNANSWERED. A pass\n"
"  on the content basis does NOT discharge the brief's acceptance criterion 2 as written; it\n"
"  discharges it under the content reading, which is this project's own named deviation.\n"
"  A campaign whose self-test does not pass executes no campaign run at all.\n"
"\n"
"  --gate-basis <b>         content (default) or bytes. Selects which comparison decides.\n"
"  --coverage-floor <p>     whole percent of the smaller run's comparable samples that must\n"
"                           be present in BOTH, or the verdict is indeterminate rather than\n"
"                           pass - a comparison whose intersection collapsed would otherwise\n"
"                           report that everything it compared agreed. Default 99. Measured\n"
"                           over all 190 pairs of M2's twenty runs: worst 0.4192% unmatched.\n"
"\n"
"the run:\n"
"  --out-dir <dir>          campaign directory. Runs land in <dir>/runs/NNN. Required.\n"
"  --scenario <name>        scenario to load. Default \"Atacama Air Defense\".\n"
"  --model-name <name>      model the scenario lives in. Default N8roSimSchema.\n"
"  --model-path <dir>       schema and instance database. Default C:\\N8RO\\data\\db.\n"
"  --schema-file <name>     schema name inside that database. Default N8roSimSchema.\n"
"  --count <n>              repeat only: how many runs. Default 20.\n"
"  --first-run <n>          repeat only: ordinal of the first run. Default 0.\n"
"\n"
"the end of a run:\n"
"  --frames <n>             stop predicate: a run is finished when the engine's frame\n"
"                           number, as published on sim/engine/state, reaches n.\n"
"                           Default 1200. No wall-clock quantity participates.\n"
"  --run-timeout-ms <n>     backstop, never the definition of the end. A run that hits it\n"
"                           is reported as timeout - its own outcome, neither pass nor\n"
"                           fail. There is no way to run unbounded. Default 600000.\n"
"\n"
"bring-up timeouts (each bounds one observed condition; none is a delay):\n"
"  --ready-timeout-ms <n>   host publishes engine state. Default 60000.\n"
"  --load-timeout-ms <n>    scenario reports loaded. Default 120000.\n"
"  --start-timeout-ms <n>   engine reports running. Default 30000.\n"
"  --stop-timeout-ms <n>    engine leaves running after stop. Default 30000.\n"
"  --host-exit-timeout-ms <n>      host exits after being asked. Default 20000.\n"
"  --recorder-exit-timeout-ms <n>  recorder closes its capture after host loss, which it\n"
"                           detects after 3.0 s of engine-state silence. Default 30000.\n"
"\n"
"processes:\n"
"  --host-exe <path>        headless host. Default C:\\N8RO\\bin\\n8ro-sim-app.exe.\n"
"  --recorder <path>        capture recorder, driven as a process. Required unless\n"
"                           --no-recorder.\n"
"  --no-recorder            run without recording. The run still executes and is still\n"
"                           recorded in run.json; there is simply no capture.\n"
"  --sim-config-host <e>    host-side config entry. Default SimEngineHost_SharedMemory.\n"
"  --sim-config-client <e>  client-side config entry. Default SimEngineClient_SharedMemory.\n"
"  --capture-max-bytes <n>  per-capture byte bound, passed to the recorder. Default:\n"
"                           61000 x --frames, which is three times the measured\n"
"                           per-frame capture cost - a bound that does not fire on\n"
"                           a normal run and does fire on a runaway. Pass 0 for\n"
"                           unbounded. A campaign-level ceiling is not this; that\n"
"                           is --disk-ceiling-bytes below.\n"
"  --on-size-limit <a>      stop (default, and OQ-6's decision) or rotate, on reaching\n"
"                           --capture-max-bytes. rotate keeps the run's tail and makes\n"
"                           the capture a SET of files whose segment ordinals restart\n"
"                           in every part - measured at M3 to turn one run's two\n"
"                           segments into five (part, segment) keys. stop keeps one\n"
"                           file per run and loses the tail, and says so in the file\n"
"                           and in run.json's capture.covers_whole_run.\n"
"\n"
"disk (CR-CAP-5; the upstream bound above is per file, this one is per campaign):\n"
"  --disk-ceiling-bytes <n> ceiling over the WHOLE campaign directory, captures and logs\n"
"                           together. Default 8589934592 (8 GiB). Checked against free\n"
"                           space before run 1 and against actual usage after every run;\n"
"                           reaching it stops the campaign with a named outcome and\n"
"                           leaves every completed run valid. 0 disables the ceiling.\n"
"  --bytes-per-frame <n>    the projection used by the pre-flight check. Default 25400,\n"
"                           measured over M2's twenty runs: 24.3 MB of capture plus\n"
"                           5.4 MB of host log per 1200-frame run.\n"
"\n"
"environment (both are measured preconditions, not preferences):\n"
"  --n8ro-release <dir>     N8RO_RELEASE for the child processes. Default C:\\N8RO.\n"
"                           Without it the host skips its plugin scan, never registers\n"
"                           componentPhysics, and refuses every 42-entity scenario load\n"
"                           while sitting idle rather than failing.\n"
"  --path-prepend <dir>     prepended to PATH for the child processes, and needed by this\n"
"                           binary too. Default C:\\N8RO\\bin. Without it an SDK-linked\n"
"                           binary does not load at all.\n"
"\n"
"other:\n"
"  --query-catalogue        query the scenario catalogue over the bus during bring-up and\n"
"                           log what it answers.\n"
"  --queue-size <n>         recorder handler-to-writer queue bound, in records. Omitted\n"
"                           leaves the recorder's own default in place. A small value\n"
"                           deliberately overloads it so that a capture's\n"
"                           trailer.drops.samples_not_recorded becomes non-zero - which is\n"
"                           the only way to exercise CR-DET-1's rule that such a capture is\n"
"                           EXCLUDED from the comparison rather than diffed. Nothing this\n"
"                           project has run has ever dropped a sample at the default.\n"
"  --help                   this text.\n"
"\n"
"exit codes:\n"
"  0  every run completed - the stop predicate was satisfied and teardown was clean\n"
"  1  at least one run did not complete (timeout or infrastructure error), or the\n"
"     campaign stopped at its disk ceiling\n"
"  2  usage or configuration error; no run was attempted\n"
"  3  the determinism self-test did not pass, and NO campaign run was attempted. Step 4 of\n"
"     the brief: \"Do not build further until it passes.\" Whether the gate FAILED or could\n"
"     not be established at all - a self-test run that did not complete has established\n"
"     nothing about determinism and is not a determinism failure - is named in\n"
"     <out-dir>/selftest/self-test.json under \"outcome\"");
}

struct Args {
    std::string command;
    ext17::run::RunConfig run;
    std::string outDir;
    unsigned long long frames = 1200;
    int count = 20;
    int firstRun = 0;
    // CR-CAP-5. Measured at M3 over M2's twenty runs: a 1200-frame run costs 29 MB of campaign
    // directory - a 24.3 MB capture, a 2.85 MB slice of the host's log and a 2.56 MB host.err,
    // the last two being the terrain-error flood the install is expected to produce. That is
    // ~24.8 KB per frame, and the ceiling is over the whole directory rather than over the
    // captures because the logs are exactly what a capture-only projection leaves out.
    bool captureBoundGiven = false;
    unsigned long long diskCeilingBytes = 8ULL * 1024 * 1024 * 1024;   // 8 GiB
    unsigned long long bytesPerFrame = 25400;
    // The per-capture bound is derived from --frames unless it is given explicitly, so that
    // CR-CAP-5's "each run is given a per-capture byte bound" holds by default rather than only
    // when somebody remembers. `captureBoundGiven` distinguishes "not given" from an explicit 0,
    // which means unbounded and is a different instruction.
    static constexpr unsigned long long kCaptureBytesPerFrame = 61000;

    // CR-DET-1 / OQ-2. `content` is ADR-1's decision and this project's, not the client's;
    // `bytes` is [B]'s strictest reading, which cannot pass on this platform and correctly stops
    // the campaign when selected. Both comparisons always run and both are always reported — the
    // basis chooses which one decides. A ruling on OQ-2 changes this default and nothing else.
    ext17::compare::CompareOptions compare;

    // Passed through to the recorder. It exists here so that CR-DET-1's
    // `samples_not_recorded` exclusion can be exercised on a real capture rather than only
    // asserted: at the default of 8192 nothing this project has run has ever dropped a sample.
    bool queueSizeGiven = false;
    long long queueSize = 0;

    bool help = false;
};

bool parseArgs(int argc, char** argv, Args& a, std::string& error) {
    if (argc < 2) {
        a.help = true;
        return true;
    }
    int i = 1;
    if (argv[1][0] != '-') {
        a.command = argv[1];
        i = 2;
    }
    for (; i < argc; ++i) {
        const std::string opt = argv[i];
        const auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                error = std::string(name) + " needs a value";
                return nullptr;
            }
            return argv[++i];
        };
        const auto asInt = [&](const char* name, int& target) {
            const char* v = next(name);
            if (!v) { return false; }
            target = std::atoi(v);
            return true;
        };

        if (opt == "--help" || opt == "-h") { a.help = true; }
        else if (opt == "--out-dir")        { const char* v = next("--out-dir"); if (!v) return false; a.outDir = v; }
        else if (opt == "--scenario")       { const char* v = next("--scenario"); if (!v) return false; a.run.scenario = v; }
        else if (opt == "--model-name")     { const char* v = next("--model-name"); if (!v) return false; a.run.modelName = v; }
        else if (opt == "--model-path")     { const char* v = next("--model-path"); if (!v) return false; a.run.modelPath = v; }
        else if (opt == "--schema-file")    { const char* v = next("--schema-file"); if (!v) return false; a.run.schemaFile = v; }
        else if (opt == "--frames")         { const char* v = next("--frames"); if (!v) return false; a.frames = std::strtoull(v, nullptr, 10); }
        else if (opt == "--count")          { if (!asInt("--count", a.count)) return false; }
        else if (opt == "--first-run")      { if (!asInt("--first-run", a.firstRun)) return false; }
        else if (opt == "--run-timeout-ms") { if (!asInt("--run-timeout-ms", a.run.runTimeoutMs)) return false; }
        else if (opt == "--ready-timeout-ms") { if (!asInt("--ready-timeout-ms", a.run.readyTimeoutMs)) return false; }
        else if (opt == "--load-timeout-ms")  { if (!asInt("--load-timeout-ms", a.run.loadTimeoutMs)) return false; }
        else if (opt == "--start-timeout-ms") { if (!asInt("--start-timeout-ms", a.run.startTimeoutMs)) return false; }
        else if (opt == "--stop-timeout-ms")  { if (!asInt("--stop-timeout-ms", a.run.stopTimeoutMs)) return false; }
        else if (opt == "--host-exit-timeout-ms")     { if (!asInt("--host-exit-timeout-ms", a.run.hostExitTimeoutMs)) return false; }
        else if (opt == "--recorder-exit-timeout-ms") { if (!asInt("--recorder-exit-timeout-ms", a.run.recorderExitTimeoutMs)) return false; }
        else if (opt == "--host-exe")       { const char* v = next("--host-exe"); if (!v) return false; a.run.hostExe = v; }
        else if (opt == "--recorder")       { const char* v = next("--recorder"); if (!v) return false; a.run.recorderExe = v; }
        else if (opt == "--no-recorder")    { a.run.attachRecorder = false; }
        else if (opt == "--sim-config-host")   { const char* v = next("--sim-config-host"); if (!v) return false; a.run.simConfigHost = v; }
        else if (opt == "--sim-config-client") { const char* v = next("--sim-config-client"); if (!v) return false; a.run.simConfigClient = v; }
        else if (opt == "--capture-max-bytes") { const char* v = next("--capture-max-bytes"); if (!v) return false; a.run.captureMaxBytes = std::strtoull(v, nullptr, 10); a.captureBoundGiven = true; }
        else if (opt == "--on-size-limit")     { const char* v = next("--on-size-limit"); if (!v) return false; a.run.onSizeLimit = v; }
        else if (opt == "--disk-ceiling-bytes") { const char* v = next("--disk-ceiling-bytes"); if (!v) return false; a.diskCeilingBytes = std::strtoull(v, nullptr, 10); }
        else if (opt == "--bytes-per-frame")   { const char* v = next("--bytes-per-frame"); if (!v) return false; a.bytesPerFrame = std::strtoull(v, nullptr, 10); }
        else if (opt == "--n8ro-release")      { const char* v = next("--n8ro-release"); if (!v) return false; a.run.n8roRelease = v; }
        else if (opt == "--path-prepend")      { const char* v = next("--path-prepend"); if (!v) return false; a.run.pathPrepend = v; }
        else if (opt == "--query-catalogue")   { a.run.queryCatalogue = true; }
        else if (opt == "--queue-size")        { const char* v = next("--queue-size"); if (!v) return false; a.queueSize = std::atoll(v); a.queueSizeGiven = true; }
        else if (opt == "--gate-basis") {
            const char* v = next("--gate-basis");
            if (!v) return false;
            if (!ext17::compare::parseGateBasis(v, a.compare.gateBasis)) {
                error = "--gate-basis must be content or bytes";
                return false;
            }
        }
        else if (opt == "--coverage-floor") {
            const char* v = next("--coverage-floor");
            if (!v) return false;
            // A whole percentage, read digit by digit. A floating-point command-line value would
            // put a locale-dependent conversion on the path that decides the gate, which is the
            // hazard CR-DET-2 exists to keep off it.
            long long pct = 0;
            for (const char* c = v; *c; ++c) {
                if (*c < '0' || *c > '9') { error = "--coverage-floor must be a whole percentage, 0 to 100"; return false; }
                pct = pct * 10 + (*c - '0');
            }
            if (pct > 100) { error = "--coverage-floor must be 0 to 100"; return false; }
            a.compare.coverageFloor = static_cast<double>(pct) / 100.0;
        }
        else {
            error = "unrecognised option " + opt;
            return false;
        }
    }
    return true;
}

std::string ordinal(int n) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%03d", n);
    return buf;
}

std::string joinPath(const std::string& dir, const std::string& leaf) {
    if (dir.empty()) { return leaf; }
    const char last = dir.back();
    return (last == '\\' || last == '/') ? dir + leaf : dir + "\\" + leaf;
}

// The campaign summary. At M2 it counts three outcomes, and the invariant it asserts is the
// one CR-EX-4 names: every attempted run lands in exactly one category, and the categories sum.
void writeCampaignSummary(const std::string& path,
                          const std::vector<ext17::run::RunRecord>& records,
                          const ext17::run::StopPredicate& predicate,
                          const ext17::run::SelfTestResult* selfTest) {
    int completed = 0, timedOut = 0, infra = 0;
    for (const auto& r : records) {
        switch (r.outcome) {
            case ext17::run::RunOutcome::Completed:           ++completed; break;
            case ext17::run::RunOutcome::Timeout:             ++timedOut;  break;
            case ext17::run::RunOutcome::InfrastructureError: ++infra;     break;
        }
    }

    ext17::json::Writer w;
    w.beginObject();
    w.member("schema", std::string("ext17-campaign-summary/2"));
    w.member("milestone", std::string("M4 - execution and the determinism gate; no conditions are "
                                      "evaluated, so no run is a pass or a fail yet"));

    // CR-DET-1: "the self-test runs at the start of every campaign and its result appears in the
    // report". At M4 this file is the report. M6 replaces it and reads this object rather than
    // re-deriving it, which is why it is a whole nested document and not three flattened keys.
    if (selfTest != nullptr) {
        w.beginObject("self_test");
        selfTest->writeMembers(w);
        w.endObject();
    } else {
        w.memberNull("self_test");
    }
    w.beginObject("stop_predicate");
    w.member("kind", std::string(predicate.kindName()));
    w.member("statement", predicate.statement());
    w.endObject();

    w.beginObject("outcomes");
    w.member("attempted", static_cast<std::int64_t>(records.size()));
    w.member("completed", static_cast<std::int64_t>(completed));
    w.member("timeout", static_cast<std::int64_t>(timedOut));
    w.member("infrastructure_error", static_cast<std::int64_t>(infra));
    w.member("sums_to_attempted",
             completed + timedOut + infra == static_cast<int>(records.size()));
    w.endObject();

    w.beginArray("runs");
    for (const auto& r : records) {
        w.beginObject();
        w.member("run_id", r.runId);
        w.member("outcome", std::string(ext17::run::toString(r.outcome)));
        w.member("predicate_satisfied", r.predicateSatisfied);
        w.member("observed_frame", r.evaluation.observedFrame);
        w.member("observed_sim_time_s", r.evaluation.observedSimTimeS, 6);
        w.member("observed_delta_s", r.evaluation.observedDeltaS, 5);
        if (r.captureBytes) { w.member("capture_bytes", *r.captureBytes); }
        else                { w.memberNull("capture_bytes"); }
        w.endObject();
    }
    w.endArray();
    w.endObject();

    const std::string text = w.str() + "\n";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        line("campaign", "could not write the campaign summary to " + path);
        return;
    }
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
    line("campaign", "summary -> " + path);
}

} // namespace

int main(int argc, char** argv) {
    Args a;
    std::string error;
    if (!parseArgs(argc, argv, a, error)) {
        std::fprintf(stderr, "n8ro-campaign: %s\n", error.c_str());
        std::fprintf(stderr, "Try 'n8ro-campaign --help'.\n");
        return 2;
    }
    if (a.help) {
        printHelp();
        return 0;
    }
    if (a.command != "run-once" && a.command != "repeat") {
        std::fprintf(stderr, "n8ro-campaign: expected a command, run-once or repeat\n");
        return 2;
    }
    if (a.outDir.empty()) {
        std::fprintf(stderr, "n8ro-campaign: --out-dir is required\n");
        return 2;
    }
    if (a.run.attachRecorder && a.run.recorderExe.empty()) {
        std::fprintf(stderr, "n8ro-campaign: --recorder <path> is required unless --no-recorder\n");
        return 2;
    }
    if (a.run.onSizeLimit != "stop" && a.run.onSizeLimit != "rotate") {
        std::fprintf(stderr, "n8ro-campaign: --on-size-limit must be stop or rotate\n");
        return 2;
    }
    // A campaign whose runs record nothing cannot be self-tested, because there is no capture to
    // compare — and it could not be judged at M6 either. Refusing here is the same rule M3
    // arrived at from the other end: a run asked to record that produced no capture is an
    // infrastructure error, not a completed run.
    if ((a.command == "repeat" || a.command == "self-test") && !a.run.attachRecorder) {
        std::fprintf(stderr, "n8ro-campaign: --no-recorder cannot be used with %s. The "
                             "determinism self-test (CR-DET-1) compares two captures, and there "
                             "would be none. Use run-once to execute without recording.\n",
                     a.command.c_str());
        return 2;
    }
    if (a.command == "repeat" && a.count < 1) {
        std::fprintf(stderr, "n8ro-campaign: --count must be at least 1\n");
        return 2;
    }
    // CR-EX-4: "there is no configuration in which a run may run unbounded". A non-positive
    // timeout is already bounded - the deadline is in the past, so the run times out at once -
    // but rejecting it makes the requirement checkable at the CLI rather than by reasoning
    // about what a negative deadline does.
    if (a.run.runTimeoutMs < 1) {
        std::fprintf(stderr, "n8ro-campaign: --run-timeout-ms must be at least 1; "
                             "there is no unbounded run\n");
        return 2;
    }
    // A budget of zero is satisfied by the engine's own starting frame, so the run would stop
    // before it had run. That is a configuration error, not a very short run.
    if (a.frames < 1) {
        std::fprintf(stderr, "n8ro-campaign: --frames must be at least 1\n");
        return 2;
    }

    if (!ext17::proc::createDirectories(a.outDir)) {
        std::fprintf(stderr, "n8ro-campaign: could not create --out-dir %s\n", a.outDir.c_str());
        return 2;
    }
    // Absolute, before anything is handed to a child. Every child runs in its own working
    // directory, so a relative path means something different there than it did here. Measured
    // at M3: a probe run given a relative --out-dir passed it to the recorder, which resolved it
    // against the run directory it had just been placed in, found nothing, and refused - and the
    // run then executed all the way to its stop predicate having recorded nothing at all.
    a.outDir = ext17::proc::absolutePath(a.outDir);
    ext17::log::mirrorToFile(joinPath(a.outDir, "campaign.log"));

    // ---- CR-CAP-5: the pre-flight check ---------------------------------------------------
    // The upstream recorder bounds one capture FILE (format 6.6). A campaign is many of them
    // plus a host log per run, so a per-file bound multiplied by twenty is not a campaign bound
    // and this check is still this project's own. It names both numbers, because "not enough
    // space" without them is a message nobody can act on.
    // The self-test's two runs cost disk like any other run and are counted in the projection,
    // because a pre-flight check that ignored them would be projecting a campaign nobody runs.
    const bool selfTests = (a.command == "repeat" || a.command == "self-test");
    const int campaignRuns = (a.command == "run-once") ? 1 : (a.command == "self-test" ? 0 : a.count);
    const int plannedRuns = campaignRuns + (selfTests ? 2 : 0);
    const unsigned long long projected =
        static_cast<unsigned long long>(plannedRuns) * a.frames * a.bytesPerFrame;
    line("campaign", "disk: projecting " + std::to_string(projected) + " bytes for "
                         + std::to_string(plannedRuns) + " run(s) of " + std::to_string(a.frames)
                         + " frames at " + std::to_string(a.bytesPerFrame) + " bytes/frame"
                         + (selfTests ? " (including the self-test's 2 runs)" : ""));
    if (a.diskCeilingBytes > 0 && projected > a.diskCeilingBytes) {
        std::fprintf(stderr, "n8ro-campaign: the projected footprint of %llu bytes exceeds the "
                             "campaign ceiling of %llu bytes. Raise --disk-ceiling-bytes "
                             "deliberately, or run fewer or shorter runs.\n",
                     projected, a.diskCeilingBytes);
        return 2;
    }
    if (const auto freeBytes = ext17::proc::freeSpaceBytes(a.outDir)) {
        line("campaign", "disk: " + std::to_string(*freeBytes) + " bytes free, ceiling "
                             + (a.diskCeilingBytes > 0 ? std::to_string(a.diskCeilingBytes)
                                                       : std::string("none")));
        if (projected > *freeBytes) {
            std::fprintf(stderr, "n8ro-campaign: the projected footprint of %llu bytes exceeds "
                                 "the %llu bytes free on the volume holding %s. Refusing to "
                                 "start.\n", projected, *freeBytes, a.outDir.c_str());
            return 2;
        }
    } else {
        // Absence is not evidence (tenet 3). A volume that cannot be queried is reported as
        // unqueried rather than quietly treated as having room.
        line("campaign", "disk: free space could not be queried for " + a.outDir
                             + "; the pre-flight space check did not run");
    }

    if (!a.captureBoundGiven) {
        a.run.captureMaxBytes = a.frames * Args::kCaptureBytesPerFrame;
        line("campaign", "per-capture bound: " + std::to_string(a.run.captureMaxBytes)
                             + " bytes, derived from " + std::to_string(a.frames)
                             + " frames. It is three times the measured per-frame capture cost, "
                               "so a run that reaches it has overrun its projection rather than "
                               "merely run.");
    }
    a.run.predicate = ext17::run::StopPredicate::frameBudget(a.frames);
    line("campaign", "stop predicate: " + a.run.predicate.statement());
    line("campaign", "run timeout: " + std::to_string(a.run.runTimeoutMs)
                         + " ms (backstop; never the definition of the end)");

    if (a.queueSizeGiven) {
        a.run.recorderQueueSize = a.queueSize;
        line("campaign", "recorder --queue-size " + std::to_string(a.queueSize)
                             + ". A small value deliberately overloads the recorder's "
                               "handler-to-writer queue, which is how a capture with a non-zero "
                               "trailer.drops.samples_not_recorded is produced. Such a capture is "
                               "EXCLUDED from the determinism comparison rather than diffed "
                               "(CR-DET-1).");
    }

    // ---- CR-DET-1: the determinism self-test, at the START of the campaign ------------------
    // [B] step 4: "Do not build further until it passes." It is not a command anybody has to
    // remember; a campaign runs it before its first run, and a campaign whose gate does not pass
    // executes no campaign run at all.
    ext17::run::SelfTestResult selfTest;
    bool selfTestRan = false;
    if (selfTests) {
        ext17::run::SelfTestConfig stc;
        stc.run = a.run;
        stc.selfTestDir = joinPath(a.outDir, "selftest");
        stc.compare = a.compare;
        line("campaign", "gate basis: " + std::string(ext17::compare::name(a.compare.gateBasis))
                             + ". OQ-2 is UNANSWERED - whether the gate is keyed on content or on "
                               "bytes is out with the owner of the brief. Both comparisons are "
                               "run and both are reported; the basis chooses which one decides.");
        selfTest = ext17::run::runSelfTest(stc);
        selfTestRan = true;

        const std::string stJson = selfTest.toJson() + "\n";
        const std::string stPath = joinPath(stc.selfTestDir, "self-test.json");
        if (std::FILE* f = std::fopen(stPath.c_str(), "wb")) {
            std::fwrite(stJson.data(), 1, stJson.size(), f);
            std::fclose(f);
            line("campaign", "self-test -> " + stPath);
        }

        line("campaign", "self-test: " + std::string(ext17::run::toString(selfTest.outcome)));
        if (!selfTest.passed()) {
            line("campaign", "the determinism self-test did not pass, so NO campaign run has been "
                             "attempted. [B] step 4: \"Do not build further until it passes.\" "
                             "The reason is in " + stPath + " under \"outcome\" and \"detail\".");
            ext17::log::closeMirror();
            return 3;
        }
        if (a.command == "self-test") {
            ext17::log::closeMirror();
            return 0;
        }
    }

    const std::string runsDir = joinPath(a.outDir, "runs");
    ext17::proc::createDirectories(runsDir);

    const int count = campaignRuns;
    std::vector<ext17::run::RunRecord> records;
    records.reserve(static_cast<std::size_t>(count));
    bool ceilingReached = false;

    for (int n = 0; n < count; ++n) {
        ext17::run::RunConfig cfg = a.run;
        cfg.runId = ordinal(a.firstRun + n);
        cfg.runDir = joinPath(runsDir, cfg.runId);
        // G1: the failure of any one run does not end the campaign.
        records.push_back(ext17::run::executeRun(cfg));

        // CR-CAP-5, the other half: the ceiling is checked against ACTUAL usage after every run,
        // not only against a projection before the first. Reaching it stops the campaign with a
        // named outcome, and every run already completed stays valid and readable - which is the
        // whole difference between stopping and filling the disk.
        if (a.diskCeilingBytes > 0) {
            const unsigned long long used = ext17::proc::directorySizeBytes(a.outDir);
            if (used >= a.diskCeilingBytes) {
                ceilingReached = true;
                line("campaign", "disk ceiling reached: " + std::to_string(used)
                                     + " bytes used of a " + std::to_string(a.diskCeilingBytes)
                                     + " byte ceiling. Stopping after run " + cfg.runId + " of "
                                     + std::to_string(count)
                                     + "; every completed run remains valid and readable.");
                break;
            }
        }
    }

    if (a.command == "repeat") {
        writeCampaignSummary(joinPath(a.outDir, "campaign.json"), records, a.run.predicate,
                             selfTestRan ? &selfTest : nullptr);
    }

    int completed = 0;
    for (const auto& r : records) {
        if (r.outcome == ext17::run::RunOutcome::Completed) { ++completed; }
    }
    line("campaign", std::to_string(completed) + " of " + std::to_string(records.size())
                         + " runs completed");

    ext17::log::closeMirror();
    // A campaign stopped at its ceiling did not do what it was asked to do, even if every run it
    // did attempt completed. That is a distinct thing from a failing run and it is named as one
    // in the log; the exit code cannot carry a fifth value, so it carries "not everything asked
    // for happened", which is what 1 means.
    if (ceilingReached) return 1;
    return completed == static_cast<int>(records.size()) ? 0 : 1;
}
