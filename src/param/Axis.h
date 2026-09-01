// EXT-17 — the one parameterisation axis, declared in campaign configuration (CR-PAR-1).
//
// [B]: *"Parameterisation — vary the run: initial positions and velocities, which entities are
// present, which scenario from the catalogue. One axis done properly beats four done loosely."*
// Which of the three is OQ-4, and it is **decided** at M5: initial positions and velocities,
// realised as one declared scalar, decided by exercising a range rather than by argument.
//
// This file is the axis's *model and its parser* and nothing else. It links no SDK: an axis is a
// declaration, and turning a declaration into bus traffic is `n8ro-campaign`'s job through the
// `RunConfig::afterLoadBeforeStart` seam M2 built. Keeping the two apart is what lets the whole
// of CR-PAR-1's configuration surface be tested without a simulator.
//
// Three rules here are requirements rather than taste.
//
//   - **The declared text of a value is authoritative; the double is derived.** A value written
//     `27.5` reaches the run record and the report as `27.5`, character for character, and the
//     double exists only to publish it and to order the sweep. M4 closed CR-DET-2's locale
//     hazard by never converting a number for a decision, and a report that printed a
//     re-formatted double would put it back on a path `tools/n8ro-compare/build.cmd` searches.
//     The conversion that does happen goes through the C locale explicitly.
//
//   - **Entities are named, never matched.** There is no glob. A pattern would have to be
//     resolved against the loaded roster, and the control path deliberately does not subscribe
//     to `sim/entity/state` — adding a subscription to expand a wildcard would perturb the
//     publication schedule the determinism gate measures. A pattern that silently matches
//     nothing is also exactly the failure this project keeps finding, so a name that never
//     appears in the run's own capture is reported rather than shrugged at.
//
//   - **Never throws** (constraint C3). Every failure is `false` plus a named error.
#pragma once

#include <array>
#include <string>
#include <vector>

namespace ext17::param {

// What the scalar does to the entities it names. One kind is implemented, deliberately — [B]
// settles the count at one and OQ-4 settles which. Any other spelling in a campaign file is a
// named refusal that says what is implemented, not a silent no-op.
enum class Kind {
    VelocityNedScaled,   // "velocity_ned_scaled": velocityNed = direction * value
};

const char* toString(Kind kind);

// One entity the axis acts on, with the NED direction the value scales. The direction is
// declared rather than discovered for the reason above: nothing here reads the roster.
struct Target {
    std::string entity;
    std::array<double, 3> directionNed{0.0, 0.0, 0.0};
};

// One point of the sweep. `text` is what the author wrote; `number` is derived from it.
struct Value {
    std::string text;
    double number = 0.0;
};

struct Axis {
    std::string name;          // report label and run-record key, e.g. "red_raid_speed_ms"
    std::string appliesTo;     // free text for the report, e.g. "velocityNed magnitude"
    std::string units;         // free text, e.g. "m/s"
    Kind kind = Kind::VelocityNedScaled;

    std::vector<Target> targets;
    std::vector<Value> values;

    // Which value the determinism gate runs at. CR-DET-1 says "the same configuration twice"
    // and a sweep has many, so the gate runs at ONE of them and the report says which — and
    // says that determinism is established for that value rather than for the sweep. Defaults
    // to the first declared value, which is why it is never empty after a successful parse.
    std::string selfTestValueText;

    [[nodiscard]] bool empty() const { return values.empty(); }

    // The values in sweep order: ascending by number, and ties broken by declaration order so
    // that a repeated value keeps the order the author wrote. Returns indices into `values`.
    // A repeated value is allowed on purpose: two campaign runs at one value are one
    // configuration, which is CR-PAR-1's third criterion said as a sweep rather than as a gate.
    [[nodiscard]] std::vector<std::size_t> sweepOrder() const;

    // The value the gate runs at, or nullptr if `selfTestValueText` names none.
    [[nodiscard]] const Value* selfTestValue() const;

    // velocityNed for one target at one value, under `kind`.
    [[nodiscard]] std::array<double, 3> velocityFor(const Target& target, const Value& value) const;
};

// Reads a campaign configuration file. Returns false and fills `error` with a named reason.
// The file is ours, not `contract/`'s: an unrecognised key here is a mistake worth reporting,
// which is the opposite of the capture format's rule (§13) and deliberately so.
bool readCampaignFile(const std::string& path, Axis& out, std::string& error);

// Parses configuration text directly. `readCampaignFile` is this plus file reading; the tests
// use this so that a malformed axis needs no temporary file.
bool parseCampaignText(const std::string& text, Axis& out, std::string& error);

} // namespace ext17::param
