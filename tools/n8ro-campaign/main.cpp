// EXT-17 — n8ro-campaign, the campaign runner's CLI.
//
// At M2 it exposes two commands: `run-once` and `repeat`. Neither judges anything — there are
// no conditions until M6 — so a run that reaches its stop predicate is `completed`, not `pass`.
// Collapsing those two would be the mistake tenet 2 exists to prevent, one milestone early.
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
"n8ro-campaign - EXT-17 headless campaign runner (milestone 2: execution only)\n"
"\n"
"usage: n8ro-campaign run-once --out-dir <dir> [options]\n"
"       n8ro-campaign repeat --out-dir <dir> --count <n> [options]\n"
"       n8ro-campaign --help\n"
"\n"
"commands:\n"
"  run-once                 execute one run into <out-dir>/runs/<run-id>.\n"
"  repeat                   execute --count runs of one configuration, continuing past a\n"
"                           run that fails, and write a campaign summary.\n"
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
"  --capture-max-bytes <n>  per-capture byte bound, passed to the recorder. Default 0,\n"
"                           meaning unbounded. A campaign-level ceiling is not this;\n"
"                           see OQ-6.\n"
"  --on-size-limit <a>      stop (default) or rotate, on reaching --capture-max-bytes.\n"
"                           rotate makes a run's capture a set of files whose segment\n"
"                           ordinals restart in every part.\n"
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
"  --help                   this text.\n"
"\n"
"exit codes:\n"
"  0  every run completed - the stop predicate was satisfied and teardown was clean\n"
"  1  at least one run did not complete (timeout or infrastructure error)\n"
"  2  usage or configuration error; no run was attempted");
}

struct Args {
    std::string command;
    ext17::run::RunConfig run;
    std::string outDir;
    unsigned long long frames = 1200;
    int count = 20;
    int firstRun = 0;
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
        else if (opt == "--capture-max-bytes") { const char* v = next("--capture-max-bytes"); if (!v) return false; a.run.captureMaxBytes = std::strtoull(v, nullptr, 10); }
        else if (opt == "--on-size-limit")     { const char* v = next("--on-size-limit"); if (!v) return false; a.run.onSizeLimit = v; }
        else if (opt == "--n8ro-release")      { const char* v = next("--n8ro-release"); if (!v) return false; a.run.n8roRelease = v; }
        else if (opt == "--path-prepend")      { const char* v = next("--path-prepend"); if (!v) return false; a.run.pathPrepend = v; }
        else if (opt == "--query-catalogue")   { a.run.queryCatalogue = true; }
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
                          const ext17::run::StopPredicate& predicate) {
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
    w.member("schema", std::string("ext17-campaign-summary/1"));
    w.member("milestone", std::string("M2 - execution only; no conditions are evaluated, so no "
                                      "run is a pass or a fail yet"));
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
    ext17::log::mirrorToFile(joinPath(a.outDir, "campaign.log"));

    a.run.predicate = ext17::run::StopPredicate::frameBudget(a.frames);
    line("campaign", "stop predicate: " + a.run.predicate.statement());
    line("campaign", "run timeout: " + std::to_string(a.run.runTimeoutMs)
                         + " ms (backstop; never the definition of the end)");

    const std::string runsDir = joinPath(a.outDir, "runs");
    ext17::proc::createDirectories(runsDir);

    const int count = (a.command == "run-once") ? 1 : a.count;
    std::vector<ext17::run::RunRecord> records;
    records.reserve(static_cast<std::size_t>(count));

    for (int n = 0; n < count; ++n) {
        ext17::run::RunConfig cfg = a.run;
        cfg.runId = ordinal(a.firstRun + n);
        cfg.runDir = joinPath(runsDir, cfg.runId);
        // G1: the failure of any one run does not end the campaign.
        records.push_back(ext17::run::executeRun(cfg));
    }

    if (a.command == "repeat") {
        writeCampaignSummary(joinPath(a.outDir, "campaign.json"), records, a.run.predicate);
    }

    int completed = 0;
    for (const auto& r : records) {
        if (r.outcome == ext17::run::RunOutcome::Completed) { ++completed; }
    }
    line("campaign", std::to_string(completed) + " of " + std::to_string(records.size())
                         + " runs completed");

    ext17::log::closeMirror();
    return completed == static_cast<int>(records.size()) ? 0 : 1;
}
