// EXT-17 M2 — the R9 / OQ-4 parameterisation-axis feasibility spike.
//
// **This is a spike, not a product.** It answers one question and is then evidence:
//
//   [B] offers three parameterisation axes - initial positions and velocities, which entities
//   are present, which scenario from the catalogue. R9 objects that an axis may require
//   authoring scenario variants into C:\N8RO, which is read-only for this project. Which axis
//   v1 uses is OQ-4, decided at M5. **This spike does not choose it.** It establishes, per
//   axis, whether the axis is reachable over the bus with no authoring into the install tree.
//
// The open question M1 left, and the reason this is a measurement rather than a reading of the
// header: SimulationEngineClient exposes sendEntityCreate / sendEntityDelete / sendEntityUpdate,
// but nothing says whether state set between `load_scenario` and `start` survives into the run
// or is overwritten when the engine materialises the roster. Each probe below is one run whose
// capture answers that for one axis.
//
// Probes, all against Atacama Air Defense unless stated:
//
//   p0-baseline       no mutation. The control every other probe is read against.
//   p1-update-pre     sendEntityUpdate on RedUAV_N_01 after load, before start.
//   p2-update-post    the same update, as soon as the engine reports running.
//   p2b-update-mid    the same update again, but not until frame 100 - a real mid-run control.
//                     p2 alone is not one: its hook fires the instant `running` is observed,
//                     which is frame 0, so it lands before the first frame is integrated and
//                     cannot be told apart from a pre-start update by its effect.
//   p3-delete-pre     sendEntityDelete on RedUAV_N_01 after load, before start.
//   p4-create-pre     sendEntityCreate of a new entity from an existing profile, before start.
//   p5-scenario       loads Baltic Sentinel instead. Axis C end to end, no authoring at all.
//
// Reads no EXT-08 source. Links the N8RO SDK only, through the same components the campaign
// runner uses - so what the spike proves, it proves about the product's control path.
#include "../../src/common/Log.h"
#include "../../src/proc/Process.h"
#include "../../src/run/RunOnce.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

// The baseline for RedUAV_N_01 at its first published sample, read out of the M1 capture:
//   positionGeodetic [-23.41870470367031, -68.2802, 400]
//   velocityNed      [-55, ~0, 0]
// The injected values are far enough from it that no probe result is ambiguous.
constexpr const char* kTargetEntity = "RedUAV_N_01";
constexpr std::array<double, 3> kInjectedPosition{-23.30000, -68.10000, 900.0};
constexpr std::array<double, 3> kInjectedVelocity{-80.0, 25.0, -5.0};

// A profile name the scenario already uses, taken from the `name` field of RedUAV_N_01's own
// samples. Creating from an existing profile keeps the probe inside what the install ships.
constexpr const char* kProfileName = "Air_UAV_LoiteringMunition_Generic";
constexpr const char* kCreatedEntity = "SpikeUAV_01";

struct Probe {
    const char* id;
    const char* scenario;
    const char* question;
};

const Probe kProbes[] = {
    {"p0-baseline",    "Atacama Air Defense", "what an unmutated run looks like"},
    {"p1-update-pre",  "Atacama Air Defense", "does an entity update before start survive into the run"},
    {"p2-update-post", "Atacama Air Defense", "does the same update take effect at frame 0"},
    {"p2b-update-mid",  "Atacama Air Defense", "does the same update take effect at frame 100"},
    {"p3-delete-pre",  "Atacama Air Defense", "does deleting an entity before start remove it from the run"},
    {"p4-create-pre",  "Atacama Air Defense", "does creating an entity before start add it to the run"},
    {"p5-scenario",    "Baltic Sentinel",     "does a different catalogue scenario run unchanged"},
};

std::string joinPath(const std::string& dir, const std::string& leaf) {
    if (dir.empty()) { return leaf; }
    const char last = dir.back();
    return (last == '\\' || last == '/') ? dir + leaf : dir + "\\" + leaf;
}

void applyProbe(const std::string& id, ext17::run::RunConfig& cfg) {
    using ext17::control::EngineControl;
    using ext17::log::line;

    if (id == "p1-update-pre") {
        cfg.afterLoadBeforeStart = [](EngineControl& c) {
            const bool ok = c.publishEntityUpdate(kTargetEntity, kInjectedPosition,
                                                  kInjectedVelocity, std::nullopt);
            line("spike", std::string("sendEntityUpdate(") + kTargetEntity + ") before start -> "
                              + (ok ? "published" : "NOT published"));
        };
    } else if (id == "p2-update-post") {
        cfg.afterStart = [](EngineControl& c) {
            const bool ok = c.publishEntityUpdate(kTargetEntity, kInjectedPosition,
                                                  kInjectedVelocity, std::nullopt);
            line("spike", std::string("sendEntityUpdate(") + kTargetEntity + ") after start -> "
                              + (ok ? "published" : "NOT published"));
        };
    } else if (id == "p2b-update-mid") {
        cfg.afterStart = [](EngineControl& c) {
            // Wait for the run to be properly under way before mutating anything. This is the
            // same bounded, event-driven wait the campaign uses everywhere else - no sleep.
            const auto w = c.waitFor("frame 100 reached, before the mid-run update",
                                     [](const ext17::control::EngineSnapshot& s) {
                                         return s.frame >= 100;
                                     },
                                     60000);
            if (!w.observed) {
                line("spike", "frame 100 was not reached; the mid-run update was not sent");
                return;
            }
            const bool ok = c.publishEntityUpdate(kTargetEntity, kInjectedPosition,
                                                  kInjectedVelocity, std::nullopt);
            line("spike", std::string("sendEntityUpdate(") + kTargetEntity + ") at frame "
                              + std::to_string(w.at.frame) + " -> "
                              + (ok ? "published" : "NOT published"));
        };
    } else if (id == "p3-delete-pre") {
        cfg.afterLoadBeforeStart = [](EngineControl& c) {
            const bool ok = c.publishEntityDelete(kTargetEntity);
            line("spike", std::string("sendEntityDelete(") + kTargetEntity + ") before start -> "
                              + (ok ? "published" : "NOT published"));
        };
    } else if (id == "p4-create-pre") {
        cfg.afterLoadBeforeStart = [](EngineControl& c) {
            const bool ok = c.publishEntityCreate(kProfileName, kCreatedEntity);
            line("spike", std::string("sendEntityCreate(") + kProfileName + ", " + kCreatedEntity
                              + ") before start -> " + (ok ? "published" : "NOT published"));
        };
    }
    // p0-baseline and p5-scenario mutate nothing; p5 differs only in which scenario it loads.
}

void usage() {
    std::puts(
"spike-axis - EXT-17 M2 R9/OQ-4 parameterisation-axis feasibility spike\n"
"\n"
"usage: spike-axis --out-dir <dir> --recorder <path> [--probe <id>] [--frames <n>]\n"
"\n"
"  --out-dir <dir>    where the probe runs land, one subdirectory each. Required.\n"
"  --recorder <path>  the capture recorder, driven as a process. Required.\n"
"  --probe <id>       run one probe. Default: all of them, in order.\n"
"  --frames <n>       frame budget per probe run. Default 200.\n"
"\n"
"probes:\n"
"  p0-baseline     no mutation; the control the others are read against\n"
"  p1-update-pre   sendEntityUpdate before start   - does it survive materialisation\n"
"  p2-update-post  sendEntityUpdate after start    - does it take effect at all\n"
"  p3-delete-pre   sendEntityDelete before start   - which entities are present\n"
"  p4-create-pre   sendEntityCreate before start   - which entities are present\n"
"  p5-scenario     a different catalogue scenario  - no authoring at all\n"
"\n"
"This spike does not choose an axis. OQ-4 is decided at M5.");
}

} // namespace

int main(int argc, char** argv) {
    std::string outDir;
    std::string recorder;
    std::string onlyProbe;
    unsigned long long frames = 200;

    for (int i = 1; i < argc; ++i) {
        const std::string opt = argv[i];
        const auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : nullptr; };
        if (opt == "--help" || opt == "-h") { usage(); return 0; }
        else if (opt == "--out-dir")   { const char* v = next(); if (!v) { usage(); return 2; } outDir = v; }
        else if (opt == "--recorder")  { const char* v = next(); if (!v) { usage(); return 2; } recorder = v; }
        else if (opt == "--probe")     { const char* v = next(); if (!v) { usage(); return 2; } onlyProbe = v; }
        else if (opt == "--frames")    { const char* v = next(); if (!v) { usage(); return 2; } frames = std::strtoull(v, nullptr, 10); }
        else { std::fprintf(stderr, "spike-axis: unrecognised option %s\n", opt.c_str()); return 2; }
    }
    if (outDir.empty() || recorder.empty()) { usage(); return 2; }
    if (!ext17::proc::createDirectories(outDir)) {
        std::fprintf(stderr, "spike-axis: could not create %s\n", outDir.c_str());
        return 2;
    }
    ext17::log::mirrorToFile(joinPath(outDir, "spike-axis.log"));

    int failures = 0;
    for (const auto& probe : kProbes) {
        if (!onlyProbe.empty() && onlyProbe != probe.id) { continue; }

        ext17::log::line("spike", std::string("=== ") + probe.id + " : " + probe.question + " ===");

        ext17::run::RunConfig cfg;
        cfg.runId = probe.id;
        cfg.runDir = joinPath(outDir, probe.id);
        cfg.scenario = probe.scenario;
        cfg.recorderExe = recorder;
        cfg.predicate = ext17::run::StopPredicate::frameBudget(frames);
        cfg.runTimeoutMs = 120000;
        applyProbe(probe.id, cfg);

        const auto record = ext17::run::executeRun(cfg);
        if (record.outcome != ext17::run::RunOutcome::Completed) { ++failures; }
    }

    ext17::log::line("spike", failures == 0 ? "every probe completed"
                                            : std::to_string(failures) + " probe(s) did not complete");
    ext17::log::closeMirror();
    return failures == 0 ? 0 : 1;
}
