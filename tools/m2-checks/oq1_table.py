# EXT-17 M2 — OQ-1's evidence table. Throwaway, like its sibling.
#
# Reads a campaign directory and tabulates, per run, the two quantities OQ-1 turns on:
#
#   the engine-state side  - the frame at which the stop predicate first held, off
#                            sim/engine/state, as the run record recorded it
#   the capture side       - sample counts and distinct sim_time_s values in the running
#                            segment, off the file
#
# OQ-1 criterion (c) is "identical across two runs of one configuration". The point of putting
# both columns beside each other is that one of them satisfies (c) and the other cannot, and
# the table shows which - rather than the decision resting on M1's single run plus an argument.

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from capture_structure import analyse  # noqa: E402


def load_campaign(campaign_dir):
    runs = []
    runs_dir = os.path.join(campaign_dir, "runs")
    for run_id in sorted(os.listdir(runs_dir)):
        run_dir = os.path.join(runs_dir, run_id)
        if not os.path.isdir(run_dir):
            continue
        record_path = os.path.join(run_dir, "run.json")
        if not os.path.exists(record_path):
            continue
        with open(record_path, "r", encoding="utf-8") as handle:
            record = json.load(handle)
        capture = None
        for name in sorted(os.listdir(run_dir)):
            if name.endswith(".n8rocap.jsonl"):
                capture = analyse(os.path.join(run_dir, name))
                break
        runs.append({"record": record, "capture": capture})
    return runs


def main(argv):
    if len(argv) < 2:
        print("usage: oq1_table.py <campaign-dir>", file=sys.stderr)
        return 2
    runs = load_campaign(argv[1])
    if not runs:
        print("no runs found", file=sys.stderr)
        return 2

    header = ("run  outcome     | engine state: frame  simTime      dt      | "
              "capture: seg0 samples  distinct t  t_max     seg1  orphans  midrun")
    print(header)
    print("-" * len(header))

    frames, sim_times, deltas = set(), set(), set()
    seg0_samples, seg0_times = set(), set()
    outcomes = {}
    orphan_values, midrun_values = set(), set()
    capture_bytes = []

    for entry in runs:
        rec = entry["record"]
        cap = entry["capture"]
        ev = rec["stop_evaluation"]
        outcomes[rec["outcome"]] = outcomes.get(rec["outcome"], 0) + 1
        frames.add(ev["observed_frame"])
        sim_times.add(ev["observed_sim_time_s"])
        deltas.add(ev["observed_delta_s"])

        if cap:
            segs = {s["segment"]: s for s in cap["segments"]}
            s0 = segs.get(0, {})
            s1 = segs.get(1, {})
            seg0_samples.add(s0.get("samples"))
            seg0_times.add(s0.get("distinct_sim_times"))
            orphans = (cap["trailer_drops"] or {}).get("samples_orphaned")
            orphan_values.add(orphans)
            midrun_values.add(cap["attached_mid_run"])
            capture_bytes.append(cap["bytes"])
            capture_cols = (f"{s0.get('samples'):>13}  {s0.get('distinct_sim_times'):>10}  "
                            f"{s0.get('sim_time_max'):>8}  {s1.get('samples', 0):>5}  "
                            f"{orphans!s:>7}  {cap['attached_mid_run']!s:>6}")
        else:
            capture_cols = " " * 20 + "(no capture)"

        print(f"{rec['run_id']:<4} {rec['outcome']:<11} | "
              f"{ev['observed_frame']:>19}  {ev['observed_sim_time_s']:<11} "
              f"{ev['observed_delta_s']:<7} | {capture_cols}")

    print()
    print("OQ-1's four criteria, against this campaign")
    print("-" * 78)
    predicate = runs[0]["record"]["stop_predicate"]
    print(f"predicate: {predicate['statement']}")
    print()
    print("(a) observable from what the run publishes")
    print("    YES - the frame number is published on sim/engine/state and every run recorded it.")
    print("(b) free of wall-clock quantities")
    print(f"    {'YES' if predicate['wall_clock_participates'] is False else 'NO'} - "
          "the predicate's only input is an engine-state snapshot; the record says so.")
    print("(c) identical across two runs of one configuration")
    print(f"    engine-state side: distinct observed frames  = {sorted(frames)}")
    print(f"                       distinct observed simTime = {sorted(sim_times)}")
    print(f"                       distinct observed dt      = {sorted(deltas)}")
    print(f"    capture side:      distinct seg-0 sample counts   = {sorted(x for x in seg0_samples if x is not None)}")
    print(f"                       distinct seg-0 sim_time counts = {sorted(x for x in seg0_times if x is not None)}")
    print(f"    -> {'SATISFIED' if len(frames) == 1 else 'NOT SATISFIED'} on the engine-state side.")
    print("(d) reached by every run without the run timeout firing")
    print(f"    outcomes: {outcomes}")
    timed_out = outcomes.get("timeout", 0)
    print(f"    -> {'SATISFIED' if timed_out == 0 and outcomes.get('completed', 0) == len(runs) else 'NOT SATISFIED'}")
    print()
    print("Supporting structural facts (CR-EX-1, CR-EX-2)")
    print("-" * 78)
    print(f"    attached_mid_run values across the campaign : {sorted(midrun_values, key=str)}")
    print(f"    samples_orphaned values                     : {sorted(orphan_values, key=str)}")
    if capture_bytes:
        total = sum(capture_bytes)
        print(f"    capture bytes: min {min(capture_bytes)}  max {max(capture_bytes)}  "
              f"total {total} ({total / 1e6:.1f} MB over {len(capture_bytes)} runs)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
