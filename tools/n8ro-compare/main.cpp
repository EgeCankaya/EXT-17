// EXT-17 M4 — n8ro-compare: compare two captures of one configuration.
//
// This binary links NOTHING — not EXT-08, not the N8RO SDK. See build.cmd, which is where that
// is visible in one file, along with the three searches that fail the build on a hit: the SDK's
// and the producer's names, any global sort of a capture, and CR-DET-2's three hazards.
//
// It exists separately from the campaign runner for two reasons. The campaign's self-test needs
// no CLI, so a CLI is not what proves it works — and a comparison that can be run over stored
// captures is what lets this project's own claim be re-derived by anyone, over any pair of
// captures, without starting a simulator.
#include "../../src/compare/Compare.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

void printHelp() {
    std::printf(
        "n8ro-compare - EXT-17 determinism comparison for two captures of one configuration\n"
        "\n"
        "usage: n8ro-compare <capture-a> <capture-b> [options]\n"
        "       n8ro-compare --help\n"
        "\n"
        "Compares two captures of the SAME configuration two ways and reports both:\n"
        "\n"
        "  content   per (entity, occupancy) value sequences aligned on sim_time_s, over\n"
        "            RUNNING segments only. A sample present in one run and absent from the\n"
        "            other is not a difference - it is the publication schedule, which this\n"
        "            platform does not repeat (format v1 s14). It is counted and reported.\n"
        "  bytes     byte for byte, with platform.model_path excluded - the one field s14\n"
        "            names as legitimately host-dependent, and the only exclusion there will\n"
        "            ever be. EXPECTED TO FAIL on this platform. Never engineered to pass.\n"
        "\n"
        "Which of the two decides the gate is OQ-2, out with the owner of the brief and\n"
        "UNANSWERED. --gate-basis selects it; both always run and both are always reported.\n"
        "\n"
        "options:\n"
        "  --gate-basis <b>       content (default, ADR-1, THIS PROJECT'S decision) or bytes\n"
        "                         ([B]'s strictest reading). Under bytes the gate fails on\n"
        "                         this platform, which is the honest implementation of the\n"
        "                         un-ruled alternative rather than an argument about it.\n"
        "  --coverage-floor <p>   percent of the smaller run's comparable samples that must be\n"
        "                         present in both, or the verdict is indeterminate rather than\n"
        "                         pass. Default 99. Measured: the worst of all 190 pairs of\n"
        "                         M2's twenty runs was 0.4192%% unmatched.\n"
        "  --outcome-a <s>        the four-state run outcome of each run, where the caller\n"
        "  --outcome-b <s>        knows it. Omitted means not supplied, which is reported as\n"
        "                         not supplied rather than as agreement.\n"
        "  --quiet                print only the verdict line.\n"
        "  --help                 this text.\n"
        "\n"
        "exit codes:\n"
        "  0  the gate passed\n"
        "  1  the gate failed, or the comparison was indeterminate\n"
        "  2  usage error, or the comparison was refused - a precondition was not met and the\n"
        "     refusal names which\n");
}

bool wantsValue(int argc, char** argv, int& i, const char* flag, std::string& out) {
    if (std::strcmp(argv[i], flag) != 0) { return false; }
    if (i + 1 >= argc) { return false; }
    out = argv[++i];
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> positional;
    ext17::compare::CompareOptions options;
    std::string outcomeA, outcomeB;
    bool quiet = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        }
        if (arg == "--quiet") { quiet = true; continue; }
        std::string v;
        if (wantsValue(argc, argv, i, "--gate-basis", v)) {
            if (!ext17::compare::parseGateBasis(v, options.gateBasis)) {
                std::fprintf(stderr, "n8ro-compare: --gate-basis must be content or bytes\n");
                return 2;
            }
            continue;
        }
        if (wantsValue(argc, argv, i, "--coverage-floor", v)) {
            // Read as an integer percent, deliberately: a floating-point command-line value would
            // put a locale-dependent conversion on the path that decides the gate.
            long long pct = 0;
            for (char c : v) {
                if (c < '0' || c > '9') {
                    std::fprintf(stderr, "n8ro-compare: --coverage-floor must be a whole "
                                         "percentage, 0 to 100\n");
                    return 2;
                }
                pct = pct * 10 + (c - '0');
            }
            if (pct > 100) {
                std::fprintf(stderr, "n8ro-compare: --coverage-floor must be 0 to 100\n");
                return 2;
            }
            options.coverageFloor = static_cast<double>(pct) / 100.0;
            continue;
        }
        if (wantsValue(argc, argv, i, "--outcome-a", v)) { outcomeA = v; continue; }
        if (wantsValue(argc, argv, i, "--outcome-b", v)) { outcomeB = v; continue; }
        if (!arg.empty() && arg[0] == '-') {
            std::fprintf(stderr, "n8ro-compare: unknown option %s\n", arg.c_str());
            std::fprintf(stderr, "Try 'n8ro-compare --help'.\n");
            return 2;
        }
        positional.push_back(arg);
    }

    if (positional.size() != 2) {
        std::fprintf(stderr, "n8ro-compare: expected exactly two capture paths\n");
        std::fprintf(stderr, "Try 'n8ro-compare --help'.\n");
        return 2;
    }

    // The label is the file's own name. A comparison report that says "A" and "B" makes the
    // reader hold the mapping in their head; one that says which file is which does not.
    const auto label = [](const std::string& path) {
        const std::size_t slash = path.find_last_of("\\/");
        return slash == std::string::npos ? path : path.substr(slash + 1);
    };

    const ext17::compare::ComparisonResult r = ext17::compare::compareCaptures(
        positional[0], positional[1], label(positional[0]), label(positional[1]),
        outcomeA, outcomeB, options);

    if (!quiet) {
        const std::string report = ext17::compare::renderReport(r);
        std::fwrite(report.data(), 1, report.size(), stdout);
    } else {
        std::printf("%s %s\n", ext17::compare::name(r.gate),
                    r.refusal == ext17::compare::Refusal::None
                        ? ""
                        : ext17::compare::name(r.refusal));
    }

    if (r.refusal != ext17::compare::Refusal::None) { return 2; }
    return r.passed() ? 0 : 1;
}
