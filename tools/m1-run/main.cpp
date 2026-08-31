// EXT-17 M1 — drive one headless run by hand, and write down the lifecycle.
//
// This is the M1 exploration tool, not the campaign runner. It exists because nothing the
// platform ships publishes a bus command from a script: n8ro-shark is a passive subscriber,
// n8ro-sim-starter is a process launcher, n8ro-sim-bot is an MCP/ZMQ server, and n8ro-workbook
// is GUI-only. The host itself takes no scenario argument.
//
// What it links: the N8RO SDK only (SimulationEngineClient), which is the linkage CR-EX-2
// requires. It reads no EXT-08 source and names no EXT-08 identifier.
//
// Two rules it keeps, because M2 inherits the habits:
//   - Never throw (constraint C3). Every failure is a return value plus a log line.
//   - No wall clock decides anything. steady_clock appears only in the bounded timeouts that
//     CR-EX-2 requires each wait to have, and never in what a run reports.

#include <infrastructure/SimulationEngineClient.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

using n8ro::sim::SimulationEngineClient;
using n8ro::sim::SimulationEngineClientConfig;

using Clock = std::chrono::steady_clock;

struct Options {
    std::string simConfig  = "SimEngineClient_SharedMemory";
    std::string modelPath  = "C:\\N8RO\\data\\db";
    std::string schemaFile = "N8roSimSchema";
    std::string modelName  = "N8roSimSchema";
    std::string scenario   = "Atacama Air Defense";
    unsigned long long frames = 1200;
    int readyTimeoutMs = 20000;
    int stepTimeoutMs  = 30000;
    int settleMs       = 6000;
};

void log(const char* stage, const std::string& text) {
    std::printf("[m1-run] %-10s %s\n", stage, text.c_str());
    std::fflush(stdout);
}

// One line of engine state, as the client sees it off sim/engine/state. This is the whole
// observable surface CR-EX-2 gets to build its waits on, so M1 prints it at every transition.
std::string engineLine(const SimulationEngineClient& c) {
    std::string s = "state=" + c.getEngineState();
    s += " frame=" + std::to_string(c.getFrameNumber());
    char buf[64];
    std::snprintf(buf, sizeof buf, " simTime=%.3f dt=%.5f", c.getSimulationTimeS(), c.getDeltaS());
    s += buf;
    s += c.isScenarioLoaded() ? " scenario=loaded" : " scenario=(none)";
    if (const auto name = c.getLoadedScenarioName(); name && !name->empty()) {
        s += "(" + *name + ")";
    }
    return s;
}

// Poll a predicate to a bounded timeout. The timeout is wall-clock because a timeout is the one
// thing that legitimately is (CR-EX-4 calls it a backstop); nothing it measures is ever reported
// as a property of the run.
template <typename Pred>
bool waitUntil(Pred pred, int timeoutMs, const char* what, const SimulationEngineClient& c) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (Clock::now() < deadline) {
        if (pred()) {
            log("observed", std::string(what) + " -> " + engineLine(c));
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    log("TIMEOUT", std::string(what) + " not observed within " + std::to_string(timeoutMs)
                    + " ms; last " + engineLine(c));
    return false;
}

bool parseArgs(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                log("error", std::string(name) + " needs a value");
                return nullptr;
            }
            return argv[++i];
        };
        if (std::strcmp(argv[i], "--sim-config") == 0) {
            const char* v = next("--sim-config"); if (!v) return false; o.simConfig = v;
        } else if (std::strcmp(argv[i], "--model-path") == 0) {
            const char* v = next("--model-path"); if (!v) return false; o.modelPath = v;
        } else if (std::strcmp(argv[i], "--schema-file") == 0) {
            const char* v = next("--schema-file"); if (!v) return false; o.schemaFile = v;
        } else if (std::strcmp(argv[i], "--model-name") == 0) {
            const char* v = next("--model-name"); if (!v) return false; o.modelName = v;
        } else if (std::strcmp(argv[i], "--scenario") == 0) {
            const char* v = next("--scenario"); if (!v) return false; o.scenario = v;
        } else if (std::strcmp(argv[i], "--frames") == 0) {
            const char* v = next("--frames"); if (!v) return false;
            o.frames = std::strtoull(v, nullptr, 10);
        } else if (std::strcmp(argv[i], "--settle-ms") == 0) {
            const char* v = next("--settle-ms"); if (!v) return false; o.settleMs = std::atoi(v);
        } else {
            log("error", std::string("unrecognised option ") + argv[i]);
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Options o;
    if (!parseArgs(argc, argv, o)) return 2;

    log("config", o.simConfig + " model-path=" + o.modelPath + " schema=" + o.schemaFile);
    log("config", "scenario=\"" + o.scenario + "\" in model \"" + o.modelName
                      + "\", frame budget=" + std::to_string(o.frames));

    const SimulationEngineClientConfig cfg{o.simConfig, o.modelPath, o.schemaFile, "ext17-m1-run"};
    auto clientOpt = SimulationEngineClient::create(cfg);
    if (!clientOpt) {
        log("FAIL", "SimulationEngineClient::create returned no value - client not built");
        return 3;
    }
    auto& client = *clientOpt;
    client.startMessagePump();
    log("connect", "client created, message pump started");

    // 1. Readiness. The host publishes sim/engine/state once it is up; its arrival is the
    //    observed condition CR-EX-2 requires in place of a sleep.
    if (!waitUntil([&] { return !client.getEngineState().empty(); },
                   o.readyTimeoutMs, "host ready (engine state published)", client)) {
        client.stopMessagePump();
        return 4;
    }

    // 2. The catalogue, over the bus and asynchronously - the surface [B] names and the one
    //    OQ-4's fallback axis would enumerate.
    if (client.requestDbList()) {
        waitUntil([&] { return !client.getLastDbList().empty(); },
                  5000, "db list answered", client);
        std::string dbs;
        for (const auto& d : client.getLastDbList()) { dbs += (dbs.empty() ? "" : ", ") + d; }
        log("catalogue", "databases: [" + dbs + "]");
    }
    if (client.requestScenarioList(o.modelName)) {
        waitUntil([&] { return !client.getLastScenarioList().empty(); },
                  5000, "scenario list answered", client);
        log("catalogue", "scenarios in " + o.modelName + ": "
                             + std::to_string(client.getLastScenarioList().size()));
        for (const auto& s : client.getLastScenarioList()) { log("catalogue", "  " + s); }
    }

    // 3. Load. The recorder must already be attached: the entity_created burst that fills the
    //    roster is published here, once, and a recorder attached after it records only orphans.
    log("publish", "sim/scenario/command load_scenario \"" + o.scenario + "\"");
    if (!client.sendScenarioCommand("load_scenario", o.scenario, o.modelName)) {
        log("FAIL", "load_scenario was not published");
        client.stopMessagePump();
        return 5;
    }
    if (!waitUntil([&] { return client.isScenarioLoaded(); },
                   o.stepTimeoutMs, "scenario loaded", client)) {
        client.stopMessagePump();
        return 6;
    }

    // 4. Start.
    log("publish", "sim/engine/command start");
    if (!client.sendEngineCommand("start")) {
        log("FAIL", "start was not published");
        client.stopMessagePump();
        return 7;
    }
    if (!waitUntil([&] { return client.isRunning(); },
                   o.stepTimeoutMs, "engine running", client)) {
        client.stopMessagePump();
        return 8;
    }

    // 5. Run to a frame budget. This is OQ-1's leading candidate, exercised by hand so M1 can
    //    say whether it is observable and whether every run reaches it - not chosen here.
    log("run", "watching until frame >= " + std::to_string(o.frames));
    unsigned long long lastLogged = 0;
    const auto runDeadline = Clock::now() + std::chrono::minutes(10);
    while (client.getFrameNumber() < o.frames && Clock::now() < runDeadline) {
        const auto f = client.getFrameNumber();
        if (f >= lastLogged + 200) {
            lastLogged = f;
            log("run", engineLine(client));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    log("run", "frame budget reached -> " + engineLine(client));

    // 6. Stop. The engine's stop path rewinds the clock to zero and reloads the scenario from
    //    source, which is what puts a second segment into every capture. Watch it happen.
    log("publish", "sim/engine/command stop");
    if (!client.sendEngineCommand("stop")) {
        log("FAIL", "stop was not published");
    }
    const auto settleEnd = Clock::now() + std::chrono::milliseconds(o.settleMs);
    while (Clock::now() < settleEnd) {
        log("after-stop", engineLine(client));
        std::this_thread::sleep_for(std::chrono::milliseconds(750));
    }

    log("done", "final " + engineLine(client));
    client.stopMessagePump();
    return 0;
}
