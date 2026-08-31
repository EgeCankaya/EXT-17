#include "RunOnce.h"

#include "../common/Log.h"
#include "../proc/Process.h"

#include <memory>
#include <optional>

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

    if (!st.control->publishLoadScenario(cfg.scenario, cfg.modelName)) {
        return fail(rec, "scenario_load", "load_scenario was not published");
    }
    {
        const auto w = st.control->waitForScenarioLoaded(cfg.loadTimeoutMs);
        rec.waits.push_back(w);
        if (!w.observed) {
            return fail(rec, "scenario_load",
                        "the scenario \"" + cfg.scenario + "\" did not load within "
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
    log::line("run", "watching for: " + cfg.predicate.statement());
    {
        StopEvaluation firstSatisfaction;
        bool captured = false;
        const auto w = st.control->waitFor(
            "stop predicate satisfied",
            [&](const control::EngineSnapshot& s) {
                if (!cfg.predicate.satisfiedBy(s)) { return false; }
                if (!captured) {
                    firstSatisfaction = cfg.predicate.evaluate(s);
                    captured = true;
                }
                return true;
            },
            cfg.runTimeoutMs);
        rec.waits.push_back(w);
        rec.evaluation = firstSatisfaction;
        rec.predicateSatisfied = captured;

        if (!w.observed) {
            rec.runTimeoutExpired = true;
            rec.evaluation = cfg.predicate.evaluate(w.at);
            rec.evaluation.satisfied = false;
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

    log::line("run", "=== run " + cfg.runId + " : \"" + cfg.scenario + "\" ===");

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
            rec.capturePath = captures.front();
            rec.captureBytes = proc::fileSizeBytes(joinPath(cfg.runDir, captures.front()));
            if (captures.size() > 1) {
                log::line("capture", std::to_string(captures.size())
                                         + " capture files in the run directory; the recorder "
                                           "rotated. Segment ordinals restart in every part.");
            }
        } else {
            log::line("capture", "no capture file in " + cfg.runDir);
        }
    }

    writeRunRecord(rec, joinPath(cfg.runDir, "run.json"));
    log::line("run", "run " + cfg.runId + " -> " + toString(rec.outcome));
    return rec;
}

} // namespace ext17::run
