// EXT-17 — "the run is finished", as an object rather than a condition buried in a loop.
//
// CR-EX-3 requires three things of the end of a run, and the shape here exists to make all
// three structural rather than remembered:
//
//   1. The predicate is **explicit and stated** — `statement()` is the one sentence that goes
//      into the README and into every per-run record.
//   2. The **value it evaluated to** is recorded per run — `evaluate()` returns not just a
//      yes/no but the quantity it looked at, so a report can show twenty runs ending at the
//      same point rather than asserting that they did.
//   3. **No wall-clock quantity participates.** The only input is an EngineSnapshot, which
//      carries frame, simulation time and dt and nothing else. The type system is the
//      enforcement: there is no clock in scope to read.
//
// The kind is a closed set of one at M2. That is deliberate — OQ-1 is decided against measured
// evidence, and a second kind would be a decision made by having somewhere to put it. Adding one is a documented change, not an implementation choice.
#pragma once

#include "../control/EngineControl.h"

#include <cstdint>
#include <string>

namespace ext17::run {

enum class StopPredicateKind {
    FrameBudget,   // frame number, off sim/engine/state, reaches or passes a stated N
};

// What the predicate saw. `observedFrame` is the frame at the publication on which the
// predicate first held — the quantity CR-EX-3's "same point by the predicate's own measure"
// criterion is checked against, and the one the M2 twenty-run experiment tabulates.
struct StopEvaluation {
    bool satisfied = false;
    std::uint64_t observedFrame = 0;
    double observedSimTimeS = 0.0;
    double observedDeltaS = 0.0;
};

class StopPredicate {
public:
    static StopPredicate frameBudget(std::uint64_t frames);

    [[nodiscard]] StopPredicateKind kind() const { return kind_; }
    [[nodiscard]] const char* kindName() const;
    [[nodiscard]] std::uint64_t frameBudgetValue() const { return frameBudget_; }

    // The one sentence CR-EX-3 requires in the README and in every per-run record.
    [[nodiscard]] std::string statement() const;

    [[nodiscard]] bool satisfiedBy(const control::EngineSnapshot& s) const;
    [[nodiscard]] StopEvaluation evaluate(const control::EngineSnapshot& s) const;

private:
    StopPredicate() = default;

    StopPredicateKind kind_ = StopPredicateKind::FrameBudget;
    std::uint64_t frameBudget_ = 0;
};

} // namespace ext17::run
