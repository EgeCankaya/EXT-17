#include "Judge.h"

#include "Geodesy.h"
#include "../capture/CaptureReader.h"
#include "../capture/CaptureSet.h"
#include "../common/Json.h"
#include "../common/JsonParse.h"

#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ext17::assertion {
namespace {

// Two sample times are the same instant when their verbatim text matches — M4's rule, and the
// reason CR-DET-2's locale hazard never reaches a decision here. This epsilon is used only where
// a *double* comparison is unavoidable: asking whether a track reaches the segment's last
// sampled instant, where both sides were parsed from the same file by the same parser.
constexpr double kSameInstantEpsilon = 1e-9;

struct TrackedSample {
    std::string timeText;
    double time = 0.0;
    std::size_t line = 0;
    bool hasPosition = false;
    Geodetic position{0.0, 0.0, 0.0};
    bool hasVelocity = false;
    double speed = 0.0;
    // Only the fields some condition asked about, kept in declaration order.
    std::vector<std::pair<std::string, std::string>> fields;

    [[nodiscard]] const std::string* field(const std::string& name) const {
        for (const auto& f : fields) {
            if (f.first == name) { return &f.second; }
        }
        return nullptr;
    }
};

struct TrackKey {
    capture::SegmentKey segment;
    std::string entity;
    long long occupancy = 0;
    bool operator<(const TrackKey& o) const {
        if (!(segment == o.segment)) { return segment < o.segment; }
        if (entity != o.entity) { return entity < o.entity; }
        return occupancy < o.occupancy;
    }
};

struct Track {
    std::vector<TrackedSample> samples;          // in file order, which is authoritative (§5.2)
    std::map<std::string, std::size_t> byTime;   // verbatim sim_time_s text -> index
    double maxSpeed = 0.0;
    bool removed = false;
    std::string removalReason;
    std::size_t removalLine = 0;
    std::string removalTimeText;

    [[nodiscard]] bool hasGap() const { return samples.size() >= 2; }

    // The largest interval between two consecutive observations of this track. Every unobserved
    // window strictly inside the track is bounded by this.
    [[nodiscard]] double largestGap() const {
        double worst = 0.0;
        for (std::size_t i = 1; i < samples.size(); ++i) {
            const double d = samples[i].time - samples[i - 1].time;
            if (d > worst) { worst = d; }
        }
        return worst;
    }
};

// Collects exactly the entities some condition names, and nothing else — which is what keeps a
// streaming reader streaming over a 24 MB capture.
class Collector : public capture::RecordSink {
public:
    Collector(const std::set<std::string>& entities,
              const std::set<std::string>& fields)
        : entities_(entities), fields_(fields) {}

    void onRecord(const capture::RecordView& view) override {
        if (!view.record) { return; }
        if (view.type == "sample") {
            onSample(view);
        } else if (view.type == "entity_remove") {
            onRemove(view);
        }
    }

    std::map<TrackKey, Track> tracks;

private:
    void onSample(const capture::RecordView& view) {
        const json::Value* entity = view.record->find("entity");
        if (!entity || !entity->isString()) { return; }
        if (entities_.find(entity->text()) == entities_.end()) { return; }

        const json::Value* time = view.record->find("sim_time_s");
        if (!time || !time->isNumber()) { return; }

        TrackKey key;
        key.segment = view.segment;
        key.entity = entity->text();
        key.occupancy = view.record->integerOr("occupancy", 0);

        TrackedSample s;
        s.timeText = time->raw();
        s.time = time->number();
        s.line = view.line;

        if (const json::Value* fields = view.record->find("fields")) {
            if (const json::Value* pos = fields->find("positionGeodetic")) {
                if (pos->isArray() && pos->elements().size() == 3) {
                    bool ok = true;
                    for (std::size_t i = 0; i < 3; ++i) {
                        if (!pos->elements()[i].isNumber()) { ok = false; break; }
                        s.position[i] = pos->elements()[i].number();
                    }
                    s.hasPosition = ok;
                }
            }
            if (const json::Value* vel = fields->find("velocityNed")) {
                if (vel->isArray() && vel->elements().size() == 3) {
                    double sum = 0.0;
                    bool ok = true;
                    for (std::size_t i = 0; i < 3; ++i) {
                        if (!vel->elements()[i].isNumber()) { ok = false; break; }
                        const double v = vel->elements()[i].number();
                        sum += v * v;
                    }
                    if (ok) {
                        s.hasVelocity = true;
                        s.speed = std::sqrt(sum);
                    }
                }
            }
            for (const auto& name : fields_) {
                if (const json::Value* f = fields->find(name.c_str())) {
                    std::string text;
                    if (f->isString()) {
                        text = f->text();
                    } else if (f->isNumber()) {
                        text = f->raw();
                    } else if (f->isBool()) {
                        text = f->boolean() ? "true" : "false";
                    } else {
                        continue;
                    }
                    s.fields.emplace_back(name, text);
                }
            }
        }

        Track& t = tracks[key];
        // A `Frozen` segment repeats an instant; the map keeps the first, and nothing that reads
        // it is used to decide anything, because frozen segments are never evaluated.
        if (t.byTime.find(s.timeText) == t.byTime.end()) {
            t.byTime.emplace(s.timeText, t.samples.size());
        }
        if (s.hasVelocity && s.speed > t.maxSpeed) { t.maxSpeed = s.speed; }
        t.samples.push_back(std::move(s));
    }

    void onRemove(const capture::RecordView& view) {
        const json::Value* entity = view.record->find("entity");
        if (!entity || !entity->isString()) { return; }
        if (entities_.find(entity->text()) == entities_.end()) { return; }

        TrackKey key;
        key.segment = view.segment;
        key.entity = entity->text();
        key.occupancy = view.record->integerOr("occupancy", 0);

        Track& t = tracks[key];
        t.removed = true;
        t.removalReason = view.record->stringOr("reason");
        t.removalLine = view.line;
        if (const json::Value* time = view.record->find("sim_time_s")) {
            if (time->isNumber()) { t.removalTimeText = time->raw(); }
        }
    }

    const std::set<std::string>& entities_;
    const std::set<std::string>& fields_;
};

// A track is bounded over a segment when there is no unobserved window this evaluator cannot
// account for. The head is covered when the track begins at the segment's first sampled instant
// (within one gap of it); the tail is covered when the track runs to the segment's last sampled
// instant, OR when a removal record positively says the occupancy ended — an occupancy that was
// closed did not go anywhere afterwards.
bool trackIsBounded(const Track& t, const capture::SegmentStats& seg, std::string& why) {
    if (!t.hasGap()) {
        why = "it carries fewer than two samples, so no sampling gap can be measured";
        return false;
    }
    const double gap = t.largestGap();
    if (t.samples.front().time > seg.firstSampleTimeS + gap + kSameInstantEpsilon) {
        why = "its first sample is later than the segment's first by more than one sampling "
              "gap, leaving a window at the head that nothing observed";
        return false;
    }
    if (t.samples.back().time < seg.lastSampleTimeS - kSameInstantEpsilon && !t.removed) {
        why = "its samples stop before the segment's last instant and no removal record says "
              "the occupancy ended, leaving a window at the tail that nothing observed";
        return false;
    }
    return true;
}

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ')) { s.pop_back(); }
    return s;
}

} // namespace

// A fixed-precision rendering that never touches the locale. The C library's fixed-point
// formatters read the ambient decimal separator, and on this machine under a German locale that
// is a comma — CR-DET-2's fourth hazard, and the reason this file's sources are searched for
// every spelling of them. Doing the digits by arithmetic removes the hazard rather than
// documenting it. (The search is deliberately blunt and matches prose as well as code, so this
// comment names none of the functions it forbids — a check that fires on its own explanation is
// a check somebody eventually switches off.)
std::string fixed(double value, int decimals) {
    if (std::isnan(value)) { return "nan"; }
    if (std::isinf(value)) { return value < 0 ? "-inf" : "inf"; }

    std::string out;
    if (value < 0.0) { out += '-'; value = -value; }

    double scale = 1.0;
    for (int i = 0; i < decimals; ++i) { scale *= 10.0; }
    // Round half away from zero, explicitly, so the same input always renders the same text.
    double scaled = std::floor(value * scale + 0.5);

    // Digits of the scaled integer, least significant first.
    std::string digits;
    if (scaled < 1.0) {
        digits = "0";
    } else {
        while (scaled >= 1.0) {
            const double next = std::floor(scaled / 10.0);
            const int digit = static_cast<int>(scaled - next * 10.0);
            digits += static_cast<char>('0' + digit);
            scaled = next;
        }
    }
    while (static_cast<int>(digits.size()) <= decimals) { digits += '0'; }

    for (std::size_t i = digits.size(); i > 0; --i) {
        const std::size_t index = i - 1;
        out += digits[index];
        if (decimals > 0 && index == static_cast<std::size_t>(decimals)) { out += '.'; }
    }
    return out;
}

const char* toString(State s) {
    switch (s) {
        case State::Met: return "met";
        case State::NotMet: return "not_met";
        case State::Indeterminate: return "indeterminate";
    }
    return "unknown";
}

const char* toString(Outcome o) {
    switch (o) {
        case Outcome::Satisfied: return "satisfied";
        case Outcome::Violated: return "violated";
        case Outcome::Undetermined: return "undetermined";
    }
    return "unknown";
}

const char* toString(Because b) {
    switch (b) {
        case Because::None: return "none";
        case Because::ThresholdReached: return "threshold_reached";
        case Because::RegionTestSatisfied: return "region_test_satisfied";
        case Because::RemovalReasonMatched: return "removal_reason_matched";
        case Because::FieldValueMatched: return "field_value_matched";
        case Because::ClearedByContinuityBound: return "cleared_by_continuity_bound";
        case Because::EveryOccupancyAccounted: return "every_occupancy_accounted";
        case Because::NoRunningSegment: return "no_running_segment";
        case Because::EntityNeverSampled: return "entity_never_sampled";
        case Because::TrackNotBounded: return "track_not_bounded";
        case Because::MarginWithinBound: return "margin_within_bound";
        case Because::FieldAbsenceNotBoundable: return "field_absence_not_boundable";
        case Because::TooFewSamplesForGap: return "too_few_samples_for_gap";
        case Because::RunCutByRotation: return "run_cut_by_rotation";
    }
    return "unknown";
}

namespace {

// ---------------------------------------------------------------------------------------------
// The evaluator. One function per kind; each returns exactly one verdict.
// ---------------------------------------------------------------------------------------------

struct Context {
    const std::map<TrackKey, Track>* tracks = nullptr;
    const std::vector<capture::SegmentStats>* runningSegments = nullptr;
    std::string capturePath;
};

std::vector<const Track*> tracksFor(const Context& ctx,
                                    const capture::SegmentKey& segment,
                                    const std::string& entity,
                                    std::vector<long long>& occupancies) {
    std::vector<const Track*> out;
    for (const auto& entry : *ctx.tracks) {
        if (!(entry.first.segment == segment)) { continue; }
        if (entry.first.entity != entity) { continue; }
        if (entry.second.samples.empty() && !entry.second.removed) { continue; }
        out.push_back(&entry.second);
        occupancies.push_back(entry.first.occupancy);
    }
    return out;
}

double continuityBound(double relativeSpeed, double gap) {
    return relativeSpeed * gap + 0.5 * kPlatformAccelerationClampMs2 * gap * gap;
}

void finishNotMet(Verdict& v, double margin, double bound, double gap) {
    v.absenceDependent = true;
    v.boundApplied = true;
    v.marginText = fixed(margin, 2);
    v.boundText = fixed(bound, 2);
    v.largestGapText = fixed(gap, 4);
    if (margin > bound) {
        v.state = State::NotMet;
        v.because = Because::ClearedByContinuityBound;
    } else {
        v.state = State::Indeterminate;
        v.because = Because::MarginWithinBound;
    }
}

Verdict judgeProximity(const Condition& c, const Context& ctx) {
    Verdict v;
    v.conditionId = c.id;
    v.kind = c.kind;
    v.capturePath = ctx.capturePath;
    v.measuredName = "closest_approach_m";
    v.thresholdName = "within_m";
    v.thresholdText = c.withinMText;

    bool sawAnyPair = false;
    bool allBounded = true;
    std::string unboundedWhy;
    double bestDistance = 0.0;
    bool haveBest = false;
    double worstGap = 0.0;
    double relativeSpeed = 0.0;
    capture::SegmentKey bestSegment;
    EntityRef bestA;
    EntityRef bestB;

    for (const auto& seg : *ctx.runningSegments) {
        std::vector<long long> occA;
        std::vector<long long> occB;
        const std::vector<const Track*> a = tracksFor(ctx, seg.key, c.entityA, occA);
        const std::vector<const Track*> b = tracksFor(ctx, seg.key, c.entityB, occB);
        if (a.empty() || b.empty()) { continue; }

        for (std::size_t i = 0; i < a.size(); ++i) {
            for (std::size_t j = 0; j < b.size(); ++j) {
                const Track& ta = *a[i];
                const Track& tb = *b[j];
                if (ta.samples.empty() || tb.samples.empty()) { continue; }
                sawAnyPair = true;

                std::string whyA;
                std::string whyB;
                const bool boundedA = trackIsBounded(ta, seg, whyA);
                const bool boundedB = trackIsBounded(tb, seg, whyB);
                if (!boundedA || !boundedB) {
                    allBounded = false;
                    if (unboundedWhy.empty()) {
                        unboundedWhy = (!boundedA ? c.entityA + "@" +
                                                        std::to_string(occA[i]) + " " + whyA
                                                  : c.entityB + "@" +
                                                        std::to_string(occB[j]) + " " + whyB);
                    }
                }
                const double gap = (ta.largestGap() > tb.largestGap()) ? ta.largestGap()
                                                                       : tb.largestGap();
                if (gap > worstGap) { worstGap = gap; }
                const double speed = ta.maxSpeed + tb.maxSpeed;
                if (speed > relativeSpeed) { relativeSpeed = speed; }

                // Walk A in file order — authoritative (§5.2) — and look B up by the verbatim
                // text of sim_time_s. A match is decided on the text and never on a conversion.
                for (const TrackedSample& sa : ta.samples) {
                    if (!sa.hasPosition) { continue; }
                    const auto it = tb.byTime.find(sa.timeText);
                    if (it == tb.byTime.end()) { continue; }
                    const TrackedSample& sb = tb.samples[it->second];
                    if (!sb.hasPosition) { continue; }

                    const double d = distanceM(sa.position, sb.position);
                    if (!haveBest || d < bestDistance) {
                        haveBest = true;
                        bestDistance = d;
                        bestSegment = seg.key;
                        bestA = EntityRef{c.entityA, occA[i], sa.timeText, sa.line};
                        bestB = EntityRef{c.entityB, occB[j], sb.timeText, sb.line};
                    }
                    if (d <= c.withinM) {
                        v.state = State::Met;
                        v.because = Because::ThresholdReached;
                        v.hasSegment = true;
                        v.segment = seg.key;
                        v.entities = {EntityRef{c.entityA, occA[i], sa.timeText, sa.line},
                                      EntityRef{c.entityB, occB[j], sb.timeText, sb.line}};
                        v.decidingSimTimeText = sa.timeText;
                        v.measuredText = fixed(d, 4);
                        v.reason = c.entityA + "@" + std::to_string(occA[i]) + " and " +
                                   c.entityB + "@" + std::to_string(occB[j]) +
                                   " were " + v.measuredText + " m apart at sim_time_s " +
                                   sa.timeText + ", which is within " + c.withinMText +
                                   " m. Decided on samples that are present";
                        return v;
                    }
                }
            }
        }
    }

    if (!sawAnyPair || !haveBest) {
        v.state = State::Indeterminate;
        v.because = Because::EntityNeverSampled;
        v.reason = "no running segment carries a positioned sample for both " + c.entityA +
                   " and " + c.entityB + " at a common sim_time_s, so nothing was compared. "
                   "An unevaluated condition is not a passing one";
        return v;
    }

    v.hasSegment = true;
    v.segment = bestSegment;
    v.entities = {bestA, bestB};
    v.decidingSimTimeText = bestA.simTimeText;
    v.measuredText = fixed(bestDistance, 4);

    if (!allBounded) {
        v.state = State::Indeterminate;
        v.because = Because::TrackNotBounded;
        v.absenceDependent = true;
        v.reason = "they were never observed within " + c.withinMText + " m - closest " +
                   v.measuredText + " m at sim_time_s " + bestA.simTimeText +
                   " - but " + unboundedWhy + ", so a closer approach could have happened "
                   "unobserved. Absence is not evidence (CR-AS-4)";
        return v;
    }

    const double margin = bestDistance - c.withinM;
    const double bound = continuityBound(relativeSpeed, worstGap);
    finishNotMet(v, margin, bound, worstGap);
    if (v.state == State::NotMet) {
        v.reason = "closest approach " + v.measuredText + " m at sim_time_s " +
                   bestA.simTimeText + ", against a threshold of " + c.withinMText +
                   " m. The margin of " + v.marginText + " m exceeds the " + v.boundText +
                   " m they could have closed inside the largest unobserved window (" +
                   v.largestGapText + " s at a relative " + fixed(relativeSpeed, 1) +
                   " m/s), so they did not reach it";
    } else {
        v.reason = "closest observed approach " + v.measuredText + " m against a threshold of " +
                   c.withinMText + " m, a margin of " + v.marginText + " m - which does NOT "
                   "exceed the " + v.boundText + " m they could have closed inside the largest "
                   "unobserved window (" + v.largestGapText + " s). They may have reached it "
                   "between two samples, and this capture cannot say (CR-AS-4)";
    }
    return v;
}

Verdict judgeArea(const Condition& c, const Context& ctx) {
    Verdict v;
    v.conditionId = c.id;
    v.kind = c.kind;
    v.capturePath = ctx.capturePath;
    v.measuredName = "closest_to_boundary_m";
    v.thresholdName = c.shape == AreaShape::Circle ? "radius_m" : "polygon";
    v.thresholdText = c.shape == AreaShape::Circle ? c.radiusMText
                                                   : std::to_string(c.vertices.size()) +
                                                         " vertices";

    const bool wantInside = (c.test == AreaTest::Inside);
    bool sawAny = false;
    bool allBounded = true;
    std::string unboundedWhy;
    bool haveBest = false;
    double bestToBoundary = 0.0;
    double worstGap = 0.0;
    double maxSpeed = 0.0;
    capture::SegmentKey bestSegment;
    EntityRef bestRef;

    for (const auto& seg : *ctx.runningSegments) {
        std::vector<long long> occ;
        const std::vector<const Track*> tracks = tracksFor(ctx, seg.key, c.entity, occ);
        for (std::size_t i = 0; i < tracks.size(); ++i) {
            const Track& t = *tracks[i];
            if (t.samples.empty()) { continue; }
            sawAny = true;

            std::string why;
            if (!trackIsBounded(t, seg, why)) {
                allBounded = false;
                if (unboundedWhy.empty()) {
                    unboundedWhy = c.entity + "@" + std::to_string(occ[i]) + " " + why;
                }
            }
            if (t.largestGap() > worstGap) { worstGap = t.largestGap(); }
            if (t.maxSpeed > maxSpeed) { maxSpeed = t.maxSpeed; }

            for (const TrackedSample& s : t.samples) {
                if (!s.hasPosition) { continue; }
                bool inside = false;
                double toBoundary = 0.0;
                if (c.shape == AreaShape::Circle) {
                    const Geodetic centre{c.centre[0], c.centre[1], c.centre[2]};
                    const double d = distanceM(s.position, centre);
                    inside = (d <= c.radiusM);
                    toBoundary = std::fabs(d - c.radiusM);
                } else {
                    const std::array<double, 2> point{s.position[0], s.position[1]};
                    inside = insidePolygon(point, c.vertices);
                    toBoundary = distanceToPolygonEdgeM(point, c.vertices);
                }
                if (!haveBest || toBoundary < bestToBoundary) {
                    haveBest = true;
                    bestToBoundary = toBoundary;
                    bestSegment = seg.key;
                    bestRef = EntityRef{c.entity, occ[i], s.timeText, s.line};
                }
                if (inside == wantInside) {
                    v.state = State::Met;
                    v.because = Because::RegionTestSatisfied;
                    v.hasSegment = true;
                    v.segment = seg.key;
                    v.entities = {EntityRef{c.entity, occ[i], s.timeText, s.line}};
                    v.decidingSimTimeText = s.timeText;
                    v.measuredText = fixed(toBoundary, 4);
                    v.reason = c.entity + "@" + std::to_string(occ[i]) + " was " +
                               (wantInside ? "inside" : "outside") + " the region at "
                               "sim_time_s " + s.timeText + ", " + v.measuredText +
                               " m from its boundary. Decided on a sample that is present";
                    return v;
                }
            }
        }
    }

    if (!sawAny || !haveBest) {
        v.state = State::Indeterminate;
        v.because = Because::EntityNeverSampled;
        v.reason = "no running segment carries a positioned sample for " + c.entity +
                   ", so nothing was tested against the region";
        return v;
    }

    v.hasSegment = true;
    v.segment = bestSegment;
    v.entities = {bestRef};
    v.decidingSimTimeText = bestRef.simTimeText;
    v.measuredText = fixed(bestToBoundary, 4);

    if (!allBounded) {
        v.state = State::Indeterminate;
        v.because = Because::TrackNotBounded;
        v.absenceDependent = true;
        v.reason = c.entity + " was never observed " + (wantInside ? "inside" : "outside") +
                   " the region - closest approach to its boundary " + v.measuredText +
                   " m - but " + unboundedWhy + ", so it could have crossed unobserved "
                   "(CR-AS-4)";
        return v;
    }

    const double bound = continuityBound(maxSpeed, worstGap);
    finishNotMet(v, bestToBoundary, bound, worstGap);
    if (v.state == State::NotMet) {
        v.reason = c.entity + " was never " + (wantInside ? "inside" : "outside") +
                   " the region. It came within " + v.measuredText +
                   " m of its boundary at sim_time_s " + bestRef.simTimeText +
                   ", which exceeds the " + v.boundText + " m it could have moved inside the "
                   "largest unobserved window (" + v.largestGapText + " s at up to " +
                   fixed(maxSpeed, 1) + " m/s)";
    } else {
        v.reason = c.entity + " was never observed " + (wantInside ? "inside" : "outside") +
                   " the region, but it came within " + v.measuredText + " m of the boundary - "
                   "closer than the " + v.boundText + " m it could have moved inside the "
                   "largest unobserved window (" + v.largestGapText + " s). It may have crossed "
                   "between two samples, and this capture cannot say (CR-AS-4)";
    }
    return v;
}

Verdict judgeTerminalRemoval(const Condition& c, const Context& ctx) {
    Verdict v;
    v.conditionId = c.id;
    v.kind = c.kind;
    v.capturePath = ctx.capturePath;
    // Two different things, so two different names: what the entity was actually removed with,
    // and what the condition asked about. Printing both as "removal_reason" made a satisfied
    // not-met verdict read as though it contradicted itself.
    v.measuredName = "removed_with";
    v.thresholdName = "removal_reason";
    v.thresholdText = c.removalReason;

    bool sawEntity = false;
    bool everyOccupancyAccounted = true;
    std::string unaccounted;

    for (const auto& seg : *ctx.runningSegments) {
        std::vector<long long> occ;
        const std::vector<const Track*> tracks = tracksFor(ctx, seg.key, c.entity, occ);
        for (std::size_t i = 0; i < tracks.size(); ++i) {
            const Track& t = *tracks[i];
            sawEntity = true;

            if (t.removed && t.removalReason == c.removalReason) {
                v.state = State::Met;
                v.because = Because::RemovalReasonMatched;
                v.hasSegment = true;
                v.segment = seg.key;
                v.entities = {EntityRef{c.entity, occ[i], t.removalTimeText, t.removalLine}};
                v.decidingSimTimeText = t.removalTimeText;
                v.measuredText = t.removalReason;
                v.reason = c.entity + "@" + std::to_string(occ[i]) +
                           " was removed with reason \"" + t.removalReason +
                           "\" at sim_time_s " + t.removalTimeText +
                           ". Decided on an entity_remove record that is present";
                return v;
            }

            // §8.1, the invariant that makes a not-met verdict here sound. Either the occupancy
            // is closed by a record that states some other reason - in which case we know why it
            // ended - or it carries a sample at the segment's last sampled instant, which is
            // positive evidence it had not been removed by then. A sampling gap does not weaken
            // the second: a re-created entity carries a HIGHER occupancy.
            const bool closedByRecord = t.removed;
            const bool ranToTheEnd = !t.samples.empty() &&
                                     t.samples.back().time >=
                                         seg.lastSampleTimeS - kSameInstantEpsilon;
            if (!closedByRecord && !ranToTheEnd) {
                everyOccupancyAccounted = false;
                if (unaccounted.empty()) {
                    unaccounted = c.entity + "@" + std::to_string(occ[i]) +
                                  " carries no removal record and its samples stop at "
                                  "sim_time_s " +
                                  (t.samples.empty() ? std::string("none")
                                                     : t.samples.back().timeText) +
                                  ", before the segment's last instant";
                }
            }
            if (closedByRecord && v.measuredText.empty()) {
                v.measuredText = t.removalReason;
                v.hasSegment = true;
                v.segment = seg.key;
                v.entities = {EntityRef{c.entity, occ[i], t.removalTimeText, t.removalLine}};
            }
        }
    }

    if (!sawEntity) {
        v.state = State::Indeterminate;
        v.because = Because::EntityNeverSampled;
        v.reason = "no running segment mentions " + c.entity + " at all, so whether it reached "
                   "a terminal state was never evaluated";
        return v;
    }

    v.absenceDependent = true;
    if (everyOccupancyAccounted) {
        v.state = State::NotMet;
        v.because = Because::EveryOccupancyAccounted;
        v.reason = c.entity + " was never removed with reason \"" + c.removalReason +
                   "\"" + (v.measuredText.empty() ? std::string()
                                                  : " - it was removed with \"" +
                                                        v.measuredText + "\"") +
                   ". Every occupancy of it is accounted for: each is closed by a removal "
                   "record, or carries a sample at the segment's last instant, which format "
                   "v1 s8.1 makes positive evidence that it had not been removed";
    } else {
        v.state = State::Indeterminate;
        v.because = Because::TrackNotBounded;
        v.reason = c.entity + " was never observed removed with reason \"" + c.removalReason +
                   "\", but " + unaccounted + " - so a removal could have happened and its "
                   "record not been recorded. Absence is not evidence (CR-AS-4)";
    }
    return v;
}

Verdict judgeTerminalField(const Condition& c, const Context& ctx) {
    Verdict v;
    v.conditionId = c.id;
    v.kind = c.kind;
    v.capturePath = ctx.capturePath;
    v.measuredName = c.field;
    v.thresholdName = "equals";
    v.thresholdText = c.equals;

    bool sawEntity = false;
    std::string lastSeen;

    for (const auto& seg : *ctx.runningSegments) {
        std::vector<long long> occ;
        const std::vector<const Track*> tracks = tracksFor(ctx, seg.key, c.entity, occ);
        for (std::size_t i = 0; i < tracks.size(); ++i) {
            const Track& t = *tracks[i];
            if (t.samples.empty()) { continue; }
            sawEntity = true;
            for (const TrackedSample& s : t.samples) {
                const std::string* value = s.field(c.field);
                if (!value) { continue; }
                lastSeen = *value;
                if (*value == c.equals) {
                    v.state = State::Met;
                    v.because = Because::FieldValueMatched;
                    v.hasSegment = true;
                    v.segment = seg.key;
                    v.entities = {EntityRef{c.entity, occ[i], s.timeText, s.line}};
                    v.decidingSimTimeText = s.timeText;
                    v.measuredText = *value;
                    v.reason = c.entity + "@" + std::to_string(occ[i]) + " carried " + c.field +
                               " = \"" + *value + "\" at sim_time_s " + s.timeText +
                               ". Decided on a sample that is present";
                    return v;
                }
            }
        }
    }

    if (!sawEntity) {
        v.state = State::Indeterminate;
        v.because = Because::EntityNeverSampled;
        v.reason = "no running segment carries a sample for " + c.entity + ", so " + c.field +
                   " was never read";
        return v;
    }

    // The one form that is never decidable in the negative. Nothing in the format bounds how
    // fast a field may change, so the value could have been taken and left inside one sampling
    // gap. This is [B]'s own dangerous example in its hardest shape, and the honest answer is
    // that this capture cannot say.
    v.state = State::Indeterminate;
    v.because = Because::FieldAbsenceNotBoundable;
    v.absenceDependent = true;
    v.measuredText = lastSeen;
    v.reason = "no sample of " + c.entity + " carried " + c.field + " = \"" + c.equals +
               "\"" + (lastSeen.empty() ? std::string()
                                        : " - the last value seen was \"" + lastSeen + "\"") +
               ". A field's rate of change is not bounded by anything in the format, so the "
               "value could have been taken and left between two samples. This form is never "
               "decidable in the negative (CR-AS-4)";
    return v;
}

} // namespace

std::string verdictLine(const Verdict& v) {
    std::string out = v.conditionId;
    while (out.size() < 34) { out += ' '; }
    out += ' ';

    std::string state = toString(v.state);
    if (v.state == State::NotMet) { state = "NOT MET"; }
    if (v.state == State::Indeterminate) { state = "INDETERMINATE"; }
    if (v.outcome == Outcome::Violated) { state += " <- VIOLATED"; }
    if (v.expect == Expect::NotMet && v.outcome == Outcome::Satisfied) {
        state += " (as asserted)";
    }
    while (state.size() < 28) { state += ' '; }
    out += state;

    if (!v.decidingSimTimeText.empty()) {
        out += " t=" + v.decidingSimTimeText;
    }
    if (!v.measuredName.empty() && !v.measuredText.empty()) {
        out += " " + v.measuredName + "=" + v.measuredText;
    }
    if (!v.thresholdName.empty() && !v.thresholdText.empty()) {
        out += " " + v.thresholdName + "=" + v.thresholdText;
    }
    if (!v.entities.empty()) {
        out += " [";
        for (std::size_t i = 0; i < v.entities.size(); ++i) {
            if (i) { out += ", "; }
            out += v.entities[i].entity + "@" + std::to_string(v.entities[i].occupancy) +
                   " line " + std::to_string(v.entities[i].line);
        }
        out += "]";
    }
    return trim(out);
}

std::string verdictJson(const Verdict& v) {
    json::Writer w(0);
    w.beginObject();
    w.member("schema", "ext17-verdict/1");
    w.member("condition_id", v.conditionId);
    w.member("kind", toString(v.kind));
    w.member("state", toString(v.state));
    w.member("expect", toString(v.expect));
    w.member("outcome", toString(v.outcome));
    w.member("because", toString(v.because));
    if (v.hasSegment) {
        w.beginObject("segment");
        w.member("part", static_cast<std::int64_t>(v.segment.part));
        w.member("segment", static_cast<std::int64_t>(v.segment.segment));
        w.endObject();
    } else {
        w.memberNull("segment");
    }
    w.beginArray("entities");
    for (const auto& e : v.entities) {
        w.beginObject();
        w.member("entity", e.entity);
        w.member("occupancy", static_cast<std::int64_t>(e.occupancy));
        w.member("sim_time_s", e.simTimeText);
        w.member("line", static_cast<std::int64_t>(e.line));
        w.endObject();
    }
    w.endArray();
    if (v.decidingSimTimeText.empty()) {
        w.memberNull("deciding_sim_time_s");
    } else {
        w.member("deciding_sim_time_s", v.decidingSimTimeText);
    }
    w.member("measured_name", v.measuredName);
    w.member("measured", v.measuredText);
    w.member("threshold_name", v.thresholdName);
    w.member("threshold", v.thresholdText);
    w.member("absence_dependent", v.absenceDependent);
    w.member("bound_applied", v.boundApplied);
    w.member("margin_m", v.marginText);
    w.member("bound_m", v.boundText);
    w.member("largest_gap_s", v.largestGapText);
    w.member("reason", v.reason);
    w.endObject();

    // One verdict is one line. The writer separates members with newlines even at indent width
    // 0, so they come out here - and a newline inside a string value is left alone, because a
    // reason naming an entity whose name contained one would otherwise split into two verdicts.
    //
    // The capture PATH is deliberately not a member. A live judgement runs in the run directory
    // and a re-judgement is handed an absolute path, so including it would make CR-CAP-1's
    // byte-identity check fail on a difference that means nothing. The run record carries the
    // path; the verdict carries the finding.
    const std::string out = w.str();
    std::string flat;
    flat.reserve(out.size());
    bool inString = false;
    for (std::size_t i = 0; i < out.size(); ++i) {
        const char c = out[i];
        if (c == '"' && (i == 0 || out[i - 1] != '\\')) { inString = !inString; }
        if (!inString && (c == '\n' || c == '\r')) { continue; }
        flat += c;
    }
    return flat;
}

namespace {

// What the evaluator needs from a read, whether that read was one file or a rotated SET of
// them. Taking the pieces rather than a `ReadResult` is what lets `judgeCapture` hand it a set
// (format 6.7) and `judgeLines` hand it a capture already in memory, through one evaluator.
struct JudgedRead {
    bool conformant = false;
    const std::vector<capture::SegmentStats>* segments = nullptr;
    std::vector<std::string> diagnosticLines;
    long long parts = 1;
    long long segmentsCutByRotation = 0;
};

bool judgeRead(const JudgedRead& read,
               const std::map<TrackKey, Track>& tracks,
               const ConditionFile& conditions,
               JudgeResult& out) {
    out.conformant = read.conformant;
    out.parts = read.parts;
    out.segmentsCutByRotation = read.segmentsCutByRotation;
    for (const auto& line : read.diagnosticLines) { out.captureDiagnostics.push_back(line); }

    std::vector<capture::SegmentStats> running;
    for (const auto& s : *read.segments) {
        if (s.clock == capture::ClockClass::Running && s.hasSamples) { running.push_back(s); }
    }
    out.judgeable = !running.empty();

    if (!out.judgeable) {
        // R14's shape, and it is not rare: applying a parameter before `start` can land between
        // two publications of the roster burst, making segment 0 `frozen`. Measured 4 of 35
        // parameterised runs. Every verdict is indeterminate, and the campaign reports the run
        // as an infrastructure error rather than as one in which everything passed.
        out.notJudgeableReason =
            "no segment classified running, so nothing in this capture can be judged: "
            "sim_time_s does not order the samples of a frozen segment, and the sampling gap a "
            "not-met verdict is bounded against cannot be measured. This is not a determinism "
            "failure and it is not a failing scenario";
        for (const auto& c : conditions.conditions) {
            Verdict v;
            v.conditionId = c.id;
            v.kind = c.kind;
            v.capturePath = out.capturePath;
            v.state = State::Indeterminate;
            v.because = Because::NoRunningSegment;
            v.reason = out.notJudgeableReason;
            out.verdicts.push_back(v);
        }
    } else {
        Context ctx;
        ctx.tracks = &tracks;
        ctx.runningSegments = &running;
        ctx.capturePath = out.capturePath;

        for (const auto& c : conditions.conditions) {
            switch (c.kind) {
                case Kind::Proximity: out.verdicts.push_back(judgeProximity(c, ctx)); break;
                case Kind::Area: out.verdicts.push_back(judgeArea(c, ctx)); break;
                case Kind::TerminalState:
                    out.verdicts.push_back(c.form == TerminalForm::RemovalReason
                                               ? judgeTerminalRemoval(c, ctx)
                                               : judgeTerminalField(c, ctx));
                    break;
            }
        }
    }

    // The expectation is applied here, once, rather than inside each per-kind evaluator. The
    // evaluators answer what the run did; this decides whether that was what was asserted. Two
    // separate questions, and keeping them in two places is what stops an evaluator from
    // quietly deciding an expectation it was never told about.
    for (std::size_t i = 0; i < out.verdicts.size(); ++i) {
        Verdict& v = out.verdicts[i];

        // A rotation cuts one segment of the run into a close in one part and an open in the
        // next, and nothing in any file states what happened in between. That does not weaken
        // POSITIVE evidence - a record that exists in some part happened - but it does weaken
        // every negative, because a negative here is a conclusion drawn from absence and this
        // project's rule (CR-AS-4) is that absence is only evidence where a bound covers the
        // window nothing observed. The window at a rotation cut is not covered by any bound
        // this evaluator can measure, so a not-met verdict becomes indeterminate and says why.
        // It is never folded into pass or fail - that is the whole point of the third state.
        if (read.segmentsCutByRotation > 0 && v.state == State::NotMet) {
            v.state = State::Indeterminate;
            v.because = Because::RunCutByRotation;
            v.reason = "not met in what was recorded, but this run's capture is a rotated set "
                       "whose " + std::to_string(read.segmentsCutByRotation)
                       + " segment(s) were cut across a part boundary. A negative conclusion "
                         "needs continuity over the window the cut leaves unobserved, and no "
                         "file states it. The positive evidence in the set stands; this "
                         "absence does not. Original finding: " + v.reason;
        }

        v.expect = conditions.conditions[i].expect;
        if (v.state == State::Indeterminate) {
            v.outcome = Outcome::Undetermined;
        } else {
            const bool asExpected = (v.state == State::Met) == (v.expect == Expect::Met);
            v.outcome = asExpected ? Outcome::Satisfied : Outcome::Violated;
        }
        switch (v.state) {
            case State::Met: ++out.met; break;
            case State::NotMet: ++out.notMet; break;
            case State::Indeterminate: ++out.indeterminate; break;
        }
        switch (v.outcome) {
            case Outcome::Satisfied: ++out.satisfied; break;
            case Outcome::Violated: ++out.violated; break;
            case Outcome::Undetermined: ++out.undetermined; break;
        }
    }
    return true;
}

void collectNames(const ConditionFile& conditions,
                  std::set<std::string>& entities,
                  std::set<std::string>& fields) {
    for (const auto& c : conditions.conditions) {
        for (const auto& e : c.namedEntities()) { entities.insert(e); }
        if (c.kind == Kind::TerminalState && c.form == TerminalForm::FieldEquals) {
            fields.insert(c.field);
        }
    }
}

} // namespace

bool judgeCapture(const std::string& capturePath,
                  const ConditionFile& conditions,
                  JudgeResult& out) {
    out = JudgeResult{};
    out.capturePath = capturePath;

    std::set<std::string> entities;
    std::set<std::string> fields;
    collectNames(conditions, entities, fields);

    Collector collector(entities, fields);
    // The SET, not the file. A run recorded with `--on-size-limit rotate` is a numbered set of
    // parts; handed an unrotated capture this returns a one-part set, which is the correct
    // answer and is why no caller has to know in advance which it has.
    const capture::SetResult set = capture::readSet(capturePath, {}, &collector);
    if (set.parts.empty()) {
        out.rejected = true;
        out.rejectReason = "file_unreadable: nothing could be read from " + capturePath;
        return false;
    }
    for (const capture::ReadResult& part : set.parts) {
        if (part.rejected) {
            out.rejected = true;
            out.rejectReason = std::string(capture::name(part.rejectCode)) + ": "
                               + part.rejectDetail;
            return false;
        }
    }

    JudgedRead read;
    read.conformant = set.conformant();
    read.segments = &set.segments;
    read.parts = static_cast<long long>(set.parts.size());
    read.segmentsCutByRotation = set.segmentsCutByRotation;
    const bool rotated = set.parts.size() > 1;
    for (const capture::ReadResult& part : set.parts) {
        // The part is named only when there is more than one, so an ordinary capture's
        // diagnostics read exactly as they did before this could read a set at all.
        const std::string where = rotated ? (part.path + ": ") : std::string();
        for (const auto& diag : part.diagnostics) {
            read.diagnosticLines.push_back(where + capture::name(diag.code) + " at line "
                                           + std::to_string(diag.line) + ": " + diag.detail);
        }
    }
    for (const auto& diag : set.diagnostics) {
        read.diagnosticLines.push_back(std::string(capture::name(diag.code)) + ": " + diag.detail);
    }
    return judgeRead(read, collector.tracks, conditions, out);
}

bool judgeLines(const std::vector<std::string>& lines,
                const std::string& label,
                const ConditionFile& conditions,
                JudgeResult& out) {
    out = JudgeResult{};
    out.capturePath = label;

    std::set<std::string> entities;
    std::set<std::string> fields;
    collectNames(conditions, entities, fields);

    Collector collector(entities, fields);
    const capture::ReadResult raw = capture::readLines(lines, label, {}, &collector);
    if (raw.rejected) {
        out.rejected = true;
        out.rejectReason = std::string(capture::name(raw.rejectCode)) + ": " + raw.rejectDetail;
        return false;
    }
    // Lines already in memory are one capture by construction: a caller holding a rotated set
    // would have to concatenate it first, and there is then nothing left to cut.
    JudgedRead read;
    read.conformant = raw.conformant();
    read.segments = &raw.segments;
    for (const auto& diag : raw.diagnostics) {
        read.diagnosticLines.push_back(std::string(capture::name(diag.code)) + " at line "
                                       + std::to_string(diag.line) + ": " + diag.detail);
    }
    return judgeRead(read, collector.tracks, conditions, out);
}

} // namespace ext17::assertion
