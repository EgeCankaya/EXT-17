// EXT-17 — one run, start to finish, unattended.
//
// The sequence is [B] step 2 exactly: start the host, wait for it to be ready, load, run,
// detect the end, tear down. Everything else here is one of the four requirements that govern
// it, and each is named at the point it is enforced in RunOnce.cpp.
//
//   CR-EX-1  a fresh host per run, ended by the handle that created it, and a refusal if a
//            host this campaign did not start is already live
//   CR-EX-2  readiness, loaded and started each an observed condition with a bounded timeout,
//            and the recorder attached before the roster burst at scenario load
//   CR-EX-3  the end is the stop predicate, stated and recorded
//   CR-EX-4  a timeout on every run, and a timed-out run is its own outcome
//
// Never throws (constraint C3). Every path out writes a run record.
#pragma once

#include "RunRecord.h"
#include "StopPredicate.h"

#include <cstdint>
#include <functional>
#include <string>

namespace ext17::run {

struct RunConfig {
    // Identity and location. Run ids are zero-padded ordinals, never timestamps: two identical
    // runs must be addressable as a pair (PRD, "File and path conventions").
    std::string runId = "000";
    std::string runDir;

    std::string scenario = "Atacama Air Defense";
    std::string modelName = "N8roSimSchema";
    std::string modelPath = "C:\\N8RO\\data\\db";
    std::string schemaFile = "N8roSimSchema";

    std::string hostExe = "C:\\N8RO\\bin\\n8ro-sim-app.exe";
    std::string recorderExe;
    std::string simConfigHost = "SimEngineHost_SharedMemory";
    std::string simConfigClient = "SimEngineClient_SharedMemory";

    // Measured preconditions, not preferences. See StartSpec in src/proc/Process.h.
    std::string n8roRelease = "C:\\N8RO";
    std::string pathPrepend = "C:\\N8RO\\bin";

    // The host writes its own log to one fixed path inside the read-only install tree, and the
    // next host start renames the previous run's copy as a crash log. Copied out per run so a
    // campaign keeps twenty of them instead of one (measured at M2).
    std::string hostLoggerPath = "C:\\N8RO\\logs\\n8ro-logger-n8ro-sim-app.log";

    StopPredicate predicate = StopPredicate::frameBudget(1200);

    // CR-EX-4: there is no configuration in which a run may run unbounded. This bounds the
    // wait on the stop predicate and nothing else.
    int runTimeoutMs = 600000;

    int readyTimeoutMs = 60000;
    int loadTimeoutMs = 120000;
    int startTimeoutMs = 30000;
    int stopTimeoutMs = 30000;
    int hostExitTimeoutMs = 20000;
    // The recorder detects host loss after 3.0 s of engine-state silence and closes its capture
    // with a well-formed trailer, so it is given room to exit on its own before being ended.
    int recorderExitTimeoutMs = 30000;

    bool attachRecorder = true;
    std::uint64_t captureMaxBytes = 0;      // 0 = unbounded (the recorder's own default)
    std::string onSizeLimit = "stop";       // OQ-6's leading candidate; decided at M3
    // 0 leaves the recorder's own default (8192 records) in place. A small value deliberately
    // overloads the handler-to-writer queue so that `trailer.drops.samples_not_recorded` becomes
    // non-zero — the only way this project can exercise CR-DET-1's exclusion rule against a real
    // capture rather than a hand-written one.
    long long recorderQueueSize = 0;

    bool queryCatalogue = false;
    int catalogueTimeoutMs = 10000;

    // The seam a parameterisation axis acts through. `afterLoadBeforeStart` fires once the
    // scenario reports loaded and before `start` is published; `afterStart` fires once the
    // engine reports running.
    //
    // They exist at M2 for the R9/OQ-4 feasibility spike, which asks whether entity state set
    // between load and start survives materialisation into the run. They are the seam CR-PAR-1
    // will use, and **which axis acts through them is OQ-4 and is not decided here.** Both
    // default to empty, and an empty hook is not called - an ordinary run has no seam in it.
    std::function<void(control::EngineControl&)> afterLoadBeforeStart;
    std::function<void(control::EngineControl&)> afterStart;
};

// Execute one run. Returns the record; the record's outcome is the answer.
RunRecord executeRun(const RunConfig& config);

} // namespace ext17::run
