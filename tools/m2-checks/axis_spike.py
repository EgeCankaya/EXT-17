# EXT-17 M2 — read the R9/OQ-4 axis spike's captures. Throwaway, like its siblings.
#
# One question per probe, answered off the capture rather than off the fact that a publish
# returned true. `sendEntityUpdate` returning true means the message went onto the bus; it says
# nothing about whether the engine kept the value, which is the whole open question.

import collections
import json
import os
import sys

TARGET = "RedUAV_N_01"
CREATED = "SpikeUAV_01"
INJECTED_POS = [-23.30000, -68.10000, 900.0]
INJECTED_VEL = [-80.0, 25.0, -5.0]


def read(path):
    """Roster events and the first/early samples of the entities the probes touch."""
    first_sample = {}
    sample_count = collections.Counter()
    adds = []
    removes = []
    roster = set()
    early = collections.defaultdict(list)
    with open(path, "r", encoding="utf-8") as handle:
        for raw in handle:
            try:
                r = json.loads(raw)
            except (ValueError, TypeError):
                continue
            t = r.get("type")
            if t == "entity_add":
                adds.append((r.get("entity"), r.get("occupancy"), r.get("segment")))
                if r.get("segment") == 0:
                    roster.add(r.get("entity"))
            elif t == "entity_remove":
                removes.append((r.get("entity"), r.get("occupancy"), r.get("segment"),
                                r.get("reason")))
            elif t == "sample" and r.get("segment") == 0:
                name = r.get("entity")
                sample_count[name] += 1
                if name not in first_sample:
                    first_sample[name] = r
                if name in (TARGET, CREATED) and len(early[name]) < 6:
                    early[name].append(r)
    return {
        "adds": adds, "removes": removes, "roster": roster,
        "first_sample": first_sample, "sample_count": sample_count, "early": early,
    }


def pos_vel(record):
    if not record:
        return None, None
    f = record.get("fields", {})
    return f.get("positionGeodetic"), f.get("velocityNed")


def close(a, b, tol=1e-3):
    if a is None or b is None:
        return False
    return all(abs(x - y) <= tol for x, y in zip(a, b))


def probe_dir(root, name):
    d = os.path.join(root, name)
    for f in sorted(os.listdir(d)):
        if f.endswith(".n8rocap.jsonl"):
            return os.path.join(d, f)
    return None


def main(argv):
    root = argv[1] if len(argv) > 1 else "campaigns/m2-axis"
    data = {}
    for name in ("p0-baseline", "p1-update-pre", "p2-update-post", "p3-delete-pre",
                 "p4-create-pre", "p5-scenario"):
        path = probe_dir(root, name)
        if path:
            data[name] = read(path)
            print(f"read {name}: {os.path.basename(path)}")
    print()

    base = data.get("p0-baseline")
    if base:
        p, v = pos_vel(base["first_sample"].get(TARGET))
        print(f"BASELINE  {TARGET} first sample  pos={p}  vel={v}")
        print(f"          segment-0 roster size = {len(base['roster'])}, "
              f"adds={len(base['adds'])}, removes={len(base['removes'])}")
    print(f"INJECTED  pos={INJECTED_POS}  vel={INJECTED_VEL}")
    print()

    print("=" * 78)
    print("AXIS A - initial positions and velocities")
    print("=" * 78)
    for probe, when in (("p1-update-pre", "before start"), ("p2-update-post", "after start")):
        d = data.get(probe)
        if not d:
            continue
        p, v = pos_vel(d["first_sample"].get(TARGET))
        print(f"\n{probe} (sendEntityUpdate {when})")
        print(f"  first sample after the update: pos={p}  vel={v}")
        took = close(p, INJECTED_POS) or close(v, INJECTED_VEL)
        print(f"  matches the injected value?  {'YES' if took else 'NO'}")
        # Where the entity was over its first few published samples, so a value that was
        # applied and then immediately overwritten is distinguishable from one never applied.
        for r in d["early"].get(TARGET, [])[:4]:
            pp, vv = pos_vel(r)
            print(f"    t={r.get('sim_time_s')}: pos={pp} vel={vv}")
        if probe == "p2-update-post":
            # The update lands mid-run; look at what the entity did later in the run.
            samples = [s for s in d["early"].get(TARGET, [])]
            print(f"    (samples captured for {TARGET}: {d['sample_count'][TARGET]})")

    print()
    print("=" * 78)
    print("AXIS B - which entities are present")
    print("=" * 78)
    for probe, what in (("p3-delete-pre", f"sendEntityDelete({TARGET})"),
                        ("p4-create-pre", f"sendEntityCreate(..., {CREATED})")):
        d = data.get(probe)
        if not d:
            continue
        print(f"\n{probe} ({what} before start)")
        print(f"  segment-0 roster size = {len(d['roster'])}"
              f"  (baseline {len(base['roster']) if base else '?'})")
        if probe == "p3-delete-pre":
            print(f"  {TARGET} in segment-0 roster? {TARGET in d['roster']}")
            print(f"  {TARGET} samples in segment 0: {d['sample_count'][TARGET]}"
                  f"  (baseline {base['sample_count'][TARGET] if base else '?'})")
            rm = [r for r in d["removes"] if r[0] == TARGET]
            print(f"  entity_remove records for {TARGET}: {rm}")
        else:
            print(f"  {CREATED} in segment-0 roster? {CREATED in d['roster']}")
            print(f"  {CREATED} samples in segment 0: {d['sample_count'][CREATED]}")
            added = [a for a in d["adds"] if a[0] == CREATED]
            print(f"  entity_add records for {CREATED}: {added}")

    print()
    print("=" * 78)
    print("AXIS C - which scenario from the catalogue")
    print("=" * 78)
    d = data.get("p5-scenario")
    if d:
        print(f"\np5-scenario (Baltic Sentinel loaded by name from the catalogue)")
        print(f"  segment-0 roster size = {len(d['roster'])}"
              f"  (Atacama baseline {len(base['roster']) if base else '?'})")
        print(f"  adds={len(d['adds'])}, removes={len(d['removes'])}")
        print(f"  a few entity names: {sorted(d['roster'])[:6]}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
