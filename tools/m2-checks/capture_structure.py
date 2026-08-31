# EXT-17 M2 — structural checks over a run's capture. Throwaway.
#
# **This is not the capture reader.** M3 builds that, in C++, from
# `contract/capture-format-v1.md` alone, and it is the artifact CR-CAP-2's conformance criterion
# is checked against. This script exists because M2 owes three narrow answers per run and would
# otherwise have to guess at them:
#
#   - `header.attached_mid_run` is false and `trailer.drops.samples_orphaned` is zero
#     (CR-EX-2's third acceptance criterion, and CR-EX-1's second)
#   - the segment structure is the one an ordinary run produces - two segments, the second
#     frozen (PROVENANCE finding 3)
#   - how many distinct `sim_time_s` values the running segment holds, which is the quantity
#     OQ-1 criterion (c) is NOT satisfiable against and the reason the predicate lives on the
#     engine-state side of that line
#
# It reads the format specification's field names and nothing else. It reads no EXT-08 source.
# Delete it once M3's reader can answer the same questions.

import collections
import json
import os
import sys


def analyse(path):
    """One capture file in, one dict of structural facts out. Never raises on a bad line."""
    header = None
    trailer = None
    malformed = 0
    # (segment, entity, occupancy) -> set of sim_time_s, and a per-sim_time_s tally for the
    # frozen-clock test the format specifies in section 14.
    per_segment_times = collections.defaultdict(set)
    per_segment_samples = collections.Counter()
    per_key_time_counts = collections.Counter()
    adds_by_occupancy = collections.Counter()
    removes = 0
    segment_open = {}
    segment_close = {}
    entities_seen = collections.defaultdict(set)

    with open(path, "r", encoding="utf-8") as handle:
        for raw in handle:
            raw = raw.strip()
            if not raw:
                continue
            try:
                record = json.loads(raw)
            except (ValueError, TypeError):
                malformed += 1
                continue
            kind = record.get("type")
            if kind == "header":
                header = record
            elif kind == "trailer":
                trailer = record
            elif kind == "segment_open":
                segment_open[record.get("segment")] = record
            elif kind == "segment_close":
                segment_close[record.get("segment")] = record
            elif kind == "entity_add":
                adds_by_occupancy[record.get("occupancy")] += 1
                entities_seen[record.get("segment")].add(
                    (record.get("entity"), record.get("occupancy")))
            elif kind == "entity_remove":
                removes += 1
            elif kind == "sample":
                seg = record.get("segment")
                t = record.get("sim_time_s")
                per_segment_times[seg].add(t)
                per_segment_samples[seg] += 1
                per_key_time_counts[(seg, record.get("entity"), record.get("occupancy"), t)] += 1

    # Section 14's frozen-clock test, per segment: in a running segment each entity publishes
    # once per sim_time_s, so the maximum is exactly 1. Anything higher is a frozen segment.
    max_per_key_time = collections.Counter()
    for (seg, _entity, _occ, _t), count in per_key_time_counts.items():
        if count > max_per_key_time[seg]:
            max_per_key_time[seg] = count

    # The union, not just the segments that carry samples. A segment the campaign tore down
    # promptly can be opened and closed with no sample in it at all - measured in 15 of the 20
    # runs at M2 - and a segment list built from sample records alone silently loses it, then
    # disagrees with the trailer's own count for a reason that has nothing to do with the file.
    segments = sorted(set(per_segment_samples) | set(segment_open) | set(segment_close))
    return {
        "file": os.path.basename(path),
        "bytes": os.path.getsize(path),
        "malformed_lines": malformed,
        "format_version": (header or {}).get("format_version"),
        "producer": ((header or {}).get("producer") or {}).get("version"),
        "attached_mid_run": (header or {}).get("attached_mid_run"),
        "sample_form": (header or {}).get("sample_form"),
        "part": (header or {}).get("part"),
        "limits": (header or {}).get("limits"),
        "has_trailer": trailer is not None,
        "end_reason": (trailer or {}).get("end_reason"),
        "trailer_counts": (trailer or {}).get("counts"),
        "trailer_drops": (trailer or {}).get("drops"),
        "segments": [
            {
                "segment": seg,
                "samples": per_segment_samples[seg],
                "distinct_sim_times": len(per_segment_times[seg]),
                "sim_time_min": min(per_segment_times[seg]) if per_segment_times[seg] else None,
                "sim_time_max": max(per_segment_times[seg]) if per_segment_times[seg] else None,
                "max_samples_per_entity_per_sim_time": max_per_key_time[seg],
                "frozen_clock": max_per_key_time[seg] > 1,
                "entity_keys": len(entities_seen.get(seg, ())),
            }
            for seg in segments
        ],
        "adds_by_occupancy": dict(adds_by_occupancy),
        "removes": removes,
        # Our own tally, to be compared against the trailer's - CR-CAP-2's criterion, checked
        # here only so M2 knows the file is internally consistent before it draws anything
        # from it.
        "our_sample_tally": sum(per_segment_samples.values()),
        "our_segment_tally": len(segments),
        "our_add_tally": sum(adds_by_occupancy.values()),
        "our_remove_tally": removes,
    }


def verdicts(facts):
    """The M2 questions, as pass/fail lines. Absence is never read as evidence: every check
    below is against a value the file actually carries, and a missing value is 'unknown'."""
    out = []

    def check(name, ok, detail):
        out.append({"check": name, "ok": bool(ok), "detail": detail})

    check("format_version is n8ro-capture/1",
          facts["format_version"] == "n8ro-capture/1",
          repr(facts["format_version"]))
    check("capture is complete (ends in a trailer)",
          facts["has_trailer"], f"end_reason={facts['end_reason']!r}")
    check("no malformed lines", facts["malformed_lines"] == 0,
          f"{facts['malformed_lines']} malformed")

    # CR-EX-2 third criterion / CR-EX-1 second criterion.
    check("attached_mid_run is false", facts["attached_mid_run"] is False,
          repr(facts["attached_mid_run"]))
    drops = facts["trailer_drops"] or {}
    check("samples_orphaned is zero", drops.get("samples_orphaned") == 0,
          repr(drops.get("samples_orphaned")))

    counts = facts["trailer_counts"] or {}
    check("our tally agrees with trailer.counts",
          counts.get("segments") == facts["our_segment_tally"]
          and counts.get("samples") == facts["our_sample_tally"]
          and counts.get("entities_added") == facts["our_add_tally"]
          and counts.get("entities_removed") == facts["our_remove_tally"],
          f"trailer={counts} ours=(segments={facts['our_segment_tally']}, "
          f"samples={facts['our_sample_tally']}, adds={facts['our_add_tally']}, "
          f"removes={facts['our_remove_tally']})")

    segs = facts["segments"]
    check("two segments, the ordinary shape of one run", len(segs) == 2,
          f"{len(segs)} segment(s)")
    if segs:
        check("segment 0 is a running segment (frozen-clock test)",
              segs[0]["max_samples_per_entity_per_sim_time"] == 1,
              f"max samples per (entity, occupancy) at one sim_time_s = "
              f"{segs[0]['max_samples_per_entity_per_sim_time']}")
    if len(segs) > 1:
        # The format's frozen-clock test is exact, but only where there are enough samples for
        # it to fire: it says "more than one sample for a key at one sim_time_s". A teardown the
        # campaign ends promptly can produce a segment 1 too short to trip it, which is not the
        # same as segment 1 being a running segment. Both are reported, distinctly - collapsing
        # them into one pass/fail would be the check lying about what it knows.
        s1 = segs[1]
        detail = (f"samples={s1['samples']}, "
                  f"max per key per sim_time={s1['max_samples_per_entity_per_sim_time']}, "
                  f"sim_time span {s1['sim_time_min']}..{s1['sim_time_max']}")
        if s1["frozen_clock"]:
            check("segment 1 identified as frozen by the format's own test", True, detail)
        else:
            check("segment 1 is not a running segment",
                  s1["samples"] == 0 or s1["sim_time_max"] == 0,
                  detail + " - too short for the frozen-clock test to fire; identified by its "
                           "zero sim_time span instead")
    return out


def main(argv):
    if len(argv) < 2:
        print("usage: capture_structure.py <capture.jsonl> [more...]", file=sys.stderr)
        print("       capture_structure.py --campaign <campaign-dir>", file=sys.stderr)
        return 2

    paths = []
    if argv[1] == "--campaign":
        runs_dir = os.path.join(argv[2], "runs")
        for run_id in sorted(os.listdir(runs_dir)):
            run_dir = os.path.join(runs_dir, run_id)
            if not os.path.isdir(run_dir):
                continue
            for name in sorted(os.listdir(run_dir)):
                if name.endswith(".n8rocap.jsonl"):
                    paths.append(os.path.join(run_dir, name))
    else:
        paths = argv[1:]

    results = []
    all_ok = True
    for path in paths:
        facts = analyse(path)
        checks = verdicts(facts)
        failed = [c for c in checks if not c["ok"]]
        all_ok = all_ok and not failed
        results.append({"facts": facts, "checks": checks})
        status = "OK  " if not failed else "FAIL"
        segs = facts["segments"]
        summary = " ".join(
            f"seg{s['segment']}:{s['samples']}s/{s['distinct_sim_times']}t/max{s['max_samples_per_entity_per_sim_time']}"
            for s in segs)
        print(f"{status} {facts['file']:<48} {facts['bytes']:>10} B  {summary}")
        for c in failed:
            print(f"       ! {c['check']}: {c['detail']}")

    print(json.dumps(results, indent=1), file=sys.stderr)
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
