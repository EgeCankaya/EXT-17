#include "EngineControl.h"

#include "../common/Log.h"

#include <infrastructure/SimulationEngineClient.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>

namespace ext17::control {
namespace {

using Clock = std::chrono::steady_clock;

std::int64_t millisSince(const Clock::time_point& start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

} // namespace

std::string EngineSnapshot::line() const {
    std::string s = "state=" + (state.empty() ? std::string("(none)") : state);
    s += " frame=" + std::to_string(frame);
    char buf[80];
    std::snprintf(buf, sizeof buf, " simTime=%.3f dt=%.5f", simTimeS, deltaS);
    s += buf;
    s += scenarioLoaded ? " scenario=loaded" : " scenario=(none)";
    if (scenarioLoaded && !scenarioName.empty()) {
        s += "(" + scenarioName + ")";
    }
    return s;
}

struct EngineControl::Impl {
    std::optional<n8ro::sim::SimulationEngineClient> client;
    std::uint64_t stateSubscription = 0;

    mutable std::mutex mutex;
    std::condition_variable cv;
    std::uint64_t ticks = 0;

    EngineSnapshot read() const {
        EngineSnapshot s;
        if (!client) { return s; }
        const auto& c = *client;
        s.state = c.getEngineState();
        s.running = c.isRunning();
        s.scenarioLoaded = c.isScenarioLoaded();
        if (const auto name = c.getLoadedScenarioName(); name) { s.scenarioName = *name; }
        s.frame = c.getFrameNumber();
        s.simTimeS = c.getSimulationTimeS();
        s.deltaS = c.getDeltaS();
        return s;
    }
};

EngineControl::EngineControl() : impl_(std::make_unique<Impl>()) {}

EngineControl::~EngineControl() {
    if (impl_ && impl_->client) {
        if (impl_->stateSubscription != 0) {
            impl_->client->unsubscribe(impl_->stateSubscription);
            impl_->stateSubscription = 0;
        }
        impl_->client->stopMessagePump();
    }
}

std::unique_ptr<EngineControl> EngineControl::create(const EngineControlConfig& config,
                                                     std::string& error) {
    error.clear();
    std::unique_ptr<EngineControl> self(new EngineControl());

    const n8ro::sim::SimulationEngineClientConfig cfg{
        config.simConfig, config.modelPath, config.schemaFile, config.senderId};
    self->impl_->client = n8ro::sim::SimulationEngineClient::create(cfg);
    if (!self->impl_->client) {
        error = "SimulationEngineClient::create returned no value; the client was not built. "
                "Check --model-path and --schema-file, and that C:\\N8RO\\bin is on PATH.";
        return nullptr;
    }

    // Subscribe before the pump starts, so no engine-state publication is missed between the
    // two. The handler needs nothing from the message: the client keeps the state itself, and
    // all this does is wake every bounded wait so it can re-evaluate its own condition.
    Impl* impl = self->impl_.get();
    impl->stateSubscription = impl->client->subscribeEngineState(
        [impl](const n8ro::core::Message&) {
            {
                std::lock_guard<std::mutex> lock(impl->mutex);
                ++impl->ticks;
            }
            impl->cv.notify_all();
        });

    impl->client->startMessagePump();
    return self;
}

EngineSnapshot EngineControl::snapshot() const {
    return impl_->read();
}

std::uint64_t EngineControl::engineStateTicks() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->ticks;
}

WaitOutcome EngineControl::waitFor(const char* what,
                                   const std::function<bool(const EngineSnapshot&)>& condition,
                                   int timeoutMs) {
    WaitOutcome outcome;
    outcome.what = what ? what : "";
    outcome.timeoutMs = timeoutMs;

    const auto started = Clock::now();
    const auto deadline = started + std::chrono::milliseconds(timeoutMs);

    // The condition is evaluated once on entry and then once per engine-state publication, so a
    // condition already true costs nothing and one that becomes true is seen on the next
    // heartbeat. No sleep, no poll interval; the deadline is the only timed quantity.
    //
    // The condition is deliberately evaluated with **our mutex released**. It reads the SDK
    // client's getters, which take the client's own lock, while the client's message pump calls
    // our handler, which takes ours. Evaluating under both would put the two locks in opposite
    // orders on the two threads, which is a deadlock waiting for a slow morning - and a
    // deadlocked wait in an unattended twenty-run campaign is the failure this project exists
    // to avoid. The tick counter is what the two threads share, and it is all they share.
    bool observed = false;
    {
        std::unique_lock<std::mutex> lock(impl_->mutex);
        std::uint64_t seen = impl_->ticks;
        for (;;) {
            lock.unlock();
            const bool held = condition(impl_->read());
            lock.lock();
            if (held) { observed = true; break; }
            // A publication that arrived while the condition was being evaluated means the
            // state has moved on: re-evaluate rather than wait on it.
            if (impl_->ticks != seen) { seen = impl_->ticks; continue; }
            if (!impl_->cv.wait_until(lock, deadline,
                                      [&] { return impl_->ticks != seen; })) {
                break;  // the bounded timeout, and nothing else, ended this wait
            }
            seen = impl_->ticks;
        }
    }

    outcome.observed = observed;
    outcome.elapsedMs = millisSince(started);
    outcome.at = impl_->read();

    if (observed) {
        log::line("observed", outcome.what + " -> " + outcome.at.line());
    } else {
        log::line("TIMEOUT", outcome.what + " not observed within " + std::to_string(timeoutMs)
                                 + " ms; last " + outcome.at.line());
    }
    return outcome;
}

WaitOutcome EngineControl::waitForHostReady(int timeoutMs) {
    // Readiness is the first engine-state publication. Its arrival is a positive observation,
    // never an inference from silence.
    return waitFor("host ready (engine state published)",
                   [](const EngineSnapshot& s) { return !s.state.empty(); }, timeoutMs);
}

bool EngineControl::publishLoadScenario(const std::string& scenarioName,
                                        const std::string& modelName) {
    log::line("publish", "sim/scenario/command load_scenario \"" + scenarioName + "\"");
    if (!impl_->client) { return false; }
    return impl_->client->sendScenarioCommand("load_scenario", scenarioName, modelName);
}

WaitOutcome EngineControl::waitForScenarioLoaded(int timeoutMs) {
    return waitFor("scenario loaded",
                   [](const EngineSnapshot& s) { return s.scenarioLoaded; }, timeoutMs);
}

bool EngineControl::publishStart() {
    log::line("publish", "sim/engine/command start");
    if (!impl_->client) { return false; }
    return impl_->client->sendEngineCommand("start");
}

WaitOutcome EngineControl::waitForRunning(int timeoutMs) {
    return waitFor("engine running",
                   [](const EngineSnapshot& s) { return s.running; }, timeoutMs);
}

bool EngineControl::publishStop() {
    log::line("publish", "sim/engine/command stop");
    if (!impl_->client) { return false; }
    return impl_->client->sendEngineCommand("stop");
}

WaitOutcome EngineControl::waitForIdle(int timeoutMs) {
    // M1: stop rewinds the clock and reloads the scenario from source, so the engine returns to
    // `idle frame=0 simTime=0.000` with the scenario still loaded. Waiting for !running is the
    // observation that the stop actually landed, rather than that it was published.
    return waitFor("engine idle after stop",
                   [](const EngineSnapshot& s) { return !s.running; }, timeoutMs);
}

bool EngineControl::requestScenarioList(const std::string& modelName) {
    if (!impl_->client) { return false; }
    return impl_->client->requestScenarioList(modelName);
}

std::vector<std::string> EngineControl::lastScenarioList() const {
    if (!impl_->client) { return {}; }
    return impl_->client->getLastScenarioList();
}

bool EngineControl::requestDbList() {
    if (!impl_->client) { return false; }
    return impl_->client->requestDbList();
}

std::vector<std::string> EngineControl::lastDbList() const {
    if (!impl_->client) { return {}; }
    return impl_->client->getLastDbList();
}

WaitOutcome EngineControl::waitForScenarioList(int timeoutMs) {
    // The catalogue answer arrives on its own topic, but the wait rides the engine-state
    // heartbeat like every other one here: each publication re-evaluates the condition.
    return waitFor("scenario list answered",
                   [this](const EngineSnapshot&) { return !lastScenarioList().empty(); },
                   timeoutMs);
}

bool EngineControl::publishEntityUpdate(const std::string& scenarioEntityName,
                                        const std::optional<std::array<double, 3>>& positionGeodetic,
                                        const std::optional<std::array<double, 3>>& velocityNed,
                                        const std::optional<std::array<double, 3>>& orientationYprDeg) {
    if (!impl_->client) { return false; }
    return impl_->client->sendEntityUpdate(scenarioEntityName, positionGeodetic, velocityNed,
                                           orientationYprDeg);
}

bool EngineControl::publishEntityCreate(const std::string& entityProfileName,
                                        const std::string& scenarioEntityName) {
    if (!impl_->client) { return false; }
    return impl_->client->sendEntityCreate(entityProfileName, scenarioEntityName);
}

bool EngineControl::publishEntityDelete(const std::string& scenarioEntityName) {
    if (!impl_->client) { return false; }
    return impl_->client->sendEntityDelete(scenarioEntityName);
}

} // namespace ext17::control
