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

#include "../../src/assert/Conditions.h"
#include "../../src/assert/Judge.h"
#include "../../src/common/Json.h"
#include "../../src/common/JsonParse.h"
#include "../../src/common/Log.h"
#include "../../src/param/Axis.h"
#include "../../src/proc/Process.h"
#include "../../src/run/RunOnce.h"
#include "../../src/run/RunRecord.h"
#include "../../src/run/SelfTest.h"
#include "../../src/run/StopPredicate.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <array>
#include <optional>
#include <cstdio>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

using ext17::log::line;

void printHelp() {
    std::puts(
"n8ro-campaign - EXT-17 headless campaign runner (milestone 6: execution, the\n"
"                determinism gate, one parameterisation axis, declared\n"
"                conditions, and the four ugly realities)\n"
"\n"
"usage: n8ro-campaign run-once  --out-dir <dir> [options]\n"
"       n8ro-campaign repeat    --out-dir <dir> --count <n> [options]\n"
"       n8ro-campaign repeat    --out-dir <dir> --campaign <file> [options]\n"
"       n8ro-campaign self-test --out-dir <dir> [options]\n"
"       n8ro-campaign report    --out-dir <dir> [--campaign <file>]\n"
"       n8ro-campaign --help\n"
"\n"
"commands:\n"
"  run-once                 execute one run into <out-dir>/runs/<run-id>. One run is not a\n"
"                           campaign, so it does not self-test.\n"
"  repeat                   run the determinism self-test, then execute the campaign,\n"
"                           continuing past a run that fails, and write a campaign\n"
"                           summary carrying the self-test's result. Without\n"
"                           --campaign that is --count runs of one configuration;\n"
"                           with it, one run per declared parameter value.\n"
"  report                   re-render a STORED campaign's report from the run records it\n"
"                           already wrote. Starts nothing, reads no capture, and needs no\n"
"                           host, scenario or recorder - a reviewer re-reading last week's\n"
"                           campaign has none of those and needs none, and a twenty-run\n"
"                           campaign takes about 25 minutes to produce. It DOES still need\n"
"                           C:\\N8RO\\bin on PATH, because it is a command of this binary and\n"
"                           this binary links the SDK; without it you get 0xC0000135 and no\n"
"                           output. n8ro-judge and n8ro-capture link nothing and need no\n"
"                           install at all (F-52). Pass --campaign to order the sweep tables:\n"
"                           the axis declares that order and it is not re-derivable from the\n"
"                           run records. It prints through the SAME printer the live campaign\n"
"                           uses, so a re-rendered report cannot disagree with the one the\n"
"                           campaign printed.\n"
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
"    bytes     byte for byte, with platform.model_path excluded, and that is the only\n"
"              exclusion made. s14's host-dependent list widened to three at the fifth pin;\n"
"              the other two are keys only a ROTATED capture carries, which this runner never\n"
"              produces (OQ-6 decided stop). See n8ro-compare --help and F-50.\n"
"              EXPECTED TO FAIL here, and never engineered to pass.\n"
"\n"
"  Which of the two decides is OQ-2, and it is DECIDED: content, by the DRI on 2026-09-01,\n"
"  from the brief's own words - and CONCURRED with by the mentor the same day, who reached\n"
"  the same answer independently. It was still never ANSWERED by the brief's author, who has\n"
"  not replied, and those are three different words on purpose: criterion 2 is the author's\n"
"  to discharge, so a second opinion raises confidence without closing the question.\n"
"\n"
"  The deciding sentence is the brief's own statement of what the self-test is for - \"if it\n"
"  ever fails, you have found either a defect in your harness or something far more\n"
"  interesting, and you must be able to tell which\" - which a byte gate cannot serve,\n"
"  because it fails 100% of the time on this platform and so distinguishes neither case.\n"
"\n"
"  A pass on the content basis therefore discharges the brief's acceptance criterion 2 under\n"
"  the CONTENT reading, which is now ruled rather than unruled. It does not discharge it\n"
"  under a byte reading and nothing here claims it does.\n"
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
"  --count <n>              repeat only: how many runs of ONE configuration. Default 20.\n"
"                           Cannot be combined with --campaign, where the number of\n"
"                           runs is the number of values the axis declares.\n"
"  --first-run <n>          repeat only: ordinal of the first run. Default 0.\n"
"\n"
"the parameterisation axis (CR-PAR-1, and step 5 of the brief):\n"
"  --campaign <file>        a JSON file declaring the ONE axis this campaign varies,\n"
"                           and the values it takes. Changing what a sweep varies is\n"
"                           an edit to this file and no rebuild. repeat only.\n"
"\n"
"  [B]: \"One axis done properly beats four done loosely.\" Which axis is OQ-4, and it\n"
"  is DECIDED - initial positions and velocities, as one declared scalar applied to\n"
"  named entities before start. It was decided by exercising a range rather than by\n"
"  argument, and the axis has a measured fidelity ceiling - see --help's note on it.\n"
"\n"
"  {\n"
"    \"axis\": {\n"
"      \"name\": \"red_raid_speed_ms\",      label in every run record and in the report\n"
"      \"kind\": \"velocity_ned_scaled\",    velocityNed = direction * value\n"
"      \"applies_to\": \"velocityNed magnitude\",       free text, for the report\n"
"      \"units\": \"m/s\",                             free text, for the report\n"
"      \"entity_groups\": [\n"
"        { \"direction_ned\": [-1, 0, 0], \"names\": [\"RedUAV_N_01\", \"RedUAV_N_02\"] },\n"
"        { \"direction_ned\": [0, -1, 0], \"names\": [\"RedUAV_E_01\"] }\n"
"      ],\n"
"      \"values\": [\"11\", \"27.5\", \"55\", \"110\", \"220\"],\n"
"      \"self_test_value\": \"55\"           optional; defaults to the first value\n"
"    }\n"
"  }\n"
"\n"
"  Entities are NAMED, never matched. There is no glob: resolving one would mean\n"
"  subscribing the control path to sim/entity/state, which would perturb the very\n"
"  publication schedule the determinism gate measures - and a pattern that silently\n"
"  matches nothing is the failure this project keeps finding. A named entity that\n"
"  carries no sample in a run's own capture is reported in that run's record.\n"
"\n"
"  A value is carried as the TEXT you wrote it as, into the run record and into the\n"
"  report. The double derived from it exists to publish it and to order the sweep,\n"
"  and is never printed - a re-formatted double would put CR-DET-2's locale hazard\n"
"  back on a path the build searches for it.\n"
"\n"
"  self_test_value is the value the determinism gate runs at. CR-DET-1 says \"the\n"
"  same configuration twice\" and a sweep has many, so the gate runs at ONE of them,\n"
"  and it establishes determinism FOR THAT VALUE. It is one claim at a named point,\n"
"  not one per run, and the report says so. Both gate runs are copies of one\n"
"  configuration, which is what makes them a valid pair (CR-PAR-1).\n"
"\n"
"  Two runs at DIFFERENT values are never compared. They are two configurations, and\n"
"  a gate over them would report a difference meaning only that the sweep worked.\n"
"\n"
"conditions (CR-AS-1..4, and steps 6 and 7 of the brief):\n"
"  --conditions <file>      a JSON file declaring what each run is judged against. Loaded\n"
"                           and validated BEFORE any host is started, so a duplicate id, an\n"
"                           unrecognised kind, an unknown key or a key written twice each\n"
"                           costs ten seconds rather than twenty runs. Without it no run is\n"
"                           judged and every completed run is reported `completed` - which\n"
"                           is the only honest name for a run nothing looked at.\n"
"\n"
"  The vocabulary is CLOSED at the three kinds the brief names - proximity between two\n"
"  entities, presence in a region, reaching a terminal state. A fourth is a named parse\n"
"  error, never a skipped condition. Units are the platform's own and are never converted:\n"
"  metres, degrees, and the platform's [lat, lon, alt] order.\n"
"\n"
"  {\n"
"    \"conditions\": [\n"
"      {\"id\": \"raid-leader-reaches-airfield\", \"kind\": \"proximity\",\n"
"       \"entities\": [\"RedUAV_N_01\", \"BlueBase_Airfield\"], \"within_m\": 3000},\n"
"\n"
"      {\"id\": \"leader-crosses-corridor\", \"kind\": \"area\", \"entity\": \"RedUAV_N_01\",\n"
"       \"test\": \"inside\",           inside (the default) or outside\n"
"       \"region\": {\"shape\": \"polygon\", \"vertices\": [[-23.47, -68.29], ...]}},\n"
"\n"
"      {\"id\": \"command-centre-destroyed\", \"kind\": \"terminal_state\",\n"
"       \"expect\": \"not_met\",        the one key added to EXT-08's shape - see below\n"
"       \"entity\": \"BlueBase_CommandCenter\", \"removal_reason\": \"destroyed\"}\n"
"    ]\n"
"  }\n"
"\n"
"  An unknown key and a key written twice are REFUSED by name, which is the OPPOSITE of the\n"
"  capture format's rule (s13) and deliberately so: a producer adds keys and an old reader\n"
"  must survive them, whereas a person writes this file and \"within_meters\" for\n"
"  \"within_m\" would be a threshold that silently did not apply. A key beginning with '_'\n"
"  is a comment.\n"
"\n"
"  \"scenario_unload\" is refused as a removal_reason: it is what the engine's stop path\n"
"  writes for every surviving entity at teardown - measured, 267 of 385 removals across the\n"
"  committed sweep, all at sim_time_s 0 - so a condition on it is met in every run for\n"
"  every entity, at a time that points at the wrong end of the run.\n"
"\n"
"  \"expect\" is the one key this project adds. The vendored schema is a referee: it reports\n"
"  whether a condition was satisfied and says nothing about whether that is welcome. Two of\n"
"  the brief's three questions survive being read as \"this should hold\"; the third does\n"
"  not - \"did anything reach a terminal state it SHOULD NOT have\" is an assertion of\n"
"  non-occurrence. The verdict still records the FACT (met / not met); whether it was the\n"
"  asserted one is separate (satisfied / violated / undetermined). Only a VIOLATION fails a\n"
"  run. OQ-5 was decided by trying the vendored shape rather than by reading it.\n"
"\n"
"  A verdict is THREE-valued. An assertion never reads absence as evidence (CR-AS-4): a\n"
"  not-met verdict is reported only where it can be BOUNDED, and otherwise reports\n"
"  indeterminate with the reason. `indeterminate` is a VERDICT state and never a fifth RUN\n"
"  outcome - the brief fixes the run vocabulary at four. Judge a stored campaign again, or\n"
"  against new conditions, with n8ro-judge.\n"
"\n"
"the four ugly realities, injected on purpose (CR-EX-6, and step 8 of the brief):\n"
"  --inject-fault <name>    inject one of the four faults the brief says a hundred-run\n"
"                           campaign will meet. The campaign must survive it, record what\n"
"                           happened, and continue to the next run.\n"
"  --inject-at-run <n>      inject it into that run only. Default: every run.\n"
"\n"
"    host_start_failure     the host executable is a path that does not exist\n"
"    scenario_load_refusal  a scenario name the catalogue does not contain. The host does\n"
"                           not fail - it sits idle, which is the dangerous shape - and the\n"
"                           wait for `loaded` is what times out\n"
"    run_never_ends         a stop predicate the run cannot reach, against a short timeout.\n"
"                           Produces `timeout`, its OWN outcome, never `fail`\n"
"    host_dies_mid_run      the host is terminated THROUGH THE HANDLE THIS RUN CREATED,\n"
"                           never by image name. Produces `infrastructure_error`, told\n"
"                           apart from a timeout by asking the process itself\n"
"\n"
"  The injected fault is written into that run's run.json and into the campaign summary, so\n"
"  a campaign run under injection can never be mistaken for a clean one.\n"
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
"  0  every run passed - or, with no --conditions declared, every run completed\n"
"  1  at least one run failed, timed out or hit an infrastructure error, or the campaign\n"
"     stopped at its disk ceiling. A campaign that meets an injected fault and carries on\n"
"     exits 1: it did exactly what CR-EX-6 asks, and it still holds a run that is not a\n"
"     pass. The four counts in the summary are what to read, not this number alone\n"
"  2  usage or configuration error; no run was attempted\n"
"  3  the determinism self-test did not pass, and NO campaign run was attempted. Step 4 of\n"
"     the brief: \"Do not build further until it passes.\" Whether the gate FAILED or could\n"
"     not be established at all - a self-test run that did not complete has established\n"
"     nothing about determinism and is not a determinism failure - is named in\n"
"     <out-dir>/selftest/self-test.json under \"outcome\"\n"
"  4  an exception escaped, which nothing here should ever produce - rule 7 of the\n"
"     brief is \"Never throw\". It is a defect in the harness, reported as one and\n"
"     never as a result about anything this tool was asked to read\n");
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

    // CR-PAR-1: the axis, declared in campaign configuration rather than in code. Changing what
    // the sweep varies is an edit to this file and no rebuild, which is the criterion in its
    // own words. Absent means an unparameterised campaign, which behaves exactly as at M4.
    std::string campaignFile;
    ext17::param::Axis axis;
    bool hasAxis = false;
    bool countGiven = false;

    // CR-AS-1: the conditions, declared outside the code, loaded and validated BEFORE any host
    // is started. Absent means an unjudged campaign, which behaves exactly as at M5 and reports
    // every completed run as `completed` rather than inventing a pass for it.
    std::string conditionsFile;
    ext17::assertion::ConditionFile conditions;
    bool hasConditions = false;

    // CR-EX-6: which of the four ugly realities to inject, and into which run.
    std::string injectFault;
    int injectRun = -1;

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
        else if (opt == "--count")          { if (!asInt("--count", a.count)) return false; a.countGiven = true; }
        else if (opt == "--campaign")       { const char* v = next("--campaign"); if (!v) return false; a.campaignFile = v; }
        else if (opt == "--conditions")     { const char* v = next("--conditions"); if (!v) return false; a.conditionsFile = v; }
        else if (opt == "--inject-fault")   { const char* v = next("--inject-fault"); if (!v) return false; a.injectFault = v; }
        else if (opt == "--inject-at-run")  { const char* v = next("--inject-at-run"); if (!v) return false; a.injectRun = std::atoi(v); }
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

// Turn one declared value of the axis into a run configuration: the value the run record will
// state, and the hook that actually applies it.
//
// **This is the only place a declaration becomes bus traffic**, and it acts through
// `RunConfig::afterLoadBeforeStart` - the seam M2 built for the OQ-4 spike. There is no second
// mechanism, deliberately: a parameter applied anywhere else would be a parameter the run
// record does not know about.
//
// The hook fires after the scenario reports loaded and before `start` is published, which is
// where an initial condition has to be set - measured at M2 (`p1`), and measured again at M5
// across a swept range (measured at M5). What it publishes is not what it concludes from:
// `sendEntityUpdate` returning true means the message reached the bus, and whether an entity of
// that name was there is answered by the capture, in RunOnce's read-back.
void applyAxisValue(const ext17::param::Axis& axis, const ext17::param::Value& value,
                    ext17::run::RunConfig& cfg) {
    cfg.parameterName = axis.name;
    cfg.parameterValueText = value.text;
    cfg.parameterAppliesTo = axis.appliesTo;
    cfg.parameterUnits = axis.units;
    cfg.parameterEntities.clear();
    for (const auto& t : axis.targets) { cfg.parameterEntities.push_back(t.entity); }

    // Captured by value: the campaign's Args outlive every run, but a hook that referred to
    // them would be a lifetime argument rather than a guarantee, and this one is copied into
    // two self-test runs and N campaign runs.
    const ext17::param::Axis axisCopy = axis;
    const ext17::param::Value valueCopy = value;
    cfg.afterLoadBeforeStart = [axisCopy, valueCopy](ext17::control::EngineControl& c) {
        int published = 0;
        for (const auto& t : axisCopy.targets) {
            const std::array<double, 3> v = axisCopy.velocityFor(t, valueCopy);
            if (c.publishEntityUpdate(t.entity, std::nullopt, v, std::nullopt)) { ++published; }
        }
        line("axis", axisCopy.name + " = " + valueCopy.text
                         + (axisCopy.units.empty() ? "" : " " + axisCopy.units) + ": published "
                         + std::to_string(published) + " of "
                         + std::to_string(axisCopy.targets.size())
                         + " entity update(s) before start");
    };
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

// The campaign's runs in SWEEP ORDER - ascending by parameter value, ties in the order the
// campaign file declared them. CR-PAR-2's first criterion is that the sweep output "orders runs
// by parameter value", and this is the one place that ordering is decided, so the JSON array
// and the printed table cannot disagree about it.
//
// The comparison is on the parsed double and the tie-break is the record's own position, so the
// order is total and is the same on every machine. Sorting on the declared TEXT would put "110"
// before "27.5", which is a sweep ordered by spelling.
std::vector<const ext17::run::RunRecord*> sweptRecords(
    const std::vector<ext17::run::RunRecord>& records) {
    std::vector<const ext17::run::RunRecord*> out;
    out.reserve(records.size());
    for (const auto& r : records) { out.push_back(&r); }
    std::stable_sort(out.begin(), out.end(),
                     [](const ext17::run::RunRecord* a, const ext17::run::RunRecord* b) {
                         return ext17::json::toDoubleCLocale(a->parameterValueText)
                                < ext17::json::toDoubleCLocale(b->parameterValueText);
                     });
    return out;
}

// CR-PAR-2, the half a person reads: *"the presentation is legible in the report's own format -
// a reviewer can see the trend without opening another tool."*
//
// So it is a fixed-width table in the campaign log, ordered by parameter value, with a bar
// scaled to the result column. The bar is what makes it a trend rather than a list of numbers:
// twenty rows of integers require the reader to do the comparing, and a shape does not.
//
// Two honesty rules are built into it rather than left to the reader.
//
//   - The bar is scaled between the MINIMUM and maximum of the column, not from zero, and the
//     header says so. A column running 89 to 104 drawn from zero is five identical bars.
//   - A run that did not complete gets no bar at all. Its result column is whatever its capture
//     happened to contain, which is not a point on the trend, and drawing it as one would put a
//     failure on the same line as a measurement.
// CR-REP-3, in one place: the four outcomes, separately, summing to the runs attempted, with no
// aggregate that collapses two of them. Shared by the live campaign and by `report`, so a
// re-rendered report cannot disagree with the one the campaign printed.
//
// `completed` is a FIFTH KEY and not a fifth outcome - it is what a run is called when no
// condition file was declared and nothing judged it - and it is printed only when non-zero, so a
// judged campaign's closing lines carry [B]'s four and nothing else.
struct OutcomeCounts {
    int pass = 0, fail = 0, timeout = 0, infrastructureError = 0, completedUnjudged = 0;
    long long indeterminateVerdicts = 0;
};

OutcomeCounts printOutcomeSummary(const std::vector<ext17::run::RunRecord>& records) {
    OutcomeCounts c;
    for (const auto& r : records) {
        switch (r.outcome) {
            case ext17::run::RunOutcome::Pass:                ++c.pass; break;
            case ext17::run::RunOutcome::Fail:                ++c.fail; break;
            case ext17::run::RunOutcome::Completed:           ++c.completedUnjudged; break;
            case ext17::run::RunOutcome::Timeout:             ++c.timeout; break;
            case ext17::run::RunOutcome::InfrastructureError: ++c.infrastructureError; break;
        }
        c.indeterminateVerdicts += r.verdictsIndeterminate;
    }
    line("campaign", std::to_string(records.size()) + " run(s) attempted");
    line("campaign", "  pass                  " + std::to_string(c.pass));
    line("campaign", "  fail                  " + std::to_string(c.fail));
    line("campaign", "  timeout               " + std::to_string(c.timeout));
    line("campaign", "  infrastructure_error  " + std::to_string(c.infrastructureError));
    if (c.completedUnjudged > 0) {
        line("campaign", "  completed (unjudged)  " + std::to_string(c.completedUnjudged)
                             + "  - no condition file was declared, so nothing judged these");
    }
    line("campaign", "  the four sum to "
                         + std::to_string(c.pass + c.fail + c.timeout + c.infrastructureError
                                          + c.completedUnjudged)
                         + ", and no aggregate above merges two of them");
    if (c.indeterminateVerdicts > 0) {
        line("campaign", "  " + std::to_string(c.indeterminateVerdicts)
                             + " INDETERMINATE verdict(s) across the campaign - a verdict state, "
                               "never a fifth run outcome. See each run's verdicts.jsonl.");
    }
    return c;
}

// CR-PAR-2's third criterion, and the half M5 recorded as unmet: *"at least one condition in
// the committed example campaign actually changes outcome"*. A result changing across the sweep
// was demonstrable at M5; a VERDICT changing was not, because no condition existed.
//
// The table shows one column per condition **whose outcome actually changes across the sweep**,
// and lists the rest below as constant. That is a deliberate choice about what a trend is: with
// seven conditions and twenty runs, a full matrix is 140 cells in which the two that move are
// invisible. A column that never changes is not a trend and is reported as a fact instead.
void printVerdictSweep(const std::vector<const ext17::run::RunRecord*>& ordered) {
    if (ordered.empty() || !ordered.front()->judged) { return; }

    // The condition list, in declaration order, taken from the first run that has verdicts. Every
    // run has one verdict per declared condition (CR-AS-2), so any judged run gives the order.
    const ext17::run::RunRecord* reference = nullptr;
    for (const auto* r : ordered) {
        if (!r->verdictConditionIds.empty()) { reference = r; break; }
    }
    if (reference == nullptr) { return; }
    const std::size_t n = reference->verdictConditionIds.size();

    // Which columns move. A run that was never judged - an infrastructure error, say - carries no
    // verdicts at all, and contributes nothing to this question rather than counting as a change.
    std::vector<bool> changes(n, false);
    for (std::size_t i = 0; i < n; ++i) {
        std::string first;
        for (const auto* r : ordered) {
            if (r->verdictOutcomes.size() != n) { continue; }
            if (first.empty()) { first = r->verdictOutcomes[i]; continue; }
            if (r->verdictOutcomes[i] != first) { changes[i] = true; break; }
        }
    }

    std::size_t moving = 0;
    for (std::size_t i = 0; i < n; ++i) { if (changes[i]) { ++moving; } }

    line("sweep", "VERDICTS ACROSS THE SWEEP  -  one column per condition whose outcome CHANGES");
    line("sweep", "");
    if (moving == 0) {
        line("sweep", "  No condition changes outcome across this sweep. CR-PAR-2's third "
                      "criterion asks for one that does, and this campaign does not show one - "
                      "which is a fact about the campaign, not a formatting choice.");
        line("sweep", "");
    } else {
        std::string head = "  value  run   outcome              ";
        std::vector<std::size_t> cols;
        for (std::size_t i = 0; i < n; ++i) {
            if (!changes[i]) { continue; }
            cols.push_back(i);
            head += " C" + std::to_string(cols.size());
            while (head.size() % 1 != 0) { break; }
            head += "   ";
        }
        line("sweep", head);

        for (const auto* r : ordered) {
            std::string row = "  ";
            std::string v = r->parameterValueText;
            while (v.size() < 5) { v += ' '; }
            row += v + "  " + r->runId + "   ";
            std::string o = ext17::run::toString(r->outcome);
            while (o.size() < 20) { o += ' '; }
            row += o;
            for (std::size_t c : cols) {
                std::string cell = "  ? ";
                if (r->verdictOutcomes.size() == n) {
                    const std::string& out = r->verdictOutcomes[c];
                    cell = out == "satisfied" ? "  OK" : (out == "violated" ? "  XX" : "  ??");
                }
                row += cell + "  ";
            }
            line("sweep", row);
        }
        line("sweep", "");
        for (std::size_t k = 0; k < cols.size(); ++k) {
            line("sweep", "  C" + std::to_string(k + 1) + "  "
                              + reference->verdictConditionIds[cols[k]]);
        }
        line("sweep", "");
        line("sweep", "  OK  satisfied - the condition's answer was the asserted one");
        line("sweep", "  XX  VIOLATED  - it was decided and was not. This is what makes a run "
                      "fail");
        line("sweep", "  ??  indeterminate - this capture cannot decide it. Never folded into "
                      "either (CR-AS-4)");
        line("sweep", "");
    }

    if (moving < n) {
        line("sweep", "  Constant across the whole sweep, and therefore not a trend:");
        for (std::size_t i = 0; i < n; ++i) {
            if (changes[i]) { continue; }
            std::string state = "(never judged)";
            for (const auto* r : ordered) {
                if (r->verdictOutcomes.size() == n) {
                    state = r->verdictOutcomes[i] + " / " + r->verdictStates[i];
                    break;
                }
            }
            line("sweep", "    " + reference->verdictConditionIds[i] + "  -  " + state);
        }
        line("sweep", "");
    }
}

void printSweep(const std::vector<ext17::run::RunRecord>& records,
                const ext17::param::Axis& axis) {
    const auto ordered = sweptRecords(records);
    if (ordered.empty()) { return; }

    // A run whose capture has NO RUNNING SEGMENT has no result. Its counts are zero because
    // nothing was measured, and zero is not a measurement of zero - plotting it would put a
    // missing point on the trend AND drag the bar scale's floor down to it, which makes every
    // other bar wrong as well. Such a run is scaled out, drawn out, and named.
    //
    // This is not hypothetical: R14 measured segment 0 classifying `frozen` on a parameterised
    // run, and the committed sweep hit it twice in seven.
    // "The run produced a measurement", which is NOT "the run passed". A run that failed its
    // conditions still measured its capture, and excluding it would drop real points off the
    // trend for a reason that has nothing to do with the trend.
    //
    // **This is F-24 again, and it broke the same table a second time.** M5 fixed a run with no
    // running segment being reported as 0; M6 renamed the outcome a completed run gets - it is
    // now `pass` or `fail` - and this predicate still asked for `Completed`, so after M6 EVERY
    // row printed `-` and no bar. Found by reading the twenty-run campaign's output, exactly as
    // F-24 was. Recorded as F-35.
    const auto ran = [](const ext17::run::RunRecord* r) {
        return r->outcome == ext17::run::RunOutcome::Pass ||
               r->outcome == ext17::run::RunOutcome::Fail ||
               r->outcome == ext17::run::RunOutcome::Completed;
    };
    const auto measured = [&ran](const ext17::run::RunRecord* r) {
        return ran(r) && r->captureRunSegmentFound;
    };

    long long lo = 0, hi = 0;
    bool any = false;
    int unmeasured = 0;
    for (const auto* r : ordered) {
        if (!measured(r)) {
            if (ran(r)) { ++unmeasured; }
            continue;
        }
        if (!any) { lo = hi = r->captureEntityAdds; any = true; }
        lo = r->captureEntityAdds < lo ? r->captureEntityAdds : lo;
        hi = r->captureEntityAdds > hi ? r->captureEntityAdds : hi;
    }

    std::size_t valueWidth = 5;
    for (const auto* r : ordered) {
        valueWidth = r->parameterValueText.size() > valueWidth ? r->parameterValueText.size()
                                                               : valueWidth;
    }

    line("sweep", "");
    line("sweep", "SWEEP  " + axis.name
                      + (axis.units.empty() ? "" : "  (" + axis.units + ")")
                      + (axis.appliesTo.empty() ? "" : "  " + axis.appliesTo)
                      + "  -  " + std::to_string(ordered.size()) + " run(s), ordered by value");
    line("sweep", "");

    char header[240];
    std::snprintf(header, sizeof header, "  %-*s  %-4s  %-20s  %8s  %7s  %8s",
                  static_cast<int>(valueWidth), "value", "run", "outcome", "adds", "keys",
                  "samples");
    line("sweep", header);

    for (const auto* r : ordered) {
        std::string bar;
        if (measured(r) && any && hi > lo) {
            const int cells = static_cast<int>(
                (r->captureEntityAdds - lo) * 40 / (hi - lo));
            bar.assign(static_cast<std::size_t>(cells < 0 ? 0 : cells), '#');
            if (bar.empty()) { bar = "."; }   // the minimum is a point on the trend, not a gap
        } else if (!ran(r)) {
            bar = "(no bar - this run did not complete)";
        } else if (!r->captureRunSegmentFound) {
            bar = "(no bar - no RUNNING segment, so nothing here was measured)";
        }

        char counts[64];
        if (measured(r)) {
            std::snprintf(counts, sizeof counts, "%8lld  %7lld", r->captureEntityAdds,
                          r->captureEntityKeys);
        } else {
            // "-" and not "0". The distinction is the whole point of the branch.
            std::snprintf(counts, sizeof counts, "%8s  %7s", "-", "-");
        }

        char row[400];
        std::snprintf(row, sizeof row, "  %-*s  %-4s  %-20s  %s  %8lld  %s",
                      static_cast<int>(valueWidth), r->parameterValueText.c_str(),
                      r->runId.c_str(), ext17::run::toString(r->outcome), counts,
                      r->captureSamples, bar.c_str());
        line("sweep", row);
    }

    line("sweep", "");

    // [B]'s criterion 3 asks for "a result that varies with the parameter, presented so the
    // trend is visible". The most consequential way for that to be false is for the axis to have
    // named an entity that was never there: every run is then the baseline, the table is flat,
    // and nothing above says why. `run.json` and `campaign.json` have carried
    // `every_named_entity_present` since M5 and the campaign log names it per run - but the
    // REPORT a person reads did not, so the one fact that explains a flat sweep was the one
    // fact they had to go and look for.
    int runsMissingEntities = 0;
    std::vector<std::string> namesNeverSeen;
    for (const auto* r : ordered) {
        if (r->parameterEntitiesMissing.empty()) { continue; }
        ++runsMissingEntities;
        for (const std::string& e : r->parameterEntitiesMissing) {
            bool already = false;
            for (const std::string& k : namesNeverSeen) {
                if (k == e) { already = true; break; }
            }
            if (!already) { namesNeverSeen.push_back(e); }
        }
    }
    if (runsMissingEntities > 0) {
        std::string names;
        for (const std::string& e : namesNeverSeen) {
            names += (names.empty() ? "" : ", ") + e;
        }
        line("sweep", "  THE AXIS DID NOT REACH EVERY ENTITY IT NAMED. "
                          + std::to_string(runsMissingEntities) + " of "
                          + std::to_string(ordered.size())
                          + " run(s) carry a named entity that has no sample in their capture: "
                          + names + ".");
        line("sweep", "  Publishing an entity update returns true when the message reached the "
                      "bus and says nothing about whether anything received it, so a mistyped "
                      "name produces a sweep in which every run is the baseline. Read this "
                      "BEFORE reading any trend above: a flat column here means the parameter "
                      "was not applied, not that it made no difference. Each run's run.json "
                      "carries every_named_entity_present and the names.");
        line("sweep", "");
    }

    if (unmeasured > 0) {
        line("sweep", "  " + std::to_string(unmeasured) + " run(s) that executed show `-` rather "
                      "than a number: their capture has no RUNNING segment, so nothing was "
                      "measured in them. That is NOT a result of zero, they are excluded from "
                      "the bar's scale as well as from the bar, and the sweep is that many "
                      "points short. See R12 and R14, and each run's own run.json.");
        line("sweep", "");
    }
    if (any && hi > lo) {
        line("sweep", "  the bar is `adds` scaled between " + std::to_string(lo) + " and "
                          + std::to_string(hi) + " - NOT from zero, so a small real change is "
                            "visible and is not a small real change drawn large by accident");
    } else if (any) {
        line("sweep", "  every completed run produced the same `adds` count, so there is no bar "
                      "to draw. CR-PAR-2 asks for a result that VARIES with the parameter; this "
                      "sweep does not show one, and saying so is the point of the line.");
    }
    line("sweep", "  adds    entity_add records in the run's first RUNNING segment. Roster "
                  "lifecycle agreed exactly across twenty identical runs (M2), so this column "
                  "carries no publication-schedule spread");
    line("sweep", "  keys    distinct (entity, occupancy) keys in that segment");
    line("sweep", "  samples total samples in it. This one DOES carry the platform's "
                  "publication-schedule spread, measured at 0.38% over twenty identical runs");
    line("sweep", "");
    if (!ordered.front()->judged) {
        line("sweep", "  These are counts read off each capture, NOT verdicts - this campaign "
                      "declared no conditions, so no run here is a pass or a fail. Pass "
                      "--conditions <file> to judge them.");
    }
    line("sweep", "  The determinism gate ran at " + axis.name + " = " + axis.selfTestValueText
                      + " and established determinism FOR THAT VALUE. It is one claim at a named "
                        "point, not one per run.");
    line("sweep", "");

    printVerdictSweep(ordered);
}

// The campaign summary. At M2 it counts three outcomes, and the invariant it asserts is the
// one CR-EX-4 names: every attempted run lands in exactly one category, and the categories sum.
void writeCampaignSummary(const std::string& path,
                          const std::vector<ext17::run::RunRecord>& records,
                          const ext17::run::StopPredicate& predicate,
                          const ext17::run::SelfTestResult* selfTest,
                          const ext17::param::Axis* axis) {
    int passed = 0, failed = 0, completed = 0, timedOut = 0, infra = 0;
    long long indeterminateVerdicts = 0;
    bool judged = false;
    std::string conditionsPath;
    long long conditionsDeclared = 0;
    for (const auto& r : records) {
        switch (r.outcome) {
            case ext17::run::RunOutcome::Pass:                ++passed;    break;
            case ext17::run::RunOutcome::Fail:                ++failed;    break;
            case ext17::run::RunOutcome::Completed:           ++completed; break;
            case ext17::run::RunOutcome::Timeout:             ++timedOut;  break;
            case ext17::run::RunOutcome::InfrastructureError: ++infra;     break;
        }
        indeterminateVerdicts += r.verdictsIndeterminate;
        if (r.judged) {
            judged = true;
            conditionsPath = r.conditionsPath;
            conditionsDeclared = r.conditionsDeclared;
        }
    }

    ext17::json::Writer w;
    w.beginObject();
    // Bumped at M5: the document gained an `axis` object, a `sweep` array, and per-run capture
    // counts. Every addition is additive, and the version still moves - a consumer written
    // against /2 has not seen the sweep, and a shape that grew without saying so is how a
    // reader comes to believe it read everything there was.
    // Bumped again at M6: `outcomes` gained `pass` and `fail`, every run gained a `judgement`,
    // and the `sweep` array gained a verdict per condition. A consumer written against /3 would
    // read `completed` where a run is now judged.
    w.member("schema", std::string("ext17-campaign-summary/4"));
    w.member("milestone", std::string("M6 - execution, the determinism gate, one "
                                      "parameterisation axis, and declared conditions. Runs "
                                      "are judged; `completed` now appears only when no "
                                      "condition file was declared"));

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

    // CR-REP-3: the four outcomes, separately, summing to the number of runs attempted, with no
    // aggregate anywhere that collapses two of them. `completed` is a fifth key and not a fifth
    // outcome - it is what a run is called when NO condition file was declared, so nothing
    // judged it. With conditions declared it is always 0.
    w.beginObject("outcomes");
    w.member("attempted", static_cast<std::int64_t>(records.size()));
    w.member("pass", static_cast<std::int64_t>(passed));
    w.member("fail", static_cast<std::int64_t>(failed));
    w.member("timeout", static_cast<std::int64_t>(timedOut));
    w.member("infrastructure_error", static_cast<std::int64_t>(infra));
    w.member("completed_unjudged", static_cast<std::int64_t>(completed));
    w.member("sums_to_attempted",
             passed + failed + completed + timedOut + infra
                 == static_cast<int>(records.size()));
    w.member("no_aggregate_merges_two_of_the_four", true);
    w.endObject();

    // CR-AS-4: reported alongside the four, and never merged into any of them. An indeterminate
    // verdict is a VERDICT state - the brief fixes the RUN vocabulary at four, and keeping the
    // two apart is what makes its acceptance criterion 5 stay exactly satisfied.
    if (judged) {
        w.beginObject("conditions");
        w.member("file", conditionsPath);
        w.member("declared", static_cast<std::int64_t>(conditionsDeclared));
        w.member("indeterminate_verdicts", static_cast<std::int64_t>(indeterminateVerdicts));
        w.member("indeterminate_is_a_verdict_state_not_a_run_outcome", true);
        w.member("note", std::string("An indeterminate verdict means this capture cannot decide "
                                     "that condition - it is not a pass, not a fail, and is "
                                     "never folded into either. See the README's absence "
                                     "classification (CR-AS-4)."));
        w.endObject();
    } else {
        w.memberNull("conditions");
    }

    // CR-PAR-1: what varied, and the values it took. Null - not an empty object - when the
    // campaign declared no axis, so that an unparameterised campaign's summary says so rather
    // than looking like a sweep of nothing.
    if (axis != nullptr) {
        w.beginObject("axis");
        w.member("name", axis->name);
        w.member("kind", std::string(ext17::param::toString(axis->kind)));
        if (!axis->appliesTo.empty()) { w.member("applies_to", axis->appliesTo); }
        if (!axis->units.empty())     { w.member("units", axis->units); }
        w.member("declared_in", std::string("the campaign configuration file, not in code "
                                            "(CR-PAR-1)"));
        w.beginArray("values");
        for (const auto& v : axis->values) { w.value(v.text); }
        w.endArray();
        w.member("values_are_declared_text", true);
        w.member("self_test_value", axis->selfTestValueText);
        w.member("self_test_establishes_for_this_value_only", true);
        w.beginArray("entities");
        for (const auto& t : axis->targets) { w.value(t.entity); }
        w.endArray();
        w.endObject();
    } else {
        w.memberNull("axis");
    }

    w.beginArray("runs");
    for (const auto& r : records) {
        w.beginObject();
        w.member("run_id", r.runId);
        if (!r.parameterName.empty()) { w.member("parameter_value", r.parameterValueText); }
        w.member("outcome", std::string(ext17::run::toString(r.outcome)));
        w.member("predicate_satisfied", r.predicateSatisfied);
        w.member("observed_frame", r.evaluation.observedFrame);
        w.member("observed_sim_time_s", r.evaluation.observedSimTimeS, 6);
        w.member("observed_delta_s", r.evaluation.observedDeltaS, 5);
        w.member("capture_samples", static_cast<std::int64_t>(r.captureSamples));
        w.member("capture_entity_keys", static_cast<std::int64_t>(r.captureEntityKeys));
        w.member("capture_entity_adds", static_cast<std::int64_t>(r.captureEntityAdds));
        if (r.captureBytes) { w.member("capture_bytes", *r.captureBytes); }
        else                { w.memberNull("capture_bytes"); }
        if (!r.parameterName.empty()) {
            w.member("every_named_entity_present", r.parameterEntitiesMissing.empty());
        }
        if (!r.injectedFault.empty()) { w.member("injected_fault", r.injectedFault); }
        if (r.judged) {
            w.beginObject("judgement");
            w.member("satisfied", static_cast<std::int64_t>(r.verdictsSatisfied));
            w.member("violated", static_cast<std::int64_t>(r.verdictsViolated));
            w.member("indeterminate", static_cast<std::int64_t>(r.verdictsIndeterminate));
            w.member("met", static_cast<std::int64_t>(r.verdictsMet));
            w.member("not_met", static_cast<std::int64_t>(r.verdictsNotMet));
            w.member("verdicts_file", std::string("runs/") + r.runId + "/verdicts.jsonl");
            w.endObject();
        }
        w.endObject();
    }
    w.endArray();

    // CR-PAR-2: the sweep, ORDERED BY PARAMETER VALUE, with each run's result against it. This
    // is the machine-readable half; `printSweep` is the half a person reads without opening
    // another tool. Both are generated from the same records in the same order, so they cannot
    // disagree about what happened.
    if (axis != nullptr) {
        w.beginArray("sweep");
        for (const auto& r : sweptRecords(records)) {
            w.beginObject();
            w.member("value", r->parameterValueText);
            w.member("run_id", r->runId);
            w.member("outcome", std::string(ext17::run::toString(r->outcome)));
            // null, never 0, when there was no running segment to measure in. A consumer that
            // read 0 here would be reading a measurement that was never taken (tenet 3).
            w.member("running_segment_found", r->captureRunSegmentFound);
            if (r->captureRunSegmentFound) {
                w.member("entity_adds", static_cast<std::int64_t>(r->captureEntityAdds));
                w.member("entity_keys", static_cast<std::int64_t>(r->captureEntityKeys));
            } else {
                w.memberNull("entity_adds");
                w.memberNull("entity_keys");
            }
            w.member("samples", static_cast<std::int64_t>(r->captureSamples));
            // CR-PAR-2's third criterion, finished at M6: the per-condition VERDICT at this
            // value. This is the seam M5 left - it recorded the criterion as half-met because a
            // result changed across the sweep and a verdict could not, there being no condition
            // until now.
            if (!r->verdictConditionIds.empty()) {
                w.beginArray("verdicts");
                for (std::size_t i = 0; i < r->verdictConditionIds.size(); ++i) {
                    w.beginObject();
                    w.member("condition_id", r->verdictConditionIds[i]);
                    w.member("state", r->verdictStates[i]);
                    w.member("outcome", r->verdictOutcomes[i]);
                    w.endObject();
                }
                w.endArray();
            } else {
                w.memberNull("verdicts");
            }
            w.endObject();
        }
        w.endArray();
        w.member("sweep_note",
                 std::string("Ordered by parameter value. The count columns are read off "
                             "each run's own capture by this project's reader; the `verdicts` "
                             "array beside them IS the judgement. `samples` "
                             "additionally carries the platform's publication-schedule spread, "
                             "measured at 0.38% over twenty identical runs (M2); `entity_adds` "
                             "does not, and is the count to read a trend from. A run with no "
                             "RUNNING segment reports null rather than 0 for both: nothing was "
                             "measured in it, and 0 would be a measurement - and it can decide "
                             "no condition either, so its verdicts are all indeterminate."));
    }
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


// --- `report`: re-render a stored campaign's report, without running anything -----------------
//
// CR-REP-1 asks that the report be both machine-readable and legible to a person, and [B]'s whole
// argument for keeping the recording separate from the assertions is that a stored campaign
// should be **cheap to go back to**. Re-reading its report should not require re-running it, and
// a twenty-run campaign takes about twenty-five minutes.
//
// It reads each run's own `run.json` and prints through **the same printer the live campaign
// uses** - one renderer, two entry points, the same discipline that makes `n8ro-judge`'s verdicts
// byte-identical to the live run's. A second printer would eventually disagree with the first.
//
// Nothing here starts a host or reads a capture. It reads the records the campaign already wrote.
bool loadRunRecord(const std::string& path, ext17::run::RunRecord& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { return false; }
    std::string text;
    char buffer[8192];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) { text.append(buffer, n); }
    std::fclose(f);

    ext17::json::Value doc;
    ext17::json::ParseError perr;
    if (!ext17::json::parse(text, doc, perr) || !doc.isObject()) { return false; }

    out.runId = doc.stringOr("run_id");
    const std::string outcome = doc.stringOr("outcome");
    if (outcome == "pass")                      { out.outcome = ext17::run::RunOutcome::Pass; }
    else if (outcome == "fail")                 { out.outcome = ext17::run::RunOutcome::Fail; }
    else if (outcome == "completed")            { out.outcome = ext17::run::RunOutcome::Completed; }
    else if (outcome == "timeout")              { out.outcome = ext17::run::RunOutcome::Timeout; }
    else { out.outcome = ext17::run::RunOutcome::InfrastructureError; }

    if (const ext17::json::Value* p = doc.find("parameter")) {
        if (p->isObject()) {
            out.parameterName = p->stringOr("axis");
            out.parameterValueText = p->stringOr("value");
        }
    }
    if (const ext17::json::Value* c = doc.find("capture")) {
        if (c->isObject()) {
            // The key names are run.json's own, not this loader's guesses at them. Reading
            // `entity_keys` where the record writes `running_segment_entity_keys` is how a
            // re-rendered report comes to say `-` for every run - which it did, once.
            out.captureSamples = c->integerOr("samples", 0);
            out.captureEntityKeys = c->integerOr("running_segment_entity_keys", 0);
            out.captureEntityAdds = c->integerOr("running_segment_entity_adds", 0);
            out.captureRunSegmentFound = c->boolOr("running_segment_found", false);
            out.capturePath = c->stringOr("path");
            out.captureConformant = c->boolOr("conformant", false);
            out.captureCoversWholeRun = c->boolOr("covers_whole_run", false);
        }
    }
    out.injectedFault = doc.stringOr("injected_fault");
    if (const ext17::json::Value* j = doc.find("judgement")) {
        if (j->isObject()) {
            out.judged = true;
            out.conditionsPath = j->stringOr("conditions_file");
            out.conditionsDeclared = j->integerOr("conditions_declared", 0);
            out.judgedThisRun = j->boolOr("judged_this_run", false);
            out.judgeable = j->boolOr("judgeable", false);
            if (const ext17::json::Value* v = j->find("verdicts")) {
                out.verdictsMet = v->integerOr("met", 0);
                out.verdictsNotMet = v->integerOr("not_met", 0);
                out.verdictsIndeterminate = v->integerOr("indeterminate", 0);
            }
            if (const ext17::json::Value* v = j->find("assertions")) {
                out.verdictsSatisfied = v->integerOr("satisfied", 0);
                out.verdictsViolated = v->integerOr("violated", 0);
                out.verdictsUndetermined = v->integerOr("undetermined", 0);
            }
        }
    }
    return true;
}

// The per-condition verdicts come from the run's own verdicts.jsonl rather than from run.json,
// because that file is the one `n8ro-judge --verify` compares byte for byte - so a report built
// from it is a report built from the artifact whose identity is checked.
void loadVerdicts(const std::string& path, ext17::run::RunRecord& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { return; }
    std::string text;
    char buffer[8192];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) { text.append(buffer, n); }
    std::fclose(f);

    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) { end = text.size(); }
        const std::string line = text.substr(start, end - start);
        start = end + 1;
        if (line.empty()) { continue; }
        ext17::json::Value v;
        ext17::json::ParseError perr;
        if (!ext17::json::parse(line, v, perr) || !v.isObject()) { continue; }
        out.verdictConditionIds.push_back(v.stringOr("condition_id"));
        out.verdictStates.push_back(v.stringOr("state"));
        out.verdictOutcomes.push_back(v.stringOr("outcome"));
    }
}

int runMain(int argc, char** argv) {
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
    if (a.command != "run-once" && a.command != "repeat" && a.command != "self-test" &&
        a.command != "report") {
        std::fprintf(stderr, "n8ro-campaign: expected a command - run-once, repeat, self-test "
                             "or report\n");
        return 2;
    }
    if (a.outDir.empty()) {
        std::fprintf(stderr, "n8ro-campaign: --out-dir is required\n");
        return 2;
    }

    // `report` reads a stored campaign and prints. It starts nothing, so it skips every
    // precondition below — the recorder, the disk projection, the self-test — and it must,
    // because a reviewer re-reading last week's campaign has no host and needs none.
    if (a.command == "report") {
        // The axis is read here rather than below, because everything below this block is about
        // preparing to RUN something and `report` runs nothing. Without it the sweep tables
        // cannot be printed at all: the axis declares the order the sweep is presented in, and
        // that order is a property of the campaign file rather than of the run records.
        if (!a.campaignFile.empty()) {
            std::string axisError;
            if (!ext17::param::readCampaignFile(a.campaignFile, a.axis, axisError)) {
                std::fprintf(stderr, "n8ro-campaign: %s\n", axisError.c_str());
                return 2;
            }
            a.hasAxis = true;
        }
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path runsDir = fs::path(a.outDir) / "runs";
        if (!fs::is_directory(runsDir, ec)) {
            std::fprintf(stderr, "n8ro-campaign: no runs directory under %s\n", a.outDir.c_str());
            return 2;
        }
        std::vector<fs::path> dirs;
        for (const fs::directory_entry& e : fs::directory_iterator(runsDir, ec)) {
            if (e.is_directory(ec)) { dirs.push_back(e.path()); }
        }
        std::sort(dirs.begin(), dirs.end());

        std::vector<ext17::run::RunRecord> stored;
        for (const fs::path& d : dirs) {
            ext17::run::RunRecord rec;
            if (!loadRunRecord((d / "run.json").string(), rec)) {
                std::fprintf(stderr, "n8ro-campaign: could not read %s\n",
                             (d / "run.json").string().c_str());
                return 2;
            }
            loadVerdicts((d / "verdicts.jsonl").string(), rec);
            stored.push_back(std::move(rec));
        }
        if (stored.empty()) {
            std::fprintf(stderr, "n8ro-campaign: no run records under %s\n",
                         runsDir.string().c_str());
            return 2;
        }
        line("report", "re-rendered from " + std::to_string(stored.size())
                           + " stored run record(s) in " + a.outDir);
        line("report", "nothing was run, no host was started and no capture was read - this is "
                       "the report the campaign already wrote, printed again by the same printer");
        if (a.hasAxis) {
            printSweep(stored, a.axis);
        } else {
            line("report", "no --campaign file was given, so the sweep table is not printed: the "
                           "axis and the order it declares live in that file and are not "
                           "re-derivable from the run records alone.");
        }
        printOutcomeSummary(stored);
        return 0;
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

    // ---- CR-PAR-1: the axis, read from campaign configuration ------------------------------
    if (!a.campaignFile.empty()) {
        std::string axisError;
        if (!ext17::param::readCampaignFile(a.campaignFile, a.axis, axisError)) {
            std::fprintf(stderr, "n8ro-campaign: %s\n", axisError.c_str());
            return 2;
        }
        a.hasAxis = true;
        if (a.command != "repeat") {
            std::fprintf(stderr, "n8ro-campaign: --campaign declares a sweep, and a sweep is "
                                 "what repeat runs. run-once executes one run and has no axis "
                                 "to sweep along.\n");
            return 2;
        }
        // The sweep's length IS the number of declared values. --count would be a second and
        // silently disagreeing statement of how many runs there are, and whichever lost would
        // be the one somebody wrote down on purpose.
        if (a.countGiven) {
            std::fprintf(stderr, "n8ro-campaign: --count cannot be used with --campaign. The "
                                 "number of runs is the number of values the axis declares "
                                 "(%d here). Add or remove values in %s.\n",
                         static_cast<int>(a.axis.values.size()), a.campaignFile.c_str());
            return 2;
        }
        a.count = static_cast<int>(a.axis.values.size());
    }

    // ---- CR-AS-1: the conditions, loaded and validated BEFORE any host is started ----------
    //
    // *"A malformed file, a duplicate condition id, and an unrecognised condition kind each
    // produce a distinct named error and a non-zero exit before any host is started."* This is
    // that, and it is deliberately here - above the pre-flight disk check, above the self-test,
    // above everything that costs a second - so a typo costs ten seconds rather than twenty runs
    // against a file that quietly loaded nothing.
    if (!a.conditionsFile.empty()) {
        ext17::assertion::ParseError perr;
        if (!ext17::assertion::readConditionFile(a.conditionsFile, a.conditions, perr)) {
            std::fprintf(stderr, "n8ro-campaign: condition file refused - %s\n",
                         perr.message().c_str());
            std::fprintf(stderr, "               No host was started and no run was attempted "
                                 "(CR-AS-1).\n");
            return 2;
        }
        a.hasConditions = true;
    }

    // CR-EX-6: the fault vocabulary is closed, and an unrecognised name is a refusal rather than
    // a campaign that quietly injected nothing and reported four handled faults.
    if (!a.injectFault.empty()) {
        static const char* kFaults[] = {"host_start_failure", "scenario_load_refusal",
                                        "run_never_ends", "host_dies_mid_run"};
        bool known = false;
        for (const char* f : kFaults) { if (a.injectFault == f) { known = true; break; } }
        if (!known) {
            std::fprintf(stderr, "n8ro-campaign: \"%s\" is not one of the four ugly realities. "
                                 "They are host_start_failure, scenario_load_refusal, "
                                 "run_never_ends and host_dies_mid_run.\n",
                         a.injectFault.c_str());
            return 2;
        }
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
                         + (selfTests ? " (including the self-test's 2 runs)" : "")
                         + (a.hasAxis ? " for a sweep of " + std::to_string(a.axis.values.size())
                                            + " declared value(s)"
                                      : ""));
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
        // CR-DET-1 says "the same configuration twice" and a sweep has many, so the gate runs at
        // ONE declared value - `axis.self_test_value`, defaulting to the first one written. Both
        // self-test runs are copies of this single RunConfig, which is how "two runs at the same
        // parameter value" is guaranteed rather than arranged (CR-PAR-1's third criterion).
        //
        // What it establishes is determinism AT THAT VALUE. It is not nineteen further claims,
        // and every place this result is reported says so.
        if (a.hasAxis) {
            const ext17::param::Value* gateValue = a.axis.selfTestValue();
            if (gateValue == nullptr) {
                std::fprintf(stderr, "n8ro-campaign: the axis names no self-test value\n");
                return 2;
            }
            applyAxisValue(a.axis, *gateValue, stc.run);
            line("campaign", "the determinism gate runs at " + a.axis.name + " = "
                                 + gateValue->text
                                 + (a.axis.units.empty() ? "" : " " + a.axis.units)
                                 + ", which is one of the values this campaign sweeps. It "
                                   "establishes determinism FOR THAT VALUE - the sweep's other "
                                   "values are run, not gated, and the report says so.");
        }
        line("campaign", "gate basis: " + std::string(ext17::compare::name(a.compare.gateBasis))
                             + ". OQ-2 is DECIDED (DRI, 2026-09-01) - content, from the brief's "
                               "own words - and CONCURRED with by the mentor the same day, "
                               "independently. It was still never ANSWERED by the brief's author, "
                               "who has not replied, and criterion 2 is theirs to discharge. Both "
                               "comparisons are run and both are reported; the basis chooses "
                               "which one decides.");
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

    // Runs execute in SWEEP ORDER, so a run's ordinal ascends with its parameter value and the
    // directory listing reads the same way the report does. The ordinals stay ordinals - never
    // the value, and never a timestamp - because two runs at one value must still be
    // addressable as separate runs.
    std::vector<std::size_t> valueOrder;
    if (a.hasAxis) { valueOrder = a.axis.sweepOrder(); }

    for (int n = 0; n < count; ++n) {
        ext17::run::RunConfig cfg = a.run;
        cfg.runId = ordinal(a.firstRun + n);
        cfg.runDir = joinPath(runsDir, cfg.runId);
        if (a.hasAxis) {
            applyAxisValue(a.axis, a.axis.values[valueOrder[static_cast<std::size_t>(n)]], cfg);
        }
        // CR-AS-1: the conditions reach every run. They were validated before the self-test, so
        // by here the file is known good and every run judges against the same seven questions.
        if (a.hasConditions) { cfg.conditions = &a.conditions; }

        // CR-EX-6: inject one of the four ugly realities into the nominated run, and only into
        // that one. The campaign must survive it and carry on, which is the requirement; the
        // injection is named in the run record so the survivor cannot be mistaken for a clean
        // run that happened to work.
        if (!a.injectFault.empty() &&
            (a.injectRun < 0 || a.injectRun == a.firstRun + n)) {
            cfg.injectFault = a.injectFault;
        }

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
                             selfTestRan ? &selfTest : nullptr, a.hasAxis ? &a.axis : nullptr);
        if (a.hasAxis) { printSweep(records, a.axis); }
    }

    const OutcomeCounts counts = printOutcomeSummary(records);
    const int passed = counts.pass;
    const int completed = counts.completedUnjudged;
    const int attempted = static_cast<int>(records.size());

    ext17::log::closeMirror();
    // A campaign stopped at its ceiling did not do what it was asked to do, even if every run it
    // did attempt completed. That is a distinct thing from a failing run and it is named as one
    // in the log; the exit code cannot carry a fifth value, so it carries "not everything asked
    // for happened", which is what 1 means.
    if (ceilingReached) return 1;
    // 0 means every run reached the outcome the campaign was asking for. With conditions
    // declared that is `pass`; without them it is `completed`, because a run nothing judged is
    // not a pass and calling it one here would undo the whole distinction.
    return (passed + completed) == attempted ? 0 : 1;
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
        std::fprintf(stderr, "%s: an exception escaped - %s%s", "n8ro-campaign", e.what(), "\n");
    } catch (...) {
        std::fprintf(stderr, "%s: a non-standard exception escaped%s", "n8ro-campaign", "\n");
    }
    std::fprintf(stderr, "%s: this is a defect in the harness, not a result about anything it "
                         "was asked to read.%s", "n8ro-campaign", "\n");
    return 4;
}
