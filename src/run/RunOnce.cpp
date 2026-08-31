#include "RunOnce.h"

#include "../assert/Judge.h"
#include "../capture/CaptureSet.h"

#include "../common/Log.h"
#include "../proc/Process.h"

#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ext17::run {
namespace {

struct RunState {
    std::optional<proc::Process> recorder;
    std::optional<proc::Process> host;
    std::unique_ptr<control::EngineControl> control;
    // Sticky: `host` is reset during teardown, and the questions asked afterwards are about
    // whether this run ever had a host of its own, not whether it still does.
    bool hostWasStarted = false;
};

std::string joinPath(const std::string& dir, const std::string& leaf) {
    if (dir.empty()) { return leaf; }
    const char last = dir.back();
    return (last == '\\' || last == '/') ? dir + leaf : dir + "\\" + leaf;
}

// Watches a capture go past and records which of a named set of entities carried a sample.
//
// It exists because of the gap between publishing and receiving. `sendEntityUpdate` returns
// true when the message reached the bus; whether an entity of that name was there to be updated
// is a different question, and the only file that answers it is the run's own capture. A
// mistyped entity name would otherwise produce a sweep in which every run is the baseline and
// nothing says so.
//
// It reads `entity` from the record and nothing else, so it costs one string compare per sample
// against a small set and retains nothing.
class ParameterEntitySink final : public capture::RecordSink {
public:
    explicit ParameterEntitySink(const std::vector<std::string>& wanted) {
        for (const std::string& e : wanted) { seen_.emplace_back(e, false); }
    }

    void onRecord(const capture::RecordView& view) override {
        if (view.type != "sample" || !view.record) { return; }
        const json::Value* entity = view.record->find("entity");
        if (!entity || !entity->isString()) { return; }
        for (auto& kv : seen_) {
            if (!kv.second && kv.first == entity->text()) { kv.second = true; return; }
        }
    }

    [[nodiscard]] std::vector<std::string> missing() const {
        std::vector<std::string> out;
        for (const auto& kv : seen_) {
            if (!kv.second) { out.push_back(kv.first); }
        }
        return out;
    }

private:
    // A vector of pairs rather than a map, and the reason is CR-DET-2: an unordered container
    // iterated is one of [B]'s three named hazards, and `tools/n8ro-compare/build.cmd` fails a
    // build that names one. Declaration order out is the order the campaign file declared.
    std::vector<std::pair<std::string, bool>> seen_;
};

RunOutcome fail(RunRecord& rec, const char* stage, const std::string& detail) {
    rec.errorStage = stage;
    rec.errorDetail = detail;
    log::line("FAIL", std::string(stage) + ": " + detail);
    return RunOutcome::InfrastructureError;
}

// The body of a run. Every exit is an outcome; teardown and the record are the caller's, so
// that a failure at any stage still tears down and still writes a record.
RunOutcome runBody(const RunConfig& cfg, RunRecord& rec, RunState& st) {
    // The run directory comes first, before anything that can fail. Every path out of this
    // function writes a run record, and CR-EX-4's counting invariant - that the outcomes sum to
    // the runs attempted - only holds if a run that never started still leaves one behind.
    const std::string hostWorkDir = joinPath(cfg.runDir, "host");
    if (!proc::createDirectories(cfg.runDir) || !proc::createDirectories(hostWorkDir)) {
        return fail(rec, "preflight", "could not create the run directory " + cfg.runDir);
    }

    // ---- CR-EX-1 pre-flight ------------------------------------------------------------
    // Detection by image name, deliberately: this is the one question a handle cannot answer,
    // and it is the question CR-EX-1's third criterion asks. Termination stays handle-only.
    const std::string hostImage = proc::baseName(cfg.hostExe);
    if (const auto existing = proc::findProcessesByImageName(hostImage); !existing.empty()) {
        std::string pids;
        for (const auto p : existing) { pids += (pids.empty() ? "" : ", ") + std::to_string(p); }
        return fail(rec, "preflight",
                    "a host process this campaign did not create is already live (" + hostImage
                        + " pid " + pids + "). Refusing to start: a shared host produces one "
                        "usable run and nineteen recordings of orphaned samples.");
    }

    // ---- The recorder, first ------------------------------------------------------------
    // PROVENANCE finding 7, as M1 sharpened it: the deadline is the *scenario load*, not the
    // host start, because the entity_created burst that fills the roster is published once, at
    // load. Starting the recorder before the host clears that deadline with room to spare and
    // costs nothing - the recorder tolerates any order and waits for the host itself.
    if (cfg.attachRecorder) {
        if (cfg.recorderExe.empty()) {
            return fail(rec, "recorder_start",
                        "--recorder was not given and --no-recorder was not set");
        }
        proc::StartSpec spec;
        spec.exePath = cfg.recorderExe;
        spec.args = {"--config", cfg.simConfigClient,
                     "--model-path", cfg.modelPath,
                     "--schema-file", cfg.schemaFile,
                     "--out-dir", cfg.runDir,
                     "--run-label", cfg.runId};
        if (cfg.captureMaxBytes > 0) {
            spec.args.push_back("--capture-max-bytes");
            spec.args.push_back(std::to_string(cfg.captureMaxBytes));
            spec.args.push_back("--on-size-limit");
            spec.args.push_back(cfg.onSizeLimit);
        }
        // Passed through only when asked for, so the recorder's own default stands otherwise.
        // A deliberately small queue is how CR-DET-1's `samples_not_recorded` exclusion is made
        // reachable: at the recorder's default nothing this project has run has ever dropped a
        // sample, and an exclusion rule that has never fired is a rule nobody has tested.
        if (cfg.recorderQueueSize > 0) {
            spec.args.push_back("--queue-size");
            spec.args.push_back(std::to_string(cfg.recorderQueueSize));
        }
        spec.workingDirectory = cfg.runDir;
        spec.environment = {{"N8RO_RELEASE", cfg.n8roRelease}};
        spec.pathPrepend = cfg.pathPrepend;
        spec.stdoutPath = joinPath(cfg.runDir, "recorder.out");
        spec.stderrPath = joinPath(cfg.runDir, "recorder.err");

        std::string error;
        st.recorder = proc::Process::start(spec, error);
        if (!st.recorder) {
            return fail(rec, "recorder_start", error);
        }
        log::line("start", "recorder pid " + std::to_string(st.recorder->pid()) + " -> "
                               + cfg.runDir);
    }

    // ---- The client -----------------------------------------------------------------------
    // Built before the host, for the same reason the recorder is: the client is a bus
    // participant that tolerates an absent host, and building it first means the readiness
    // wait below is a genuine observation of the host's first publication rather than a race
    // against the shared-memory segment appearing.
    {
        control::EngineControlConfig ccfg;
        ccfg.simConfig = cfg.simConfigClient;
        ccfg.modelPath = cfg.modelPath;
        ccfg.schemaFile = cfg.schemaFile;
        ccfg.senderId = "ext17-campaign-" + cfg.runId;
        std::string error;
        st.control = control::EngineControl::create(ccfg, error);
        if (!st.control) {
            return fail(rec, "client_create", error);
        }
        log::line("connect", "client created, message pump started");
    }

    // ---- The host ---------------------------------------------------------------------------
    // The host appends to one shared log inside the read-only install tree, so this run's
    // portion of it starts wherever the file ends now. Measured at M2: without this offset,
    // run N's copy carries runs 0..N as well, which is another run's state crossing into this
    // one - the platform's doing rather than ours, but evidence all the same.
    rec.hostLoggerOffset = proc::fileSizeBytes(cfg.hostLoggerPath).value_or(0);
    {
        proc::StartSpec spec;
        spec.exePath = cfg.hostExe;
        // CR-EX-6, fault 1 of 4: **a host that fails to start.** Injected by pointing the start
        // at a path that is not an executable, which is what a bad --host-exe, a half-installed
        // runtime and a corrupted binary all look like from here. The failure is a real one -
        // CreateProcess genuinely refuses - rather than a flag this function checks and
        // pretends about.
        if (cfg.injectFault == "host_start_failure") {
            spec.exePath = joinPath(cfg.runDir, "no-such-host.exe");
            log::line("inject", "fault host_start_failure: the host executable is "
                                + spec.exePath + ", which does not exist");
        }
        spec.args = {"--sim-config", cfg.simConfigHost,
                     "--model-path", cfg.modelPath,
                     "--schema-file", cfg.schemaFile};
        // The host creates data/db/ and logs/ in its working directory, so it gets its own.
        spec.workingDirectory = hostWorkDir;
        // Without N8RO_RELEASE the host resolves its plugin directory from the working
        // directory, skips the plugin scan, never registers componentPhysics, and refuses every
        // 42-entity scenario load - while sitting idle rather than failing (M1 7a).
        spec.environment = {{"N8RO_RELEASE", cfg.n8roRelease}};
        spec.pathPrepend = cfg.pathPrepend;
        spec.stdoutPath = joinPath(cfg.runDir, "host.out");
        spec.stderrPath = joinPath(cfg.runDir, "host.err");

        std::string error;
        st.host = proc::Process::start(spec, error);
        if (!st.host) {
            return fail(rec, "host_start", error);
        }
        st.hostWasStarted = true;
        log::line("start", "host pid " + std::to_string(st.host->pid()) + " cwd " + hostWorkDir);
    }

    // ---- CR-EX-2: readiness, loaded, started - each an observed condition ------------------
    {
        const auto w = st.control->waitForHostReady(cfg.readyTimeoutMs);
        rec.waits.push_back(w);
        if (!w.observed) {
            return fail(rec, "host_ready",
                        "the host published no engine state within "
                            + std::to_string(cfg.readyTimeoutMs) + " ms");
        }
    }

    if (cfg.queryCatalogue) {
        if (st.control->requestScenarioList(cfg.modelName)) {
            const auto w = st.control->waitForScenarioList(cfg.catalogueTimeoutMs);
            rec.waits.push_back(w);
            log::line("catalogue", "scenarios in " + cfg.modelName + ": "
                                       + std::to_string(st.control->lastScenarioList().size()));
        }
    }

    // CR-EX-6, fault 2 of 4: **a scenario that refuses to load.** Injected by asking for a
    // scenario name the catalogue does not contain - M5 measured the catalogue answering with 10
    // names in 119 ms, and this is not one of them. The refusal is the platform's own and takes
    // its real shape: the host does not fail, it sits idle, and what times out is our wait for
    // `loaded`. That idling-rather-than-failing shape is F-5's, and it is the reason this fault
    // is worth injecting rather than reasoning about.
    std::string scenarioToLoad = cfg.scenario;
    if (cfg.injectFault == "scenario_load_refusal") {
        scenarioToLoad = "No Such Scenario (EXT-17 fault injection)";
        log::line("inject", "fault scenario_load_refusal: asking for \"" + scenarioToLoad
                                + "\", which the catalogue does not contain");
    }
    if (!st.control->publishLoadScenario(scenarioToLoad, cfg.modelName)) {
        return fail(rec, "scenario_load", "load_scenario was not published");
    }
    {
        const auto w = st.control->waitForScenarioLoaded(cfg.loadTimeoutMs);
        rec.waits.push_back(w);
        if (!w.observed) {
            return fail(rec, "scenario_load",
                        "the scenario \"" + scenarioToLoad + "\" did not load within "
                            + std::to_string(cfg.loadTimeoutMs)
                            + " ms. A load refused for a missing component factory leaves the "
                              "host idle rather than failing - check N8RO_RELEASE and host.err.");
        }
    }

    if (cfg.afterLoadBeforeStart) {
        log::line("axis", "applying the after-load, before-start hook");
        cfg.afterLoadBeforeStart(*st.control);
    }

    if (!st.control->publishStart()) {
        return fail(rec, "engine_start", "start was not published");
    }
    {
        const auto w = st.control->waitForRunning(cfg.startTimeoutMs);
        rec.waits.push_back(w);
        if (!w.observed) {
            return fail(rec, "engine_start",
                        "the engine did not report running within "
                            + std::to_string(cfg.startTimeoutMs) + " ms");
        }
    }

    if (cfg.afterStart) {
        log::line("axis", "applying the after-start hook");
        cfg.afterStart(*st.control);
    }

    // ---- CR-EX-3 and CR-EX-4: the end, and the backstop -------------------------------------
    // The predicate is the definition of the end; the run timeout is the bound on waiting for
    // it, and nothing else. Both are here, in one call, so that the difference between them is
    // structural: satisfying the predicate is `observed`, exhausting the timeout is not.
    //
    // The evaluation is captured *inside* the condition, on the first publication at which the
    // predicate holds. Reading it afterwards would report a later frame, and "twenty runs end
    // at the same point by the predicate's own measure" is precisely a claim about that number.
    // CR-EX-6, fault 3 of 4: **a run that never ends.** Injected by setting the predicate beyond
    // anything the run timeout allows, which is what a mis-set frame budget, a scenario that
    // stalls and a paused engine all present as. The important property is what it must produce:
    // `timeout`, its OWN outcome, and never `fail` - a run that did not finish has not told you
    // anything about the scenario. This is also the only fault that exercises CR-EX-4's backstop
    // on a real run rather than by construction.
    StopPredicate predicate = cfg.predicate;
    int runTimeoutMs = cfg.runTimeoutMs;
    if (cfg.injectFault == "run_never_ends") {
        predicate = StopPredicate::frameBudget(1000000000);
        runTimeoutMs = cfg.injectFaultAtMs;
        log::line("inject", "fault run_never_ends: the stop predicate is now "
                                + predicate.statement() + ", which this run will not reach, and "
                                  "the run timeout is " + std::to_string(runTimeoutMs) + " ms");
    }

    // CR-EX-6, fault 4 of 4: **a host that dies mid-run.** Injected by terminating the host
    // through THE HANDLE THIS RUN CREATED - never by image name, which is the security posture's
    // rule and CR-EX-1's. The run then meets exactly what an unattended campaign meets when a
    // host crashes at 3 a.m.: the engine-state heartbeat stops, the wait for the predicate runs
    // out, and the recorder closes its capture on its own after 3.0 s of silence.
    //
    // It fires at a FRAME, not at a wall-clock delay: the frame is what this project measures
    // runs in, and a fault injected on a stopwatch would land in a different place every time -
    // which is the one thing an injected fault must not do if the record of it is to mean
    // anything. `injectFaultAtFrame` is a quarter of the budget by default.
    const long long dieAtFrame =
        cfg.injectFault == "host_dies_mid_run"
            ? static_cast<long long>(cfg.predicate.frameBudgetValue() / 4) + 1
            : -1;
    bool hostKilled = false;
    if (dieAtFrame > 0) {
        log::line("inject", "fault host_dies_mid_run: the host will be terminated by the handle "
                            "this run created - never by image name - at frame "
                                + std::to_string(dieAtFrame));
        // **A dead host is not noticed until the run timeout expires**, and that is a real
        // property of this design rather than an artifact of the injection. The wait blocks on
        // engine-state publications; when the host goes away they simply stop arriving, and
        // there is no second timed quantity watching for silence - deliberately, because
        // CR-EX-4 makes the run timeout the ONLY clock in a run and a heartbeat-silence
        // detector would be a second one. So an unattended campaign meeting [B]'s fourth ugly
        // reality survives it and pays --run-timeout-ms in wall clock for each occurrence.
        // Recorded as F-27; the README's operational note is to size --run-timeout-ms against
        // the frame budget rather than leaving it at ten minutes.
        //
        // The injected run shortens it so the demonstration takes seconds instead of minutes.
        // The shortening is the injection's, not the product's.
        runTimeoutMs = cfg.injectFaultAtMs;
    }

    log::line("run", "watching for: " + predicate.statement());
    {
        StopEvaluation firstSatisfaction;
        bool captured = false;
        const auto w = st.control->waitFor(
            "stop predicate satisfied",
            [&](const control::EngineSnapshot& s) {
                if (dieAtFrame > 0 && !hostKilled &&
                    s.frame >= static_cast<std::uint64_t>(dieAtFrame)) {
                    hostKilled = true;
                    log::line("inject", "terminating the host at frame "
                                            + std::to_string(s.frame));
                    if (st.host) { st.host->terminate(); }
                }
                if (!predicate.satisfiedBy(s)) { return false; }
                if (!captured) {
                    firstSatisfaction = predicate.evaluate(s);
                    captured = true;
                }
                return true;
            },
            runTimeoutMs);
        rec.waits.push_back(w);
        rec.evaluation = firstSatisfaction;
        rec.predicateSatisfied = captured;

        if (!w.observed) {
            rec.runTimeoutExpired = true;
            rec.evaluation = predicate.evaluate(w.at);
            rec.evaluation.satisfied = false;

            // A host that died is an INFRASTRUCTURE failure, not a timeout. The two look
            // identical from the wait - both are "the predicate was not satisfied in time" - and
            // telling them apart is exactly what CR-EX-5 requires, because one says the harness
            // broke and the other says the run was too slow. The host's own process answers it.
            if (st.host && !st.host->isAlive()) {
                return fail(rec, "host_died",
                            "the host process exited during the run, before the stop predicate "
                            "was satisfied. The engine-state heartbeat stopped and the wait ran "
                            "out. This is an infrastructure failure and not a timeout: the run "
                            "was not too slow, the thing running it went away.");
            }
            log::line("TIMEOUT", "the run timeout expired before the stop predicate was "
                                 "satisfied; this run is a timeout, not a failure");
            return RunOutcome::Timeout;
        }
    }

    // ---- Stop -------------------------------------------------------------------------------
    // Stop rewinds the simulation clock to zero and reloads the scenario from source, which is
    // what puts a second segment into every capture (PROVENANCE finding 3). Waiting for the
    // engine to leave `running` is the observation that the stop landed.
    if (!st.control->publishStop()) {
        return fail(rec, "engine_stop", "stop was not published");
    }
    {
        const auto w = st.control->waitForIdle(cfg.stopTimeoutMs);
        rec.waits.push_back(w);
        if (!w.observed) {
            return fail(rec, "engine_stop",
                        "the engine did not leave the running state within "
                            + std::to_string(cfg.stopTimeoutMs) + " ms");
        }
    }

    return RunOutcome::Completed;
}

void recordProcess(RunRecord& rec, const char* role, const std::string& exePath,
                   std::optional<proc::Process>& process, int exitTimeoutMs,
                   bool requestGraceful) {
    if (!process) { return; }
    ProcessRecord pr;
    pr.role = role;
    pr.exePath = exePath;
    pr.pid = process->pid();
    pr.startedByUs = true;

    if (requestGraceful && process->isAlive()) {
        pr.gracefulStopRequested = process->requestStopGracefully();
    }

    const bool exited = process->waitFor(exitTimeoutMs);
    if (exited) {
        pr.exitedOnItsOwn = true;
    } else {
        // CR-EX-1: by the handle this campaign created, never by image name.
        log::line("teardown", std::string(role) + " pid " + std::to_string(pr.pid)
                                  + " did not exit within " + std::to_string(exitTimeoutMs)
                                  + " ms; terminating by handle");
        process->terminate();
        pr.terminatedByHandle = true;
    }
    pr.exitCode = process->exitCode();
    log::line("teardown", std::string(role) + " pid " + std::to_string(pr.pid) + " exit "
                              + (pr.exitCode ? std::to_string(*pr.exitCode) : std::string("?"))
                              + (pr.exitedOnItsOwn ? " (on its own)" : " (by handle)"));
    process->close();
    process.reset();
    rec.processes.push_back(pr);
}

} // namespace

RunRecord executeRun(const RunConfig& cfg) {
    RunRecord rec;
    rec.runId = cfg.runId;
    rec.scenario = cfg.scenario;
    rec.modelName = cfg.modelName;
    rec.predicate = cfg.predicate;
    rec.runTimeoutMs = cfg.runTimeoutMs;
    rec.recorderAttached = cfg.attachRecorder;
    rec.captureMaxBytes = cfg.captureMaxBytes;
    rec.onSizeLimit = cfg.attachRecorder && cfg.captureMaxBytes > 0 ? cfg.onSizeLimit
                                                                    : std::string("n/a");
    rec.hostExe = cfg.hostExe;
    rec.recorderExe = cfg.attachRecorder ? cfg.recorderExe : std::string("");
    rec.simConfigHost = cfg.simConfigHost;
    rec.simConfigClient = cfg.simConfigClient;
    rec.modelPath = cfg.modelPath;
    rec.schemaFile = cfg.schemaFile;
    rec.n8roRelease = cfg.n8roRelease;
    rec.pathPrepend = cfg.pathPrepend;
    // CR-PAR-1. Set before runBody so that a run which never starts still records what it was
    // asked to vary - a sweep's missing point is then attributable rather than merely absent.
    // Every named entity is assumed missing until the capture shows otherwise.
    rec.parameterName = cfg.parameterName;
    rec.parameterValueText = cfg.parameterValueText;
    rec.parameterAppliesTo = cfg.parameterAppliesTo;
    rec.parameterUnits = cfg.parameterUnits;
    rec.parameterEntities = cfg.parameterEntities;
    rec.parameterEntitiesMissing = cfg.parameterEntities;
    // CR-EX-6: recorded BEFORE runBody, so that a fault which stops the run before anything else
    // happens still leaves a record saying the run was injected. Found by running the four
    // injections and reading run.json: the field was set nowhere, so every injected run looked
    // exactly like a clean one that had happened to fail.
    rec.injectedFault = cfg.injectFault;

    log::line("run", "=== run " + cfg.runId + " : \"" + cfg.scenario + "\""
                         + (cfg.parameterName.empty()
                                ? std::string()
                                : "  [" + cfg.parameterName + " = " + cfg.parameterValueText
                                      + (cfg.parameterUnits.empty() ? "" : " " + cfg.parameterUnits)
                                      + "]")
                         + " ===");

    RunState st;
    rec.outcome = runBody(cfg, rec, st);

    // ---- Teardown, on every path out ----------------------------------------------------
    if (st.control) {
        rec.finalSnapshot = st.control->snapshot();
        // Release the bus client before the host goes away, so the pump is not shutting down
        // against a dead segment.
        st.control.reset();
    }

    recordProcess(rec, "host", cfg.hostExe, st.host, cfg.hostExitTimeoutMs, true);

    // The recorder exits on its own once the engine-state heartbeat has been silent for 3.0 s,
    // closing the capture with a well-formed trailer and end_reason "host_lost". Giving it that
    // window is what makes the capture readable; killing it first would truncate the file.
    recordProcess(rec, "recorder", cfg.recorderExe, st.recorder, cfg.recorderExitTimeoutMs, false);

    // The host's own log lives at one fixed path in the read-only install tree and the next
    // host start renames it. Copy it out so a campaign keeps one per run.
    // Only if this run started a host. A run that refused at pre-flight has no log of its own,
    // and the file it would copy belongs to somebody else's host.
    if (st.hostWasStarted && proc::fileExists(cfg.hostLoggerPath)) {
        const std::string dest = joinPath(cfg.runDir, "host-logger.log");
        const auto copied = proc::copyFileTailFrom(cfg.hostLoggerPath, dest, rec.hostLoggerOffset);
        if (copied) {
            rec.hostLoggerCopy = "host-logger.log";
            rec.hostLoggerBytes = *copied;
        } else {
            log::line("teardown", "could not copy the host log from " + cfg.hostLoggerPath);
        }
    }

    // Find the capture by extension rather than by reproducing the recorder's naming rule,
    // which is EXT-08's and not something this project may depend on.
    if (cfg.attachRecorder) {
        const auto captures = proc::listFilesWithSuffix(cfg.runDir, ".n8rocap.jsonl");
        if (!captures.empty()) {
            // Every part, not only the first. A run recorded with --on-size-limit rotate is a
            // set of files whose segment ordinals restart in each one, and a record that named
            // one of them would send M4 and M6 to a fraction of the run without saying so.
            rec.captureParts = captures;
            rec.capturePath = captures.front();
            rec.captureBytes = proc::fileSizeBytes(joinPath(cfg.runDir, captures.front()));
            std::uint64_t total = 0;
            for (const std::string& part : captures) {
                total += proc::fileSizeBytes(joinPath(cfg.runDir, part)).value_or(0);
            }
            rec.captureTotalBytes = total;
            if (captures.size() > 1) {
                log::line("capture", std::to_string(captures.size())
                                         + " capture files in the run directory; the recorder "
                                           "rotated. Segment ordinals restart in every part.");
            }

            // Read the capture back with this project's own conformant reader, immediately, on
            // the run that produced it. It links nothing and needs no install, so the campaign
            // pays a quarter of a second per run for two answers it cannot get any other way:
            // whether the file it just wrote is well formed, and whether it covers the run.
            // ...and, when the run was parameterised, collect which of the axis's named
            // entities actually carried a sample. A publish returning true says the message
            // reached the bus and nothing more; this is the only thing that says the entity was
            // there to receive it (CR-PAR-1, and tenet 3 applied to our own input).
            ParameterEntitySink paramSink(cfg.parameterEntities);
            const capture::SetResult set = capture::readSet(
                joinPath(cfg.runDir, captures.front()), {},
                cfg.parameterEntities.empty() ? nullptr : &paramSink);
            if (!set.parts.empty()) {
                rec.captureRead = true;
                const capture::ReadResult& last = set.parts.back();
                rec.captureEndReason = last.hasTrailer ? last.trailer.endReason
                                                       : std::string("(no trailer - truncated)");
                // Format 6.7's own rule for telling a last part from a stopped one: a part
                // carrying size_limit with no continued_in is a run that stopped at its limit.
                rec.captureCoversWholeRun =
                    last.hasTrailer && last.trailer.endReason != "size_limit";
                rec.captureConformant = set.conformant();
                rec.captureSamples = set.counts.samples;
                rec.captureSegmentKeys = static_cast<long long>(set.segments.size());
                rec.captureRunSegments = set.runSegments;
                std::string diags;
                for (const auto& kv : set.diagnosticCounts) {
                    diags += (diags.empty() ? "" : ", ") + std::string(capture::name(kv.first))
                             + " x" + std::to_string(kv.second);
                }
                for (const capture::ReadResult& part : set.parts) {
                    if (part.rejected) {
                        diags += (diags.empty() ? "" : ", ") + std::string("REJECTED ")
                                 + capture::name(part.rejectCode);
                    }
                    for (const auto& kv : part.diagnosticCounts) {
                        diags += (diags.empty() ? "" : ", ")
                                 + std::string(capture::name(kv.first)) + " x"
                                 + std::to_string(kv.second);
                    }
                }
                rec.captureDiagnostics = diags;

                // The first RUNNING segment - the run proper. `running` is the reader's
                // three-valued clock class, and taking the first one rather than segment 0 by
                // ordinal means a capture whose segment 0 froze (R12) contributes nothing here
                // rather than contributing a number computed over a segment the determinism
                // gate would refuse.
                for (const capture::SegmentStats& seg : set.segments) {
                    if (seg.clock != capture::ClockClass::Running) { continue; }
                    rec.captureRunSegmentFound = true;
                    rec.captureEntityKeys = seg.distinctEntityKeys;
                    rec.captureEntityAdds = seg.entityAdds;
                    break;
                }
                rec.parameterEntitiesMissing = paramSink.missing();
                if (!rec.parameterEntitiesMissing.empty()) {
                    std::string names;
                    for (const std::string& e : rec.parameterEntitiesMissing) {
                        names += (names.empty() ? "" : ", ") + e;
                    }
                    log::line("axis", "the axis named " + std::to_string(names.empty() ? 0
                                          : rec.parameterEntitiesMissing.size())
                                          + " entity(ies) that carry no sample in this run's "
                                            "capture: " + names
                                          + ". The update was published and nothing received it.");
                }
                log::line("capture", "read back: " + std::to_string(set.counts.samples)
                                         + " samples over " + std::to_string(set.segments.size())
                                         + " segment key(s) for "
                                         + std::to_string(set.runSegments)
                                         + " run segment(s), end_reason "
                                         + rec.captureEndReason + ", "
                                         + (rec.captureConformant ? "conformant"
                                                                  : "NOT conformant (" + diags + ")"));
                if (!rec.captureCoversWholeRun) {
                    // Not an error: a bounded capture that stops is exactly what the operator
                    // asked --on-size-limit stop for. It is a caveat on everything computed from
                    // the file afterwards, so it is said once, loudly, and written into the
                    // record rather than left for whoever opens the file in a month.
                    log::line("capture", "this capture does NOT cover the whole run - it ended at "
                                         "its size limit. Anything computed from it is over the "
                                         "part of the run that was recorded, not the run.");
                }
            }
        } else {
            // A run that was asked to record and recorded nothing is an infrastructure error,
            // not a completed run. Found at M3 by a probe whose recorder refused to start: the
            // run executed to its stop predicate, the campaign called it `completed`, and there
            // was no capture in the directory at all. Tenet 1 - a wrong number is worse than no
            // number - and CR-EX-5: infrastructure is never a test result. A run reported as
            // completed is a run M6 will later judge, and it would be judging nothing.
            //
            // Only where the run had otherwise succeeded. A run that already failed keeps the
            // fault it actually had, since the first failure is the informative one.
            log::line("capture", "no capture file in " + cfg.runDir);
            if (rec.outcome == RunOutcome::Completed) {
                rec.outcome = fail(rec, "capture",
                                   "the recorder was attached and no .n8rocap.jsonl file was "
                                   "written into " + cfg.runDir
                                       + ". The run reached its stop predicate, but it recorded "
                                         "nothing and cannot be judged or compared later.");
            }
        }
    }

    // --- The judgement (CR-AS-1..4, CR-CAP-1, CR-EX-5) ---------------------------------------
    //
    // Over the STORED CAPTURE, after teardown, through `src/assert/`'s evaluator - the same one
    // `n8ro-judge` runs. There is deliberately no second, "live" code path: a verdict produced
    // while a host was running would be a verdict a re-judgement could disagree with, and
    // CR-CAP-1 asks that they be identical. One evaluator over one file makes that structural.
    //
    // A run that already failed for an infrastructure reason is NOT judged. Judging it would
    // produce verdicts over a capture of something that did not happen, and CR-EX-5's whole
    // point is that a harness failure is never a test result.
    if (cfg.conditions && !cfg.conditions->conditions.empty()) {
        rec.judged = true;
        rec.conditionsPath = cfg.conditions->path;
        rec.conditionsDeclared = static_cast<long long>(cfg.conditions->conditions.size());

        if (rec.outcome != RunOutcome::Completed) {
            rec.notJudgedReason =
                "this run's outcome is " + std::string(toString(rec.outcome))
                + ", and an infrastructure failure or a timeout is never a test result "
                  "(CR-EX-5). No verdict is invented for it, so this run carries 0 verdicts "
                  "against " + std::to_string(rec.conditionsDeclared)
                + " declared conditions - which is what tells a reader the run was cut short "
                  "rather than that it passed.";
            log::line("judge", "not judged: " + rec.notJudgedReason);
        } else if (rec.capturePath.empty()) {
            rec.notJudgedReason = "there is no capture to judge.";
            log::line("judge", "not judged: " + rec.notJudgedReason);
        } else {
            rec.judgedThisRun = true;
            assertion::JudgeResult jr;
            assertion::judgeCapture(joinPath(cfg.runDir, rec.capturePath), *cfg.conditions, jr);

            rec.verdictsMet = jr.met;
            rec.verdictsNotMet = jr.notMet;
            rec.verdictsIndeterminate = jr.indeterminate;
            rec.verdictsSatisfied = jr.satisfied;
            rec.verdictsViolated = jr.violated;
            rec.verdictsUndetermined = jr.undetermined;
            rec.judgeable = jr.judgeable;
            rec.notJudgeableReason = jr.notJudgeableReason;
            for (const auto& v : jr.verdicts) {
                rec.verdictConditionIds.push_back(v.conditionId);
                rec.verdictStates.push_back(assertion::toString(v.state));
                rec.verdictOutcomes.push_back(assertion::toString(v.outcome));
                rec.verdictTextLines.push_back(assertion::verdictLine(v));
                rec.verdictJsonLines.push_back(assertion::verdictJson(v));
            }

            // CR-EX-5's mapping, and the two cases that are not test results.
            if (jr.rejected || !jr.conformant) {
                rec.outcome = fail(rec, "judge",
                                   "the capture could not be judged: "
                                       + (jr.rejected ? jr.rejectReason
                                                      : std::string("it is not conformant")));
            } else if (!jr.judgeable) {
                // R14's shape. Not a determinism failure, and certainly not a failing scenario.
                rec.outcome = fail(rec, "judge", jr.notJudgeableReason);
            } else if (jr.violated > 0) {
                rec.outcome = RunOutcome::Fail;
            } else if (jr.satisfied == 0) {
                rec.outcome = fail(rec, "judge",
                                   "every verdict was indeterminate, so nothing was decided. "
                                   "Reporting that as a pass is the \"all passed having checked "
                                   "nothing\" failure CR-AS-1 exists to prevent, turned on the "
                                   "verdicts instead of on the loader.");
            } else {
                rec.outcome = RunOutcome::Pass;
            }

            // One verdict file per run, one JSON object per line. This is what CR-CAP-1's
            // identity check compares byte for byte against a later re-judgement.
            const std::string verdictPath = joinPath(cfg.runDir, "verdicts.jsonl");
            if (std::FILE* f = std::fopen(verdictPath.c_str(), "wb")) {
                for (const std::string& l : rec.verdictJsonLines) {
                    std::fwrite(l.data(), 1, l.size(), f);
                    std::fwrite("\n", 1, 1, f);
                }
                std::fclose(f);
            } else {
                log::line("judge", "could not write " + verdictPath);
            }

            for (const std::string& l : rec.verdictTextLines) { log::line("verdict", l); }
            log::line("judge", "satisfied " + std::to_string(jr.satisfied)
                                   + ", violated " + std::to_string(jr.violated)
                                   + ", indeterminate " + std::to_string(jr.indeterminate)
                                   + "  (met " + std::to_string(jr.met) + ", not met "
                                   + std::to_string(jr.notMet) + ")");
        }
    }

    writeRunRecord(rec, joinPath(cfg.runDir, "run.json"));
    log::line("run", "run " + cfg.runId + " -> " + toString(rec.outcome));
    return rec;
}

} // namespace ext17::run
