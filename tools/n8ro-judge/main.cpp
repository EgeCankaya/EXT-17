// EXT-17 M6 — n8ro-judge: evaluate declared conditions over stored captures.
//
// This binary is CR-CAP-1's second half made structural. It **cannot** start a host, load a
// scenario or subscribe to a bus, because it links nothing that could — see build.cmd, which
// names no include path, no library, and searches its own sources for the attempt. So
// *"produces verdicts with no host started and no bus subscription made"* is not a promise about
// how it is invoked; it is a property of what it is.
//
// It is also the same evaluator `n8ro-campaign` runs in-process over the capture it has just
// written. One evaluator, one kind of input — a stored capture. That is what makes *"verdicts
// produced by re-judging are identical to those produced during the live run"* true by
// construction, and `--verify` is what checks that the construction stayed true.
#include "../../src/assert/Conditions.h"
#include "../../src/assert/Judge.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using namespace ext17;

namespace {

const char* kHelp =
"n8ro-judge - EXT-17 condition evaluator over STORED captures (CR-AS-1..4, CR-CAP-1)\n"
"\n"
"usage: n8ro-judge check    --conditions <file>\n"
"       n8ro-judge capture  <capture-file> --conditions <file> [options]\n"
"       n8ro-judge campaign <campaign-dir> --conditions <file> [options]\n"
"       n8ro-judge --help\n"
"\n"
"commands:\n"
"  check                    load and validate the condition file and stop. This is what\n"
"                           n8ro-campaign runs BEFORE any host is started (CR-AS-1), so a\n"
"                           typo costs ten seconds rather than twenty runs.\n"
"  capture                  judge one stored capture.\n"
"  campaign                 judge every run's capture under <campaign-dir>/runs/NNN. This is\n"
"                           the re-judge mode: no host is started and no bus subscription is\n"
"                           made, and this binary could not make one.\n"
"\n"
"options:\n"
"  --conditions <file>      the condition file. Required by every command.\n"
"  --write <name>           write each run's verdicts to <run-dir>/<name>, one JSON object\n"
"                           per line. Default verdicts-rejudge.jsonl for campaign, and\n"
"                           nothing for capture.\n"
"  --verify <name>          after judging, byte-compare what was produced against the named\n"
"                           file already in each run directory - the one the live campaign\n"
"                           wrote. Default verdicts.jsonl. A difference is a non-zero exit\n"
"                           and names the first line that differs.\n"
"  --quiet                  one line per run; omit the per-verdict detail.\n"
"  --help                   this text.\n"
"\n"
"the three kinds, and what a not-met verdict is entitled to say (CR-AS-4):\n"
"  proximity                did two entities come within a distance. A not-met verdict is\n"
"                           sound when the closest observed approach clears the threshold by\n"
"                           more than the pair could have closed inside the largest\n"
"                           unobserved window; otherwise indeterminate.\n"
"  area                     was an entity inside (or outside) a circle or polygon. The same\n"
"                           bound, measured to the region's boundary.\n"
"  terminal_state           did an entity reach a terminal state. With removal_reason a\n"
"                           not-met verdict is sound when every occupancy is closed by a\n"
"                           record or carries a sample at the segment's last instant -\n"
"                           capture format v1 s8.1 makes that positive evidence. With\n"
"                           field+equals a not-met verdict is NEVER sound: nothing bounds a\n"
"                           field's rate of change, so it reports indeterminate.\n"
"\n"
"  A met verdict is always sound - it is computed from records that are present. An\n"
"  indeterminate verdict is a VERDICT state and never a fifth RUN outcome; the brief fixes\n"
"  the run vocabulary at four, and keeping the two apart is deliberate.\n"
"\n"
"  Verdicts are evaluated over RUNNING segments only. A capture with none cannot be judged\n"
"  at all - sim_time_s does not order a frozen segment's samples - and every verdict is\n"
"  indeterminate. That is R14's shape and it is not rare.\n"
"\n"
"expect - the one key this project adds to the vendored condition shape (OQ-5):\n"
"  \"expect\": \"met\"       the default, and how a file written for EXT-08's referee reads.\n"
"  \"expect\": \"not_met\"   the condition asserts NON-OCCURRENCE.\n"
"\n"
"  The vendored schema is a referee: it reports whether a condition was satisfied and says\n"
"  nothing about whether that is welcome. A campaign runner must map those facts onto the\n"
"  brief's four run outcomes, and two of its three questions survive that reading while the\n"
"  third does not - \"did anything reach a terminal state it SHOULD NOT have\" is an\n"
"  assertion of non-occurrence, and the vendored shape expresses one only for the area\n"
"  kind, through test: inside|outside. Without expect, a condition asking whether the\n"
"  command centre was destroyed asserts that it should have been.\n"
"\n"
"  The verdict still records the FACT in the vendored schema's own terms - met or not met -\n"
"  so EXT-08's verdicts and a re-judgement here stay directly comparable. Whether the fact\n"
"  was the asserted one is a separate field: satisfied, violated, or undetermined. Only a\n"
"  VIOLATION makes a run fail. An indeterminate verdict is undetermined and is never folded\n"
"  into either, whatever was expected.\n"
"\n"
"the THREE of the four run outcomes a stored capture can carry (CR-EX-5):\n"
"  pass                     every asserted condition was satisfied.\n"
"  fail                     the capture read, and a declared condition was VIOLATED.\n"
"  infrastructure_error     the capture would not read, or was not conformant, or carries\n"
"                           no running segment, or decided nothing at all. None of these is\n"
"                           a failing scenario, and reporting one as fail is what the\n"
"                           brief's rule 3 forbids.\n"
"\n"
"  timeout is the fourth, and it is absent here for a reason rather than by oversight: it\n"
"  is a property of an EXECUTION - a run that ran past its backstop - and nothing about a\n"
"  file on disk can be too slow. n8ro-campaign assigns it, and a run that timed out is\n"
"  never judged at all (CR-EX-5), so no capture reaching this binary can carry it. The run\n"
"  vocabulary is still FOUR and is fixed at four; this command can reach three of them.\n"
"\n"
"exit codes:\n"
"  0  every capture was judged and every asserted condition was satisfied\n"
"  1  a condition was violated, or a verdict was indeterminate, or a --verify comparison\n"
"     differed\n"
"  2  usage error, a condition file that would not load, or a capture that would not read\n"
"  4  an exception escaped, which nothing here should ever produce - rule 7 of the\n"
"     brief is \"Never throw\". It is a defect in the harness, reported as one and\n"
"     never as a result about anything this tool was asked to read\n";

struct Options {
    std::string conditionsPath;
    std::string writeName;
    std::string verifyName;
    bool write = false;
    bool verify = false;
    bool quiet = false;
};

// `verdictJson` lives in src/assert/Judge.cpp, not here. The live campaign and this tool must
// render a verdict to the same bytes, because CR-CAP-1's identity check compares the two files
// byte for byte - and two renderers eventually differ by a space.

void printVerdicts(const assertion::JudgeResult& r, const Options& opt) {
    if (opt.quiet) { return; }
    for (const auto& v : r.verdicts) {
        std::printf("    %s\n", assertion::verdictLine(v).c_str());
        if (v.outcome != assertion::Outcome::Satisfied) {
            std::printf("      %s\n", v.reason.c_str());
        }
    }
}

// CR-EX-5's four-outcome mapping, in one place.
//
// `fail` is exactly *"the capture was read successfully AND a declared condition was evaluated
// and not met"* — where "not met" means the asserted expectation was violated, not merely that
// the answer was negative. A condition asserting non-occurrence is satisfied by a not-met state.
//
// Two cases route to `infrastructure_error` rather than to a test result, and both are the
// honest reading of CR-EX-5's *"an unreadable or structurally unsound capture"*:
//
//   - **No running segment.** R14's shape: applying a parameter before `start` can land between
//     two publications of the roster burst and leave segment 0 `frozen`. Nothing in such a
//     capture can be judged. It is not a determinism failure and it is certainly not a failing
//     scenario, so it is neither `pass` nor `fail`.
//   - **Nothing decided.** Every verdict indeterminate, with a running segment present. Calling
//     that `pass` is precisely the "all passed having checked nothing" failure CR-AS-1 exists to
//     prevent, turned on the verdicts instead of on the loader.
std::string outcomeOf(const assertion::JudgeResult& r) {
    if (r.rejected || !r.conformant) { return "infrastructure_error"; }
    if (!r.judgeable) { return "infrastructure_error"; }
    if (r.violated > 0) { return "fail"; }
    if (r.satisfied == 0) { return "infrastructure_error"; }
    return "pass";
}

bool writeLines(const std::filesystem::path& path, const std::vector<std::string>& lines) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) { return false; }
    for (const auto& l : lines) { out << l << "\n"; }
    return out.good();
}

bool readLinesFrom(const std::filesystem::path& path, std::vector<std::string>& lines) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { return false; }
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) { line.pop_back(); }
        lines.push_back(line);
    }
    return true;
}

// CR-CAP-1's second acceptance criterion, checked rather than asserted. Identity here is
// byte-for-byte over the whole verdict file, because a comparison that only looked at the state
// would pass while the numbers a person acts on had drifted.
int verifyAgainst(const std::filesystem::path& path,
                  const std::vector<std::string>& produced,
                  const std::string& label) {
    std::vector<std::string> stored;
    if (!readLinesFrom(path, stored)) {
        std::printf("    VERIFY: %s has no %s to compare against\n",
                    label.c_str(), path.filename().string().c_str());
        return 1;
    }
    if (stored.size() != produced.size()) {
        std::printf("    VERIFY FAILED: %s has %zu verdict(s), this re-judgement produced %zu\n",
                    path.filename().string().c_str(), stored.size(), produced.size());
        return 1;
    }
    for (std::size_t i = 0; i < stored.size(); ++i) {
        if (stored[i] != produced[i]) {
            std::printf("    VERIFY FAILED: line %zu differs\n", i + 1);
            std::printf("      live     : %s\n", stored[i].c_str());
            std::printf("      re-judged: %s\n", produced[i].c_str());
            return 1;
        }
    }
    std::printf("    verify: %zu verdict(s) byte-identical to the live run's %s\n",
                produced.size(), path.filename().string().c_str());
    return 0;
}

int judgeOne(const std::filesystem::path& capture,
             const std::filesystem::path& runDir,
             const assertion::ConditionFile& conditions,
             const Options& opt,
             const std::string& label,
             long long counts[4],
             long long& indeterminateVerdicts) {
    assertion::JudgeResult r;
    assertion::judgeCapture(capture.string(), conditions, r);

    const std::string outcome = outcomeOf(r);
    if (outcome == "pass") { ++counts[0]; }
    else if (outcome == "fail") { ++counts[1]; }
    else { ++counts[3]; }
    indeterminateVerdicts += r.indeterminate;

    // Both vocabularies, side by side and never merged. The facts are met / not met; whether
    // they were the asserted ones is satisfied / violated. A run fails on a violation, not on a
    // negative answer — a condition asserting non-occurrence is satisfied by a not-met state.
    std::printf("  %s  %-20s  satisfied %lld  violated %lld  indeterminate %lld"
                "   (met %lld, not met %lld)\n",
                label.c_str(), outcome.c_str(),
                r.satisfied, r.violated, r.indeterminate, r.met, r.notMet);
    if (r.rejected) {
        std::printf("    capture rejected: %s\n", r.rejectReason.c_str());
        return 2;
    }
    if (!r.judgeable) {
        std::printf("    %s\n", r.notJudgeableReason.c_str());
    }
    printVerdicts(r, opt);

    std::vector<std::string> lines;
    for (const auto& v : r.verdicts) { lines.push_back(assertion::verdictJson(v)); }

    int rc = (r.violated > 0 || r.indeterminate > 0) ? 1 : 0;
    if (opt.write && !runDir.empty()) {
        if (!writeLines(runDir / opt.writeName, lines)) {
            std::fprintf(stderr, "n8ro-judge: could not write %s\n",
                         (runDir / opt.writeName).string().c_str());
            return 2;
        }
    }
    if (opt.verify && !runDir.empty()) {
        rc = std::max(rc, verifyAgainst(runDir / opt.verifyName, lines, label));
    }
    return rc;
}

int commandCampaign(const std::string& dir,
                    const assertion::ConditionFile& conditions,
                    const Options& opt) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path runs = fs::path(dir) / "runs";
    if (!fs::is_directory(runs, ec)) {
        std::fprintf(stderr, "n8ro-judge: no runs directory under %s\n", dir.c_str());
        return 2;
    }
    std::vector<fs::path> runDirs;
    for (const fs::directory_entry& e : fs::directory_iterator(runs, ec)) {
        if (e.is_directory(ec)) { runDirs.push_back(e.path()); }
    }
    std::sort(runDirs.begin(), runDirs.end());

    std::printf("re-judging %s\n", dir.c_str());
    std::printf("  %zu condition(s) from %s\n",
                conditions.conditions.size(), conditions.path.c_str());
    std::printf("  no host is started and no bus subscription is made - this binary links "
                "nothing that could\n\n");

    long long counts[4] = {0, 0, 0, 0};   // pass, fail, timeout, infrastructure_error
    long long indeterminateVerdicts = 0;
    int worst = 0;
    long long judged = 0;

    for (const fs::path& run : runDirs) {
        std::vector<fs::path> captures;
        for (const fs::directory_entry& e : fs::directory_iterator(run, ec)) {
            const std::string name = e.path().filename().string();
            if (name.size() > 14 && name.rfind(".n8rocap.jsonl") == name.size() - 14 &&
                name.find(".part") == std::string::npos) {
                captures.push_back(e.path());
            }
        }
        std::sort(captures.begin(), captures.end());
        for (const fs::path& c : captures) {
            ++judged;
            worst = std::max(worst, judgeOne(c, run, conditions, opt,
                                             run.filename().string(), counts,
                                             indeterminateVerdicts));
        }
    }

    if (judged == 0) {
        std::fprintf(stderr, "n8ro-judge: no captures found under %s\n", runs.string().c_str());
        return 2;
    }

    std::printf("\n  %lld run(s) judged\n", judged);
    std::printf("    pass                  %lld\n", counts[0]);
    std::printf("    fail                  %lld\n", counts[1]);
    std::printf("    timeout               %lld\n", counts[2]);
    std::printf("    infrastructure_error  %lld\n", counts[3]);
    std::printf("    (the four outcomes sum to %lld, and no aggregate merges two of them)\n",
                counts[0] + counts[1] + counts[2] + counts[3]);
    std::printf("    indeterminate VERDICTS %lld - a verdict state, never a fifth run "
                "outcome\n", indeterminateVerdicts);
    return worst;
}

} // namespace

int runMain(int argc, char** argv) {
    Options opt;
    std::string command;
    std::string target;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto next = [&](const char* what) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "n8ro-judge: %s needs a value\n", what);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--help" || a == "-h") { std::fputs(kHelp, stdout); return 0; }
        else if (a == "--conditions") { opt.conditionsPath = next("--conditions"); }
        else if (a == "--write") { opt.write = true; opt.writeName = next("--write"); }
        else if (a == "--verify") { opt.verify = true; opt.verifyName = next("--verify"); }
        else if (a == "--quiet") { opt.quiet = true; }
        else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "n8ro-judge: unknown option %s\n", a.c_str());
            return 2;
        } else if (command.empty()) { command = a; }
        else if (target.empty()) { target = a; }
        else {
            std::fprintf(stderr, "n8ro-judge: unexpected argument %s\n", a.c_str());
            return 2;
        }
    }

    if (command.empty()) { std::fputs(kHelp, stdout); return 2; }
    if (opt.conditionsPath.empty()) {
        std::fprintf(stderr, "n8ro-judge: --conditions is required\n");
        return 2;
    }
    if (opt.verify && opt.verifyName.empty()) { opt.verifyName = "verdicts.jsonl"; }

    // CR-AS-1: the condition file is loaded and validated FIRST, and a named parse error is a
    // non-zero exit before anything else happens at all.
    assertion::ConditionFile conditions;
    assertion::ParseError perr;
    if (!assertion::readConditionFile(opt.conditionsPath, conditions, perr)) {
        std::fprintf(stderr, "n8ro-judge: condition file refused - %s\n",
                     perr.message().c_str());
        return 2;
    }

    if (command == "check") {
        std::printf("%s: %zu condition(s), all valid\n",
                    opt.conditionsPath.c_str(), conditions.conditions.size());
        for (const auto& c : conditions.conditions) {
            std::printf("  %-32s %-15s expect %s\n", c.id.c_str(),
                        assertion::toString(c.kind), assertion::toString(c.expect));
        }
        return 0;
    }

    if (target.empty()) {
        std::fprintf(stderr, "n8ro-judge: %s needs a target\n", command.c_str());
        return 2;
    }

    if (command == "capture") {
        long long counts[4] = {0, 0, 0, 0};
        long long indeterminate = 0;
        if (opt.write && opt.writeName.empty()) { opt.writeName = "verdicts-rejudge.jsonl"; }
        std::printf("judging %s\n", target.c_str());
        return judgeOne(std::filesystem::path(target),
                        std::filesystem::path(target).parent_path(),
                        conditions, opt, "capture", counts, indeterminate);
    }
    if (command == "campaign") {
        if (opt.write && opt.writeName.empty()) { opt.writeName = "verdicts-rejudge.jsonl"; }
        return commandCampaign(target, conditions, opt);
    }

    std::fprintf(stderr, "n8ro-judge: unknown command %s\n", command.c_str());
    return 2;
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
        std::fprintf(stderr, "n8ro-judge: an exception escaped - %s\n", e.what());
    } catch (...) {
        std::fprintf(stderr, "n8ro-judge: a non-standard exception escaped\n");
    }
    std::fprintf(stderr, "n8ro-judge: this is a defect in the harness, not a result about "
                         "anything it was asked to read.\n");
    return 4;
}
