// EXT-17 — the names. See Capture.h.
//
// These strings are interface, not diagnostics prose: the conformance suite asserts on them and
// a report prints them, so renaming one is a change to what this project publishes.
#include "Capture.h"

namespace ext17::capture {

const char* name(Code code) {
    switch (code) {
        case Code::Ok:                          return "ok";
        case Code::FileUnreadable:              return "file_unreadable";
        case Code::NotACapture:                 return "not_a_capture";
        case Code::UnsupportedFormatVersion:    return "unsupported_format_version";
        case Code::MalformedLine:               return "malformed_line";
        case Code::BlankLine:                   return "blank_line";
        case Code::TruncatedNoTrailer:          return "truncated_no_trailer";
        case Code::RecordsAfterTrailer:         return "records_after_trailer";
        case Code::UnknownRecordType:           return "unknown_record_type";
        case Code::MissingRequiredKey:          return "missing_required_key";
        case Code::FormatVersionNotFirst:       return "format_version_not_first";
        case Code::ClosedVocabularyViolation:   return "closed_vocabulary_violation";
        case Code::UnknownMessage:              return "unknown_message";
        case Code::UndeclaredField:             return "undeclared_field";
        case Code::FieldOrderMismatch:          return "field_order_mismatch";
        case Code::FieldTypeMismatch:           return "field_type_mismatch";
        case Code::ArrayLengthMismatch:         return "array_length_mismatch";
        case Code::SampleOutsideSegment:        return "sample_outside_segment";
        case Code::SampleTimeDecreased:         return "sample_time_decreased";
        case Code::SegmentOrdinalNotIncreasing: return "segment_ordinal_not_increasing";
        case Code::UnbalancedSegment:           return "unbalanced_segment";
        case Code::SampleAfterRemove:           return "sample_after_remove";
        case Code::UnmatchedEntityRemove:       return "unmatched_entity_remove";
        case Code::CountsDisagree:              return "counts_disagree";
        case Code::PartLinkBroken:              return "part_link_broken";
    }
    return "unknown_code";
}

Severity severity(Code code) {
    switch (code) {
        case Code::FileUnreadable:
        case Code::NotACapture:
        case Code::UnsupportedFormatVersion:
            return Severity::Reject;
        default:
            return Severity::Defect;
    }
}

const char* name(ClockClass c) {
    switch (c) {
        case ClockClass::Running:       return "running";
        case ClockClass::Frozen:        return "frozen";
        case ClockClass::Indeterminate: return "indeterminate";
    }
    return "unknown";
}

long long ReadResult::diagnosticTotal() const {
    long long n = 0;
    for (const auto& kv : diagnosticCounts) n += kv.second;
    return n;
}

bool SetResult::conformant() const {
    if (!diagnosticCounts.empty()) return false;
    for (const ReadResult& p : parts) {
        if (!p.conformant()) return false;
    }
    return !parts.empty();
}

long long SetResult::diagnosticTotal() const {
    long long n = 0;
    for (const auto& kv : diagnosticCounts) n += kv.second;
    for (const ReadResult& p : parts) n += p.diagnosticTotal();
    return n;
}

} // namespace ext17::capture
