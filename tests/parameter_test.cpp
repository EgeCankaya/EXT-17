// EXT-17 — the parameterisation axis's tests. CR-PAR-1, and the half of CR-PAR-2 that is not
// about a simulator.
//
// Links nothing: not EXT-08, not the N8RO SDK. `tests\build.cmd` builds and runs it, and it
// needs no N8RO install. That is the point of `src/param/Axis` being a declaration and a parser
// with no bus in it — **the whole of CR-PAR-1's configuration surface is checkable here**, and
// the only part that needs a simulator is whether the platform honours the value, which is
// measured against real runs.
//
// Three groups of check, and the third is the one worth having.
//
//   **The axis parses, and refuses by name.** A campaign file is written by a person, so every
//   way of getting it wrong should produce a sentence that says what is wrong — never a default,
//   and never a silent zero in the middle of a sweep.
//
//   **A value's declared TEXT survives.** `27.5` reaches the run record as `27.5`. M4 closed
//   CR-DET-2's locale hazard by never converting a number for a decision; this is the same rule
//   applied to the parameter, and it is tested the same way — the whole suite is re-run under a
//   comma-decimal locale and every answer must be identical.
//
//   **Two runs at different values are never a self-test pair.** The guarantee is structural —
//   both gate runs are copies of one `RunConfig` — so what is tested is the structure: that the
//   sweep order is by value and not by spelling, that the gated value is one the campaign
//   sweeps, and that a campaign file naming a gate value it does not sweep is refused.
#include "../src/param/Axis.h"

#include <clocale>
#include <cstdio>
#include <string>
#include <vector>

using namespace ext17::param;

namespace {

int g_failures = 0;
int g_checks = 0;

void ok(const std::string& what, bool condition, const std::string& detail = {}) {
    ++g_checks;
    if (condition) {
        std::printf("  ok   %s\n", what.c_str());
    } else {
        ++g_failures;
        std::printf("  FAIL %s\n", what.c_str());
        if (!detail.empty()) { std::printf("       %s\n", detail.c_str()); }
    }
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

void heading(const char* text) { std::printf("\n%s\n", text); }

// A minimal well-formed campaign file, with the parts under test substituted in.
std::string configWith(const std::string& valuesArray,
                       const std::string& extra = {},
                       const std::string& groups = {},
                       const std::string& kind = "velocity_ned_scaled") {
    const std::string g = groups.empty()
        ? std::string("[{\"direction_ned\":[-1,0,0],\"names\":[\"RedUAV_N_01\",\"RedUAV_N_02\"]}]")
        : groups;
    return std::string("{\"axis\":{")
           + "\"name\":\"red_raid_speed_ms\","
           + "\"kind\":\"" + kind + "\","
           + "\"applies_to\":\"velocityNed magnitude\","
           + "\"units\":\"m/s\","
           + "\"entity_groups\":" + g + ","
           + "\"values\":" + valuesArray
           + extra
           + "}}";
}

// ---------------------------------------------------------------------------------------------

void testItParses() {
    heading("CR-PAR-1: the axis is declared in configuration, and parses");

    Axis a;
    std::string err;
    const bool parsed = parseCampaignText(configWith("[\"11\",\"27.5\",\"55\"]"), a, err);
    ok("a well-formed campaign file parses", parsed, err);
    ok("the axis carries its name", a.name == "red_raid_speed_ms");
    ok("...its kind", a.kind == Kind::VelocityNedScaled);
    ok("...its report labels", a.appliesTo == "velocityNed magnitude" && a.units == "m/s");
    ok("entity groups are flattened into named targets", a.targets.size() == 2);
    ok("...each keeping its group's direction",
       a.targets[0].directionNed[0] == -1.0 && a.targets[1].directionNed[0] == -1.0);
    ok("three declared values become three sweep points", a.values.size() == 3);

    // The whole of CR-PAR-1's first criterion: changing the values is an edit to this text.
    Axis b;
    ok("changing the values needs no rebuild - a different file is a different sweep",
       parseCampaignText(configWith("[\"1\",\"2\"]"), b, err) && b.values.size() == 2);
}

void testDeclaredTextIsAuthoritative() {
    heading("CR-PAR-1 + CR-DET-2: a value's DECLARED TEXT is what is carried");

    Axis a;
    std::string err;
    ok("parses", parseCampaignText(configWith("[\"11\",\"27.5\",\"55.0\",\"1e2\"]"), a, err), err);
    ok("\"27.5\" is carried as the text 27.5", a.values[1].text == "27.5", a.values[1].text);
    ok("...and not re-formatted from its double", a.values[1].text != "27.500000");
    ok("\"55.0\" stays \"55.0\" and does not become \"55\"", a.values[2].text == "55.0");
    ok("\"1e2\" stays \"1e2\" and does not become \"100\"", a.values[3].text == "1e2");
    ok("the double is derived from it for ordering only", a.values[3].number == 100.0);

    // A value written as a JSON number rather than a string keeps its characters too - the
    // parser preserves a number's original text, which is the same property the capture reader
    // relies on for §8.3.
    Axis b;
    ok("a value written as a JSON number keeps its own characters",
       parseCampaignText(configWith("[27.5]"), b, err) && b.values[0].text == "27.5",
       b.values.empty() ? err : b.values[0].text);

    // "55" and "55.0" are one number and two labels, and the report prints the label. Matching
    // the gate value on text rather than on the double is what keeps those distinct.
    Axis c;
    ok("a gate value is matched on text, not on the double",
       !parseCampaignText(configWith("[\"55.0\"]", ",\"self_test_value\":\"55\""), c, err));
    ok("...and the refusal says which value it could not find",
       contains(err, "self_test_value") && contains(err, "55"), err);
}

void testRefusals() {
    heading("Every way of getting a campaign file wrong is refused BY NAME");

    Axis a;
    std::string err;

    ok("text that is not JSON is refused",
       !parseCampaignText("{not json", a, err) && contains(err, "not valid JSON"), err);

    ok("a file with no axis object is refused",
       !parseCampaignText("{}", a, err) && contains(err, "\"axis\""), err);
    ok("...and the refusal says why there is no default",
       contains(err, "CR-PAR-1") && contains(err, "rather than in code"), err);

    ok("an axis with no name is refused",
       !parseCampaignText("{\"axis\":{\"values\":[\"1\"]}}", a, err) && contains(err, "name"),
       err);

    ok("an unimplemented kind is refused rather than ignored",
       !parseCampaignText(configWith("[\"1\"]", "", "", "which_entities_are_present"), a, err),
       err);
    ok("...and the refusal names the kind that IS implemented, and why there is one",
       contains(err, "velocity_ned_scaled") && contains(err, "one axis"), err);

    ok("an empty values array is refused - a sweep with no values is not a sweep",
       !parseCampaignText(configWith("[]"), a, err) && contains(err, "non-empty"), err);

    ok("a value that is not a number is refused, not silently read as zero",
       !parseCampaignText(configWith("[\"11\",\"fast\",\"55\"]"), a, err), err);
    ok("...and the refusal quotes the value it could not read", contains(err, "fast"), err);

    ok("a comma-decimal value is refused rather than half-read",
       !parseCampaignText(configWith("[\"1,5\"]"), a, err), err);
    ok("...and the refusal says values are written with a '.' whatever the locale",
       contains(err, "locale"), err);

    ok("an entity group with no direction is refused",
       !parseCampaignText(configWith("[\"1\"]", "", "[{\"names\":[\"E1\"]}]"), a, err)
           && contains(err, "direction_ned"), err);
    ok("a zero direction is refused - the value would scale nothing",
       !parseCampaignText(configWith("[\"1\"]", "",
                                     "[{\"direction_ned\":[0,0,0],\"names\":[\"E1\"]}]"), a, err)
           && contains(err, "scale nothing"), err);
    ok("a two-element direction is refused",
       !parseCampaignText(configWith("[\"1\"]", "",
                                     "[{\"direction_ned\":[1,0],\"names\":[\"E1\"]}]"), a, err),
       err);
    ok("an entity group with no names is refused",
       !parseCampaignText(configWith("[\"1\"]", "", "[{\"direction_ned\":[1,0,0]}]"), a, err),
       err);
    ok("no entity groups at all is refused",
       !parseCampaignText(configWith("[\"1\"]", "", "[]"), a, err), err);

    // The same entity in two groups would publish two directions and the last would win, which
    // is a sweep whose parameter is decided by file order.
    // A campaign file is ours, not `contract/`'s. The capture format's rule is that an
    // unrecognised key is IGNORED (§13), and the reader implements exactly that; here the
    // opposite rule applies, because a person wrote this file and a key written twice means one
    // of their two lines is doing nothing.
    ok("a key written twice in the axis is refused, not resolved first-wins",
       !parseCampaignText("{\"axis\":{\"name\":\"a\",\"name\":\"b\","
                          "\"entity_groups\":[{\"direction_ned\":[1,0,0],\"names\":[\"E\"]}],"
                          "\"values\":[\"1\"]}}", a, err), err);
    ok("...and the refusal names the key and says the second does nothing",
       contains(err, "axis.name") && contains(err, "doing nothing"), err);
    ok("a key written twice at the top level is refused too",
       !parseCampaignText("{\"axis\":{},\"axis\":{}}", a, err) && contains(err, "twice"), err);

    // §13's rule inverted, and deliberately: a producer adds keys and an old reader must
    // survive them; a person writes a campaign file and an unknown key is a typo.
    ok("an unknown key in the axis is refused, not ignored",
       !parseCampaignText(configWith("[\"1\"]", ",\"valeus\":[\"2\"]"), a, err), err);
    ok("...and the refusal says a typo that was ignored would be a sweep that did not happen",
       contains(err, "typo"), err);
    ok("an unknown key at the top level is refused",
       !parseCampaignText("{\"axis\":{},\"extra\":1}", a, err) && contains(err, "extra"), err);
    ok("an unknown key in an entity group is refused",
       !parseCampaignText(configWith("[\"1\"]", "",
                                     "[{\"direction_ned\":[1,0,0],\"names\":[\"E\"],"
                                     "\"speed\":3}]"), a, err) && contains(err, "speed"), err);

    // ...with one carve-out, because JSON has no comments and a configuration file needs them.
    Axis commented;
    ok("a key beginning with '_' is a comment and is accepted everywhere",
       parseCampaignText("{\"_note\":\"why this sweep exists\",\"axis\":{"
                         "\"_note\":\"and why these values\",\"name\":\"a\","
                         "\"entity_groups\":[{\"_note\":\"the north group\","
                         "\"direction_ned\":[1,0,0],\"names\":[\"E\"]}],"
                         "\"values\":[\"1\"]}}", commented, err), err);

    ok("one entity in two groups is refused",
       !parseCampaignText(configWith("[\"1\"]", "",
                                     "[{\"direction_ned\":[1,0,0],\"names\":[\"E1\"]},"
                                     "{\"direction_ned\":[0,1,0],\"names\":[\"E1\"]}]"), a, err)
           && contains(err, "more than one entity group"), err);
}

void testNoPatterns() {
    heading("Entities are NAMED, never matched - and a pattern is refused, not ignored");

    Axis a;
    std::string err;
    const bool refused =
        !parseCampaignText(configWith("[\"1\"]", "",
                                      "[{\"direction_ned\":[1,0,0],\"names\":[\"RedUAV_N_*\"]}]"),
                           a, err);
    ok("a name containing '*' is refused", refused, err);
    // The refusal has to say WHY, because a glob is the obvious thing to reach for and the
    // reason it is absent is not obvious: resolving one means subscribing the control path to
    // sim/entity/state, which perturbs the publication schedule the gate measures.
    ok("...and the refusal gives the reason rather than just the rule",
       contains(err, "sim/entity/state") && contains(err, "determinism gate"), err);
    ok("a name containing '?' is refused too",
       !parseCampaignText(configWith("[\"1\"]", "",
                                     "[{\"direction_ned\":[1,0,0],\"names\":[\"RedUAV_N_0?\"]}]"),
                          a, err), err);
    ok("an empty name is refused",
       !parseCampaignText(configWith("[\"1\"]", "",
                                     "[{\"direction_ned\":[1,0,0],\"names\":[\"\"]}]"), a, err),
       err);
}

void testSweepOrder() {
    heading("CR-PAR-2: the sweep is ordered by parameter VALUE, not by spelling");

    Axis a;
    std::string err;
    ok("parses", parseCampaignText(configWith("[\"110\",\"27.5\",\"9\",\"220\"]"), a, err), err);

    const auto order = a.sweepOrder();
    ok("every declared value appears exactly once in the order", order.size() == 4);
    ok("the order is ascending by number: 9, 27.5, 110, 220",
       a.values[order[0]].text == "9" && a.values[order[1]].text == "27.5"
           && a.values[order[2]].text == "110" && a.values[order[3]].text == "220");

    // Sorted as text, "110" precedes "27.5" and "9" comes last. A sweep ordered by spelling
    // draws a trend that is not there, which is the failure CR-PAR-2's first criterion names.
    ok("...which is NOT the order the same values sort in as text",
       a.values[order[0]].text != "110");

    // A repeated value is allowed: two campaign runs at one value are one configuration, which
    // is CR-PAR-1's third criterion stated as a sweep rather than as a gate.
    Axis b;
    ok("a repeated value is allowed",
       parseCampaignText(configWith("[\"55\",\"11\",\"55\"]"), b, err) && b.values.size() == 3,
       err);
    const auto border = b.sweepOrder();
    ok("...and the two runs at it keep the order the file declared them in",
       b.values[border[0]].text == "11" && border[1] == 0 && border[2] == 2);
}

void testGateValue() {
    heading("CR-DET-1 in a sweep: the gate runs at ONE declared value");

    Axis a;
    std::string err;
    ok("with no self_test_value, the gate runs at the FIRST DECLARED value",
       parseCampaignText(configWith("[\"110\",\"27.5\",\"9\"]"), a, err)
           && a.selfTestValueText == "110", err);
    ok("...which is the value written first, not the lowest one swept",
       a.selfTestValueText != "9");
    ok("the gate value resolves to a value the campaign sweeps",
       a.selfTestValue() != nullptr && a.selfTestValue()->number == 110.0);

    Axis b;
    ok("an explicit self_test_value is honoured",
       parseCampaignText(configWith("[\"110\",\"27.5\",\"9\"]", ",\"self_test_value\":\"27.5\""),
                         b, err)
           && b.selfTestValueText == "27.5", err);

    // The gate must establish something about a run the campaign actually performs. A value
    // outside the sweep would gate a configuration nobody runs, which is the "checked once,
    // elsewhere" shape CR-DET-1 exists to prevent.
    Axis c;
    ok("a self_test_value outside the sweep is refused",
       !parseCampaignText(configWith("[\"110\",\"9\"]", ",\"self_test_value\":\"55\""), c, err),
       err);
    ok("...and the refusal says why it must be one of the swept values",
       contains(err, "a value the campaign actually sweeps"), err);
}

void testVelocity() {
    heading("The scalar becomes a velocity, and nothing else");

    Axis a;
    std::string err;
    ok("parses",
       parseCampaignText(configWith("[\"110\"]", "",
                                    "[{\"direction_ned\":[-1,0,0],\"names\":[\"N1\"]},"
                                    "{\"direction_ned\":[0,-1,0],\"names\":[\"E1\"]}]"), a, err),
       err);

    const auto vN = a.velocityFor(a.targets[0], a.values[0]);
    const auto vE = a.velocityFor(a.targets[1], a.values[0]);
    ok("the north group flies south at the value", vN[0] == -110.0 && vN[1] == 0.0 && vN[2] == 0.0);
    ok("the east group flies west at the same value", vE[0] == 0.0 && vE[1] == -110.0);

    // Position is untouched by construction: there is nowhere in this model to put one. The
    // committed axis varies velocity only, and the raid geometry stays the scenario's own
    // (measured at M5).
    ok("the axis carries no position, so the scenario's own geometry is what flies",
       sizeof(Target) == sizeof(std::string) + sizeof(std::array<double, 3>));
}

// ---------------------------------------------------------------------------------------------

int runAll() {
    testItParses();
    testDeclaredTextIsAuthoritative();
    testRefusals();
    testNoPatterns();
    testSweepOrder();
    testGateValue();
    testVelocity();
    return g_failures;
}

} // namespace

int main() {
    std::printf("parameter_test - CR-PAR-1, CR-PAR-2, and the locale rule applied to a "
                "declared value\n");

    runAll();
    const int afterC = g_failures;
    const int checksC = g_checks;

    // CR-DET-2's fourth hazard, this project's own addition, applied to the axis. A campaign
    // file is read, ordered and reported through code that must not consult the machine's
    // decimal separator - so the whole suite runs again under a comma-decimal locale and every
    // answer must be identical. Under a hopeful strtod, "27.5" reads as 27 here.
    heading("=== re-running every check under German_Germany.1252 ===");
    const char* got = std::setlocale(LC_ALL, "German_Germany.1252");
    if (got == nullptr) {
        std::printf("  ..   German_Germany.1252 is not available on this machine; the locale "
                    "re-run was SKIPPED, and that is reported rather than passed\n");
    } else {
        g_failures = 0;
        g_checks = 0;
        runAll();
        std::setlocale(LC_ALL, "C");
        const bool same = (g_checks == checksC);
        std::printf("\n");
        ok("the same checks ran under both locales", same,
           std::to_string(checksC) + " then " + std::to_string(g_checks));
        ok("and every one of them reached the same answer", g_failures == afterC,
           std::to_string(afterC) + " failure(s) under C, " + std::to_string(g_failures)
               + " under German_Germany.1252");
        g_failures += afterC;
        g_checks += checksC;
    }

    std::printf("\n%d check(s), %d failure(s)\n", g_checks, g_failures);
    if (g_failures != 0) {
        std::printf("parameter_test: FAILED\n");
        return 1;
    }
    std::printf("parameter_test: all checks passed\n");
    return 0;
}
