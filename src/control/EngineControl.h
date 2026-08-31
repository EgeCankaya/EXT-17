// EXT-17 — the control path: bring the host up to a running scenario, and back down.
//
// This is the one place EXT-17 links the N8RO SDK (PRD, "Linkage boundary"). It reads no
// EXT-08 source and names no EXT-08 identifier; the host and the recorder are processes.
//
// CR-EX-2's sharpest criterion is that "a code search for sleep primitives in that path returns
// nothing", so every wait here is **event-driven, not polled**. The client's engine-state
// subscription notifies a condition variable, and each wait is a `wait_until` on a bounded
// deadline with the observed condition as its predicate. Three consequences worth stating:
//
//   - There is no poll interval to tune, and no fixed delay anywhere. The deadline is the
//     bounded, logged timeout CR-EX-2 requires each wait to have, and nothing else.
//   - `sim/engine/state` is a continuous heartbeat while the host lives (the recorder declares
//     host loss after 3.0 s of silence), so a predicate is re-evaluated at frame rate. A
//     condition that becomes true between two heartbeats is observed on the next one.
//   - If the host dies the notifications stop, the wait reaches its deadline, and the run
//     becomes an infrastructure_error. That is a timeout on our own wait, not a conclusion
//     drawn from a missing record — the distinction PROVENANCE finding 5 makes load-bearing.
//
// Never throws (constraint C3): every operation is a bool or an outcome struct plus a log line.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ext17::control {

// The whole observable surface a wait may build on: one engine-state publication, as the
// client sees it. Nothing here is a wall-clock quantity.
struct EngineSnapshot {
    std::string state;
    bool running = false;
    bool scenarioLoaded = false;
    std::string scenarioName;
    std::uint64_t frame = 0;
    double simTimeS = 0.0;
    double deltaS = 0.0;

    // The single-line form M1 used, kept identical so the two transcripts compare by eye.
    [[nodiscard]] std::string line() const;
};

struct WaitOutcome {
    std::string what;
    bool observed = false;
    int timeoutMs = 0;
    // Diagnostic only. This is a wall-clock measurement and it never participates in any
    // decision, verdict or comparison — see the run record's "diagnostics" object.
    std::int64_t elapsedMs = 0;
    EngineSnapshot at;
};

struct EngineControlConfig {
    std::string simConfig = "SimEngineClient_SharedMemory";
    std::string modelPath = "C:\\N8RO\\data\\db";
    std::string schemaFile = "N8roSimSchema";
    std::string senderId = "ext17-campaign";
};

class EngineControl {
public:
    // Heap-allocated because the engine-state handler captures `this`. Returns nullptr and
    // fills `error` if the client could not be built.
    static std::unique_ptr<EngineControl> create(const EngineControlConfig& config,
                                                 std::string& error);
    ~EngineControl();

    EngineControl(const EngineControl&) = delete;
    EngineControl& operator=(const EngineControl&) = delete;

    [[nodiscard]] EngineSnapshot snapshot() const;

    // The generic bounded wait every named wait below is built from. Logs the observation or
    // the timeout, with the last snapshot in either case.
    WaitOutcome waitFor(const char* what,
                        const std::function<bool(const EngineSnapshot&)>& condition,
                        int timeoutMs);

    // The four transitions of the run-start and run-stop path, in order.
    WaitOutcome waitForHostReady(int timeoutMs);
    [[nodiscard]] bool publishLoadScenario(const std::string& scenarioName,
                                           const std::string& modelName);
    WaitOutcome waitForScenarioLoaded(int timeoutMs);
    [[nodiscard]] bool publishStart();
    WaitOutcome waitForRunning(int timeoutMs);
    [[nodiscard]] bool publishStop();
    WaitOutcome waitForIdle(int timeoutMs);

    // Catalogue, asynchronous over the bus. The answer arrives on `sim/scenario/query-result`;
    // the wait for it rides the engine-state heartbeat like every other wait here.
    [[nodiscard]] bool requestScenarioList(const std::string& modelName);
    [[nodiscard]] std::vector<std::string> lastScenarioList() const;
    [[nodiscard]] bool requestDbList();
    [[nodiscard]] std::vector<std::string> lastDbList() const;
    WaitOutcome waitForScenarioList(int timeoutMs);

    // Entity mutation, used by the R9/OQ-4 axis spike. Exposed here rather than duplicated
    // there so the spike proves the component, not a copy of it.
    [[nodiscard]] bool publishEntityUpdate(const std::string& scenarioEntityName,
                                           const std::optional<std::array<double, 3>>& positionGeodetic,
                                           const std::optional<std::array<double, 3>>& velocityNed,
                                           const std::optional<std::array<double, 3>>& orientationYprDeg);
    [[nodiscard]] bool publishEntityCreate(const std::string& entityProfileName,
                                           const std::string& scenarioEntityName);
    [[nodiscard]] bool publishEntityDelete(const std::string& scenarioEntityName);

    // Count of engine-state publications this client has seen. Used by the spike to establish
    // that the heartbeat is live without inferring anything from a record's absence.
    [[nodiscard]] std::uint64_t engineStateTicks() const;

private:
    EngineControl();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ext17::control
