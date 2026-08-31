#include "StopPredicate.h"

namespace ext17::run {

StopPredicate StopPredicate::frameBudget(std::uint64_t frames) {
    StopPredicate p;
    p.kind_ = StopPredicateKind::FrameBudget;
    p.frameBudget_ = frames;
    return p;
}

const char* StopPredicate::kindName() const {
    switch (kind_) {
        case StopPredicateKind::FrameBudget: return "frame_budget";
    }
    return "unknown";
}

std::string StopPredicate::statement() const {
    switch (kind_) {
        case StopPredicateKind::FrameBudget:
            return "A run is finished when the engine's frame number, as published on "
                   "sim/engine/state, reaches " + std::to_string(frameBudget_) + ".";
    }
    return "unknown stop predicate";
}

bool StopPredicate::satisfiedBy(const control::EngineSnapshot& s) const {
    switch (kind_) {
        case StopPredicateKind::FrameBudget:
            return s.frame >= frameBudget_;
    }
    return false;
}

StopEvaluation StopPredicate::evaluate(const control::EngineSnapshot& s) const {
    StopEvaluation e;
    e.satisfied = satisfiedBy(s);
    e.observedFrame = s.frame;
    e.observedSimTimeS = s.simTimeS;
    e.observedDeltaS = s.deltaS;
    return e;
}

} // namespace ext17::run
