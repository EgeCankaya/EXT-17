// EXT-17 — rotated-set stitching. See CaptureSet.h.
#include "CaptureSet.h"

#include <set>
#include <string>

namespace ext17::capture {
namespace {

std::string num(long long v) { return std::to_string(v); }

std::size_t lastSeparator(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash;
}

std::string directoryOf(const std::string& path) {
    const std::size_t s = lastSeparator(path);
    return s == std::string::npos ? std::string() : path.substr(0, s + 1);
}

std::string baseNameOf(const std::string& path) {
    const std::size_t s = lastSeparator(path);
    return s == std::string::npos ? path : path.substr(s + 1);
}

} // namespace

SetResult readSet(const std::string& firstPartPath, const SetOptions& options,
                  RecordSink* sink) {
    SetResult set;
    const std::string dir = directoryOf(firstPartPath);

    auto note = [&set](Code code, const std::string& detail) {
        ++set.diagnosticCounts[code];
        set.diagnostics.push_back(Diagnostic{code, 0, detail});
    };

    std::set<std::string> visited;
    std::string path = firstPartPath;
    long long expectedPart = 0;
    std::string previousBaseName;

    for (;;) {
        const std::string base = baseNameOf(path);
        if (!visited.insert(base).second) {
            note(Code::PartLinkBroken,
                 "part link revisits \"" + base + "\"; the set does not terminate (§6.7)");
            break;
        }
        if (visited.size() > options.maxParts) {
            note(Code::PartLinkBroken,
                 "more than " + num(static_cast<long long>(options.maxParts)) + " parts");
            break;
        }

        ReadResult part = readFile(path, options.read, sink);
        const bool rejected = part.rejected;
        const Header header = part.header;
        const Trailer trailer = part.trailer;
        const bool hasTrailer = part.hasTrailer;

        // The set's segment list is the concatenation of the parts', already keyed on
        // (part, segment) by the reader. Nothing is merged: a segment cut by a rotation is one
        // segment in the run and two keys here, and the two keys are what make the per-segment
        // statistic right rather than what makes it wrong.
        for (const SegmentStats& s : part.segments) set.segments.push_back(s);

        set.counts.segments += trailer.counts.segments;
        set.counts.samples += trailer.counts.samples;
        set.counts.entitiesAdded += trailer.counts.entitiesAdded;
        set.counts.entitiesRemoved += trailer.counts.entitiesRemoved;
        set.counts.verdicts += trailer.counts.verdicts;

        // A segment cut by a rotation is one segment of the run appearing as a close in this
        // part and an open in the next. §6.7 names the shape exactly, so recognising it needs no
        // heuristic: this part continues, and its last segment closed at the size bound.
        if (hasTrailer && trailer.hasContinuedIn && !part.segments.empty()
            && part.segments.back().closeReason == "size_limit") {
            ++set.segmentsCutByRotation;
        }

        set.parts.push_back(std::move(part));

        if (rejected) break;

        if (header.part != expectedPart) {
            note(Code::PartLinkBroken,
                 "\"" + base + "\" declares header.part " + num(header.part) + "; the set is at "
                     "part " + num(expectedPart) + " (§6.7)");
        }
        if (expectedPart == 0 && header.hasContinuesFrom) {
            note(Code::PartLinkBroken,
                 "\"" + base + "\" is the first part and carries continues_from \""
                     + header.continuesFrom + "\"; that key is present only on parts after the "
                       "first (§6.7)");
        }
        if (expectedPart > 0) {
            if (!header.hasContinuesFrom) {
                note(Code::PartLinkBroken,
                     "\"" + base + "\" is part " + num(expectedPart) + " and carries no "
                       "continues_from (§6.7)");
            } else if (header.continuesFrom != previousBaseName) {
                note(Code::PartLinkBroken,
                     "\"" + base + "\" continues_from \"" + header.continuesFrom
                         + "\"; the previous part is \"" + previousBaseName + "\" (§6.7)");
            }
        }

        if (!hasTrailer || !trailer.hasContinuedIn) {
            // The last part of a completed run carries the run's real ending and no
            // `continued_in`. A truncated part carries neither, and its own missing trailer is
            // what already reported that — so the set simply ends here either way.
            break;
        }

        // §6.7: a part in the middle carries end_reason "size_limit" AND a continued_in. A part
        // that says the run continues but does not say it stopped at its bound is a producer
        // defect, and it is the one that would silently lose the rest of a run.
        if (trailer.endReason != "size_limit") {
            note(Code::PartLinkBroken,
                 "\"" + base + "\" continues into \"" + trailer.continuedIn
                     + "\" but its end_reason is \"" + trailer.endReason
                     + "\"; an intermediate part ends with \"size_limit\" (§6.7)");
        }

        // Filenames rather than paths, by construction: the parts of a set are written to one
        // directory, and an absolute path would leak the producing host's layout into an
        // artifact that crosses a repository boundary (§6.7).
        if (trailer.continuedIn.find('/') != std::string::npos
            || trailer.continuedIn.find('\\') != std::string::npos) {
            note(Code::PartLinkBroken,
                 "continued_in \"" + trailer.continuedIn + "\" is a path, not a bare filename (§6.7)");
        }

        previousBaseName = base;
        path = dir + trailer.continuedIn;
        ++expectedPart;
    }

    set.runSegments = set.counts.segments - set.segmentsCutByRotation;
    return set;
}

} // namespace ext17::capture
