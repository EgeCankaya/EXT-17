// EXT-17 M5 — the OQ-4 *fidelity* spike.
//
// **A spike, not a product.** M2's `tools/spike-axis` measured FEASIBILITY for all three of
// [B]'s parameterisation axes and said so in its own words:
//
//   "It measured feasibility, not fidelity. That an injected position is honoured says nothing
//    about whether a swept range of positions produces a scenario that still makes sense."
//
// Fidelity is one of OQ-4's two deciding criteria and it is the one nothing has measured. This
// spike measures it, for the axis that would have to carry it, and is then evidence. It chooses
// no axis by itself: it is evidence, weighed against this and against M2's spike.
//
// The sweep candidate under test: **the closing speed of the Red raid** in Atacama Air Defense.
// Every Red UAV is authored at 55 m/s on a fixed heading — the north group flying south, the
// east group flying west — into a defended cluster about 8.6 km away. A frame budget of 1200 is
// 60 s of simulation, so the speed decides how far the raid gets inside the window, and the
// baseline run already contains real engagement outcomes (health leaving `nominal`, SAM rounds
// created and deleted). That makes it a candidate for a scalar whose sweep changes an outcome.
//
// Each probe is one run. `--speeds` is a list of metres per second; each value produces one run
// in which every Red UAV's velocity is set, before `start`, to the authored heading scaled to
// that speed. The authored position is left alone (`std::nullopt`), so the raid geometry is the
// scenario's own and the only thing varying is the scalar.
//
// `--catalogue` is a second, separate probe: one short run that asks the platform's own
// scenario-catalogue query what scenarios exist and logs the answer verbatim. That is axis C's
// enumeration cost, measured rather than assumed, because CR-PAR-1's fourth criterion requires
// enumeration through the platform's query and flags the answers as asynchronous.
//
// Reads no EXT-08 source. Links the N8RO SDK only, through the same src/ components the
// campaign runner uses — so what the spike measures, it measures about the product's own
// control path.
#include "../../src/common/Log.h"
#include "../../src/control/EngineControl.h"
#include "../../src/proc/Process.h"
#include "../../src/run/RunOnce.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace {

// The Red raid, read off the roster at `sim_time_s` 0 of a baseline capture rather than out of
// the scenario file, which is a binary. Twelve north-group UAVs flying south, eighteen
// east-group UAVs flying west, every one of them authored at 400 m and 55 m/s.
//
// The heading is a unit NED vector. Scaling it by the swept speed is the whole mutation: no
// position is touched, so the formation the scenario authored is the formation that flies.
struct Raider {
    const char* name;
    double northUnit;
    double eastUnit;
};

const Raider kRaid[] = {
    {"RedUAV_N_01", -1.0, 0.0}, {"RedUAV_N_02", -1.0, 0.0}, {"RedUAV_N_03", -1.0, 0.0},
    {"RedUAV_N_04", -1.0, 0.0}, {"RedUAV_N_05", -1.0, 0.0}, {"RedUAV_N_06", -1.0, 0.0},
    {"RedUAV_N_07", -1.0, 0.0}, {"RedUAV_N_08", -1.0, 0.0}, {"RedUAV_N_09", -1.0, 0.0},
    {"RedUAV_N_10", -1.0, 0.0}, {"RedUAV_N_11", -1.0, 0.0}, {"RedUAV_N_12", -1.0, 0.0},
    {"RedUAV_E_01", 0.0, -1.0}, {"RedUAV_E_02", 0.0, -1.0}, {"RedUAV_E_03", 0.0, -1.0},
    {"RedUAV_E_04", 0.0, -1.0}, {"RedUAV_E_05", 0.0, -1.0}, {"RedUAV_E_06", 0.0, -1.0},
    {"RedUAV_E_07", 0.0, -1.0}, {"RedUAV_E_08", 0.0, -1.0}, {"RedUAV_E_09", 0.0, -1.0},
    {"RedUAV_E_10", 0.0, -1.0}, {"RedUAV_E_11", 0.0, -1.0}, {"RedUAV_E_12", 0.0, -1.0},
    {"RedUAV_E_13", 0.0, -1.0}, {"RedUAV_E_14", 0.0, -1.0}, {"RedUAV_E_15", 0.0, -1.0},
    {"RedUAV_E_16", 0.0, -1.0}, {"RedUAV_E_17", 0.0, -1.0}, {"RedUAV_E_18", 0.0, -1.0},
};

constexpr std::size_t kRaidSize = sizeof(kRaid) / sizeof(kRaid[0]);

std::string joinPath(const std::string& dir, const std::string& leaf) {
    if (dir.empty()) { return leaf; }
    const char last = dir.back();
    return (last == '\\' || last == '/') ? dir + leaf : dir + "\\" + leaf;
}

// A run id that is a stable, locale-free label for the value: the speed in whole metres per
// second and, if it has one, one decimal. Run ids are never timestamps (PRD, "File and path
// conventions") and this one is not a formatted double either.
std::string speedLabel(double speedMs) {
    const long long whole = static_cast<long long>(speedMs);
    const long long tenth = static_cast<long long>((speedMs - static_cast<double>(whole)) * 10.0 + 0.5);
    std::string s = "v" + std::to_string(whole);
    if (tenth != 0) { s += "p" + std::to_string(tenth); }
    return s;
}

void usage() {
    std::puts(
"spike-oq4 - EXT-17 M5 OQ-4 fidelity spike\n"
"\n"
"usage: spike-oq4 --out-dir <dir> --recorder <path> [--speeds <a,b,c>]\n"
"                 [--frames <n>] [--repeat <n>]\n"
"       spike-oq4 --out-dir <dir> --recorder <path> --catalogue\n"
"\n"
"  --out-dir <dir>    where the probe runs land, one subdirectory each. Required.\n"
"  --recorder <path>  the capture recorder, driven as a process. Required.\n"
"  --speeds <list>    comma-separated closing speeds in m/s. One run each. The scenario\n"
"                     authors 55. Default: 11,27.5,55,110,220,440,900.\n"
"  --repeat <n>       run each value n times, into <label>-000, -001, ... Default 1.\n"
"                     n > 1 is the CR-PAR-1 check: two runs at ONE value are one\n"
"                     configuration and are therefore a valid self-test pair.\n"
"  --frames <n>       frame budget per probe run. Default 1200, which is 60 s - the\n"
"                     baseline's first engagement is at 35 s, so a shorter run would\n"
"                     measure a window in which nothing has happened yet.\n"
"  --catalogue        instead of the sweep, run one short run that asks the platform's\n"
"                     own scenario-catalogue query what exists, and log the answer.\n"
"\n"
"This spike chooses no axis. It measures the one criterion OQ-4 names and M2's\n"
"feasibility spike deliberately did not: whether a swept range still makes sense.");
}

} // namespace

int main(int argc, char** argv) {
    std::string outDir;
    std::string recorder;
    std::string speedList = "11,27.5,55,110,220,440,900";
    unsigned long long frames = 1200;
    unsigned long long repeat = 1;
    bool catalogueProbe = false;

    for (int i = 1; i < argc; ++i) {
        const std::string opt = argv[i];
        const auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : nullptr; };
        if (opt == "--help" || opt == "-h") { usage(); return 0; }
        else if (opt == "--out-dir")   { const char* v = next(); if (!v) { usage(); return 2; } outDir = v; }
        else if (opt == "--recorder")  { const char* v = next(); if (!v) { usage(); return 2; } recorder = v; }
        else if (opt == "--speeds")    { const char* v = next(); if (!v) { usage(); return 2; } speedList = v; }
        else if (opt == "--frames")    { const char* v = next(); if (!v) { usage(); return 2; } frames = std::strtoull(v, nullptr, 10); }
        else if (opt == "--repeat")    { const char* v = next(); if (!v) { usage(); return 2; } repeat = std::strtoull(v, nullptr, 10); }
        else if (opt == "--catalogue") { catalogueProbe = true; }
        else { std::fprintf(stderr, "spike-oq4: unrecognised option %s\n", opt.c_str()); return 2; }
    }
    if (outDir.empty() || recorder.empty()) { usage(); return 2; }
    if (!ext17::proc::createDirectories(outDir)) {
        std::fprintf(stderr, "spike-oq4: could not create %s\n", outDir.c_str());
        return 2;
    }
    ext17::log::mirrorToFile(joinPath(outDir, "spike-oq4.log"));

    int failures = 0;

    if (catalogueProbe) {
        ext17::log::line("spike", "=== catalogue : what the platform's own query answers, and how fast ===");
        ext17::run::RunConfig cfg;
        cfg.runId = "catalogue";
        cfg.runDir = joinPath(outDir, "catalogue");
        cfg.recorderExe = recorder;
        cfg.attachRecorder = false;   // nothing here is read off a capture
        cfg.predicate = ext17::run::StopPredicate::frameBudget(60);
        cfg.runTimeoutMs = 120000;
        // The hook fires once the scenario reports loaded, which is the only EngineControl this
        // spike is handed. The query is about the model, not about the loaded scenario.
        cfg.afterLoadBeforeStart = [](ext17::control::EngineControl& c) {
            if (!c.requestScenarioList("N8roSimSchema")) {
                ext17::log::line("spike", "requestScenarioList was NOT published");
                return;
            }
            const auto w = c.waitForScenarioList(10000);
            if (!w.observed) {
                ext17::log::line("spike", "the catalogue did not answer within 10000 ms");
                return;
            }
            const auto names = c.lastScenarioList();
            ext17::log::line("spike", "catalogue answered in " + std::to_string(w.elapsedMs)
                                          + " ms with " + std::to_string(names.size()) + " scenario(s)");
            for (const auto& n : names) { ext17::log::line("spike", "  scenario: " + n); }
        };
        const auto record = ext17::run::executeRun(cfg);
        if (record.outcome != ext17::run::RunOutcome::Completed) { ++failures; }
        ext17::log::closeMirror();
        return failures == 0 ? 0 : 1;
    }

    std::vector<double> speeds;
    {
        std::string field;
        const std::string src = speedList + ",";
        for (const char ch : src) {
            if (ch == ',') {
                if (!field.empty()) { speeds.push_back(std::strtod(field.c_str(), nullptr)); }
                field.clear();
            } else { field += ch; }
        }
    }
    if (speeds.empty()) { std::fprintf(stderr, "spike-oq4: --speeds parsed to nothing\n"); return 2; }

    if (repeat == 0) { repeat = 1; }
    for (const double speedMs : speeds) {
      for (unsigned long long rep = 0; rep < repeat; ++rep) {
        // Ordinals, never timestamps, and only when there is more than one run at a value -
        // so a plain sweep keeps the label the value gave it.
        std::string label = speedLabel(speedMs);
        if (repeat > 1) {
            const std::string ord = std::to_string(rep);
            label += "-" + std::string(3 - (ord.size() < 3 ? ord.size() : 3), '0') + ord;
        }
        ext17::log::line("spike", "=== " + label + " : does a Red raid at this speed still make "
                                                   "sense; the scenario authors 55 m/s ===");

        ext17::run::RunConfig cfg;
        cfg.runId = label;
        cfg.runDir = joinPath(outDir, label);
        cfg.recorderExe = recorder;
        cfg.predicate = ext17::run::StopPredicate::frameBudget(frames);
        cfg.runTimeoutMs = 600000;
        cfg.afterLoadBeforeStart = [speedMs](ext17::control::EngineControl& c) {
            int published = 0;
            for (const auto& r : kRaid) {
                const std::array<double, 3> vel{r.northUnit * speedMs, r.eastUnit * speedMs, 0.0};
                if (c.publishEntityUpdate(r.name, std::nullopt, vel, std::nullopt)) { ++published; }
            }
            ext17::log::line("spike", "set velocity on " + std::to_string(published) + " of "
                                          + std::to_string(kRaidSize) + " Red UAV(s) before start");
        };

        const auto record = ext17::run::executeRun(cfg);
        if (record.outcome != ext17::run::RunOutcome::Completed) { ++failures; }
      }
    }

    ext17::log::line("spike", failures == 0 ? "every probe completed"
                                            : std::to_string(failures) + " probe(s) did not complete");
    ext17::log::closeMirror();
    return failures == 0 ? 0 : 1;
}
