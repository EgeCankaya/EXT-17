// EXT-17 — the conditions a campaign judges its runs against, and the loader for the file they
// are declared in. CR-AS-1, CR-AS-3, ADR-5, and OQ-5.
//
// **OQ-5 is decided at M6: the declaration shape of `contract/condition-file-schema.md` is
// adopted verbatim, and three rules around it are superseded.** The decision was taken by
// writing M6's conditions in the vendored shape and evaluating them against the seven committed
// captures rather than by reading the schema — `docs/m6-oq5.md` carries the measurement. A file
// written for EXT-08's referee parses here.
//
// This file is the conditions' *model and their loader* and nothing else. It **links nothing** —
// not EXT-08, not the N8RO SDK, not a third-party JSON library — the fourth component under that
// rule after `src/capture/`, `src/compare/` and `src/param/`, and for the same reason: a
// condition is a declaration, and going and looking at a simulation is somebody else's job. That
// is what puts the whole of CR-AS-1's and CR-AS-3's surface into `tests/assertion_test.cpp`,
// which needs no install.
//
// ## The three rules that are superseded, each measured rather than argued
//
// **1. The unknown-key rule is inverted.** The vendored digest says an unrecognised key is
// ignored — which is `capture-format-v1.md` §13's rule, correct for a *capture*, where a
// producer adds keys and an old reader must survive them. A condition file is not a capture: a
// person writes it. M5 measured what §13's rule does to a human-authored file (**F-23**) — a
// campaign file accepted `"value"` for `"values"` and the sweep silently did not happen. The
// condition-file equivalent is `"within_meters"` for `"within_m"`: a proximity condition with no
// threshold, in a campaign that then reports twenty confident passes. So an unknown key and a
// key written twice are each **refused by name**, and a key beginning with `_` is a comment —
// which is the only thing the vendored rule was buying.
//
// **2. A teardown removal is not a terminal state.** Measured over the seven committed captures:
// **267 of 385** `entity_remove` records carry `reason: "scenario_unload"`, every surviving
// entity at teardown, all at `sim_time_s` 0. A condition on that reason is met in every run for
// every entity, and its deciding time points at the wrong end of the run. [B] asks *"did
// anything reach a terminal state it should not have"*; being unloaded at the end is not that.
// Naming it is a parse error rather than a condition that is trivially true.
//
// **3. The verdict is three-valued.** That belongs to `Judge.h`, not here, but it is the third
// of the three: the vendored `met: false` at end of run is a conclusion drawn from absence, and
// CR-AS-4 forbids exactly that.
//
// ## Two limits, recorded rather than designed around
//
// **There is no entity pattern.** M5's cleanest binary flip — *"no CIWS gun engaged"* — is
// therefore not expressible: the gun rounds are named `BlueGun_East_01_wpn_44749_4` and the
// numeric parts are generated per run. Two other conditions flip across the same sweep and are
// expressible, so nothing CR-PAR-2 requires is lost. Note that M5's *reason* for refusing a
// glob — that resolving one would perturb the publication schedule the determinism gate
// measures — does not apply here, because a condition is evaluated over a stored capture with no
// host and no subscription. It is declined on merit, and CR-AS-3 closes the vocabulary anyway.
//
// ## The one key that is ADDED, and why the shape could not be adopted without it
//
// **`expect`, taking `met` (the default) or `not_met`.** This is the single addition to the
// vendored declaration shape, and it was found by trying rather than by reading — see
// `docs/m6-oq5.md` §3.5.
//
// The vendored schema is a **referee**: it reports whether each condition was satisfied, and
// says nothing about whether being satisfied is good news. That is complete for EXT-08, which
// judges one live run and prints facts. EXT-17 must map those facts onto CR-EX-5's four run
// outcomes, and the PRD fixes the mapping — *"a run is `fail` only when the capture was read
// successfully and a declared condition was evaluated and not met"* — which reads every
// condition as a thing that **should hold**.
//
// Two of [B]'s three questions survive that reading. The third does not: *"did anything reach a
// terminal state it should not have"* is an assertion of **non-occurrence**, and the vendored
// shape can express one only for the `area` kind, through `test: inside | outside`. Written
// without polarity, `command-centre-destroyed` asserts that the command centre *should* be
// destroyed — the exact inverse of the question [B] asks. A required condition that cannot be
// expressed is precisely what the PRD's resolution rule says to act on.
//
// So `expect` is added, and it is deliberately kept **outside** the verdict rather than folded
// into it. A verdict still records the fact — met, not met, or undecidable — in the vendored
// schema's own terms, so EXT-08's verdicts and EXT-17's stay directly comparable, which ADR-5
// names as a benefit of adopting. Whether that fact is *welcome* is a separate field, and a
// separate question.
//
// ## Never throws (constraint C3)
//
// Every failure is `false` plus a named `ParseError`. CR-AS-1 requires the named error and a
// non-zero exit **before any host is started**, so this runs first and the campaign never
// reaches bring-up on a bad file.
#pragma once

#include <array>
#include <string>
#include <vector>

namespace ext17::assertion {

// The vocabulary, closed at the three kinds [B] names. A fourth spelling is a named parse error
// and never a skipped condition — ADR-5 contains the expressiveness rabbit hole with a
// PRD-revision requirement rather than with discipline.
enum class Kind {
    Proximity,       // "proximity"      — did two entities come within a distance
    Area,            // "area"           — was an entity inside (or outside) a region
    TerminalState,   // "terminal_state" — did an entity reach a terminal state
};

const char* toString(Kind kind);

// What the campaign asserts about the answer. The verdict states the fact; this states whether
// the fact is the one that should hold. See the header — it is the one key added to the vendored
// shape, and it is what makes [B]'s *"a terminal state it should not have"* expressible.
enum class Expect { Met, NotMet };

const char* toString(Expect e);

enum class AreaShape { Circle, Polygon };
enum class AreaTest { Inside, Outside };

// `terminal_state` has two forms and they are not interchangeable — CR-AS-4 classifies them
// differently, because one is witnessed by a normative invariant and the other by nothing.
enum class TerminalForm {
    RemovalReason,   // "removal_reason" — matched verbatim against `entity_remove.reason`
    FieldEquals,     // "field" + "equals" — matched against a sample's field value
};

struct Condition {
    std::string id;
    Kind kind = Kind::Proximity;

    // Defaults to `Met`, so a file written for EXT-08's referee — which has no such key — reads
    // here with every condition asserted to hold, which is the reading CR-EX-5 describes.
    Expect expect = Expect::Met;

    // --- proximity ---------------------------------------------------------------------
    // Exactly two entity names. Naming one twice is refused: it is met at distance zero, so it
    // is a condition that cannot fail and therefore cannot be evidence.
    std::string entityA;
    std::string entityB;
    double withinM = 0.0;
    std::string withinMText;   // the declared text, kept for the report (M5's rule)

    // --- area --------------------------------------------------------------------------
    std::string entity;
    AreaTest test = AreaTest::Inside;
    AreaShape shape = AreaShape::Circle;
    std::array<double, 3> centre{0.0, 0.0, 0.0};   // [lat°, lon°, alt m]; alt defaults to 0
    double radiusM = 0.0;
    std::string radiusMText;
    std::vector<std::array<double, 2>> vertices;   // [lat°, lon°], at least three

    // --- terminal_state ----------------------------------------------------------------
    TerminalForm form = TerminalForm::RemovalReason;
    std::string removalReason;
    std::string field;
    std::string equals;

    // Every entity this condition names, in declaration order. The judge tracks exactly these
    // and no others, which is what keeps a streaming reader streaming.
    [[nodiscard]] std::vector<std::string> namedEntities() const;
};

// Why a condition file was refused. Each is a distinct named error, because CR-AS-1 asks for
// distinct named errors and because "the file is bad" is not something anybody can act on.
enum class ParseCode {
    Ok = 0,
    FileUnreadable,
    FileTooLarge,
    MalformedJson,
    NotAnObject,
    MissingConditions,       // no "conditions" key at all
    ConditionsNotAnArray,
    NoConditionsDeclared,    // an empty array. Reported explicitly, never as a pass (CR-AS-1)
    UnknownKey,              // superseding rule 1 — see the header
    DuplicateKey,            // superseding rule 1
    DuplicateId,             // CR-AS-1, named explicitly
    UnrecognisedKind,        // CR-AS-3, named explicitly
    MissingId,
    MissingKey,
    BadValue,
    EntityNamedTwice,        // proximity naming one entity twice: met at distance zero
    BothTerminalForms,       // "field"+"equals" AND "removal_reason": use one form or the other
    TeardownIsNotTerminal,   // superseding rule 2 — see the header
    PolygonSpansSeam,        // a polygon crossing the antimeridian or a pole is not supported
};

const char* toString(ParseCode code);

struct ParseError {
    ParseCode code = ParseCode::Ok;
    std::string conditionId;   // when the error is attributable to one condition
    std::size_t index = 0;     // 0-based position in the array, when it is
    std::string detail;        // always names the values involved, never just the rule

    [[nodiscard]] std::string message() const;
};

struct ConditionFile {
    std::string path;
    std::vector<Condition> conditions;

    [[nodiscard]] bool empty() const { return conditions.empty(); }
};

// Reads and validates a condition file. Returns false with a named `ParseError`. CR-AS-1: this
// is called before any host is started, and a false return stops the campaign with a non-zero
// exit having attempted no run.
bool readConditionFile(const std::string& path, ConditionFile& out, ParseError& error);

// Parses condition text directly. `readConditionFile` is this plus file reading; the tests use
// this so that a malformed condition file needs no temporary file.
bool parseConditionText(const std::string& text,
                        const std::string& label,
                        ConditionFile& out,
                        ParseError& error);

// The `entity_remove.reason` the engine's stop path writes for every surviving entity. Exposed
// because both the loader and the judge must agree on it, and a second spelling in a second
// file is how they would come to disagree.
//
// Three vocabularies are easy to conflate here and a search over a capture does conflate them:
//   `scenario_unload`   is an `entity_remove.reason`   — this one
//   `scenario_unloaded` is a `segment_close.reason`
//   `host_lost`         is a `trailer.end_reason`
constexpr const char* kTeardownRemovalReason = "scenario_unload";

} // namespace ext17::assertion
