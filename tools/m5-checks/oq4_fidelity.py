# EXT-17 M5 — read the OQ-4 fidelity spike's captures. Throwaway, like its M2 siblings.
#
# M2's spike asked whether an injected value is HONOURED. This one asks whether a swept RANGE of
# that value produces runs that still make sense — OQ-4's first deciding criterion, and the one
# the PRD records as unmeasured. So every column below is read off the capture:
#
#   honoured    does the raid actually fly at the value that was injected, at its first
#               published sample and still at the last one
#   roster      is the authored roster the same at every value — the sweep's results are only
#               comparable across values if the entities they are computed over are the same set
#   penetration how far the nearest raider got, in metres, toward the defended cluster
#   engagement  how many raiders left `health: nominal`, and how many SAM rounds were spawned:
#               a candidate for CR-PAR-2's "at least one condition actually changes outcome"
#   sanity      anything non-finite, and whether altitude held
#
# Nothing here is a product. `n8ro-capture` is the conformant reader; this is a question asked
# of already-conformant files.

import collections
import json
import math
import os
import sys

# The defended cluster's centre, taken as the SAM battery's authored position — it is the thing
# the raid is flying at, and the only fixed point the penetration number needs.
DEFENDED = ("BlueSAM_ShortRange", -23.49649, -68.24741)

# Degrees to metres at this latitude. A local flat approximation is enough: every number it
# produces is a comparison between runs of the same scenario, never an absolute range claim.
M_PER_DEG_LAT = 110574.0
M_PER_DEG_LON = 111320.0 * math.cos(math.radians(-23.5))


def ground_range_m(lat, lon):
    dlat = (lat - DEFENDED[1]) * M_PER_DEG_LAT
    dlon = (lon - DEFENDED[2]) * M_PER_DEG_LON
    return math.hypot(dlat, dlon)


def read(path):
    first = {}          # the genuinely first published sample, which is the roster burst at load
    first_op = {}       # the first sample at phase "operational" - the first frame of the run
    last = {}
    counts = collections.Counter()
    adds = set()
    removes = collections.Counter()
    health_left_nominal = set()
    spawned = set()
    nonfinite = 0
    closest = float("inf")
    closest_by = None
    first_hurt_t = None
    with open(path, "r", encoding="utf-8") as handle:
        for raw in handle:
            try:
                r = json.loads(raw)
            except (ValueError, TypeError):
                continue
            t = r.get("type")
            if t == "entity_add":
                key = (r["entity"], r["occupancy"])
                adds.add(key)
                if "_wpn_" in r["entity"]:
                    spawned.add(key)
            elif t == "entity_remove":
                removes[r.get("reason") or "(none)"] += 1
            elif t == "sample":
                key = (r["entity"], r["occupancy"])
                seg = (r.get("part", 0), r.get("segment"))
                # Only segment 0 — the run proper. Segment 1 is the teardown reload and M4
                # measured it carrying samples in 5 runs of 20.
                if seg[1] != 0:
                    continue
                counts[key] += 1
                f = r["fields"]
                if r["entity"].startswith("RedUAV_"):
                    p = f.get("positionGeodetic")
                    if p:
                        rng = ground_range_m(p[0], p[1])
                        if rng < closest:
                            closest, closest_by = rng, r["entity"]
                if key not in first:
                    first[key] = f
                if key not in first_op and f.get("phase") == "operational":
                    first_op[key] = f
                last[key] = f
                if f.get("health") != "nominal":
                    if key not in health_left_nominal and first_hurt_t is None:
                        first_hurt_t = r.get("sim_time_s")
                    health_left_nominal.add(key)
                for v in list(f.get("positionGeodetic", [])) + list(f.get("velocityNed", [])):
                    if not isinstance(v, (int, float)) or not math.isfinite(v):
                        nonfinite += 1
    return {
        "first": first, "first_op": first_op, "last": last, "counts": counts, "adds": adds,
        "hurt": health_left_nominal, "spawned": spawned, "nonfinite": nonfinite,
        "closest": closest, "closest_by": closest_by, "first_hurt_t": first_hurt_t,
        "removes": removes,
    }


def raiders(d):
    return sorted(k for k in d["counts"] if k[0].startswith("RedUAV_"))


def summarise(label, path):
    d = read(path)
    rs = raiders(d)
    # The authored roster is the scenario's own 42 entities. Weapons are created DURING the
    # run - they are a result of the parameter, never part of it - and are counted separately.
    authored = sorted(k for k in d["counts"] if "_wpn_" not in k[0])
    gun_rounds = sorted({k for k in d["spawned"] if k[0].startswith("BlueGun")})

    # Honoured: the north group's lead flies at the injected speed. Read three times - at the
    # roster burst published at LOAD (which may or may not predate the update; it is not the
    # run), at the first sample of the run proper, and at the last. A value that was applied and
    # then overwritten shows up as a difference between the second and the third.
    lead = ("RedUAV_N_01", 1)
    def speed_of(store):
        v = store.get(lead, {}).get("velocityNed")
        return math.hypot(v[0], v[1]) if v else float("nan")
    speed_burst = speed_of(d["first"])
    speed0 = speed_of(d["first_op"])
    speed1 = speed_of(d["last"])

    # Penetration: the closest any raider got to the defended cluster, minimised over EVERY
    # sample rather than over the first and last - a raider that is destroyed on the way in has
    # its last sample where it died, which is not where it got closest.
    closest = d["closest"]
    closest_by = d["closest_by"]

    alt0 = d["first"].get(lead, {}).get("positionGeodetic", [0, 0, float("nan")])[2]
    alt1 = d["last"].get(lead, {}).get("positionGeodetic", [0, 0, float("nan")])[2]

    return {
        "label": label,
        "authored_keys": len(authored),
        "raiders": len(rs),
        "speed_burst": speed_burst,
        "speed_first": speed0,
        "speed_last": speed1,
        "closest_m": closest,
        "closest_by": closest_by,
        "hurt": len([k for k in d["hurt"] if k[0].startswith("RedUAV_")]),
        "sam_rounds": len(d["spawned"]),
        "gun_rounds": len(gun_rounds),
        "alt_first": alt0,
        "alt_last": alt1,
        "nonfinite": d["nonfinite"],
        "first_hurt_t": d["first_hurt_t"],
        "adds": len(d["adds"]),
        "removes": sum(d["removes"].values()),
        "keys_with_samples": len(d["counts"]),
        "samples": sum(d["counts"].values()),
    }


def main(root):
    rows = []
    for name in sorted(os.listdir(root)):
        run = os.path.join(root, name)
        if not os.path.isdir(run) or not name.startswith("v"):
            continue
        caps = [f for f in os.listdir(run) if f.endswith(".n8rocap.jsonl")]
        if not caps:
            print("%-8s no capture" % name)
            continue
        rows.append(summarise(name, os.path.join(run, caps[0])))

    rows.sort(key=lambda r: float(r["label"][1:].replace("p", ".")))

    print("%-7s %7s %7s %7s %6s %5s %7s %5s %5s %9s %-13s %5s %5s %5s %9s %7s %4s" % (
        "value", "burst", "spd@1st", "spd@last", "roster", "raid", "samples", "adds", "remvs",
        "closest_m", "closest_by", "hurt", "wpns", "guns", "1st_hurt_t", "alt@last", "nan"))
    for r in rows:
        print("%-7s %7.1f %7.1f %7.1f %6d %5d %7d %5d %5d %9.0f %-13s %5d %5d %5d %9s %7.0f %4d" % (
            r["label"], r["speed_burst"], r["speed_first"], r["speed_last"],
            r["authored_keys"], r["raiders"],
            r["samples"], r["adds"], r["removes"],
            r["closest_m"], r["closest_by"] or "-", r["hurt"], r["sam_rounds"], r["gun_rounds"],
            ("%.2f" % r["first_hurt_t"]) if r["first_hurt_t"] is not None else "-",
            r["alt_last"], r["nonfinite"]))

    keysets = {r["authored_keys"] for r in rows}
    print()
    print("authored roster size across every value: %s  (%s)" % (
        sorted(keysets), "IDENTICAL" if len(keysets) == 1 else "VARIES"))
    for field, what in (("hurt", "raiders that left health nominal"),
                        ("adds", "entity_add records"),
                        ("removes", "entity_remove records"),
                        ("sam_rounds", "weapon rounds spawned"),
                        ("gun_rounds", "CIWS gun rounds spawned"),
                        ("keys_with_samples", "(entity, occupancy) keys carrying samples")):
        vals = sorted({r[field] for r in rows})
        print("%-46s across the sweep: %-28s (%s)" % (
            what, vals, "VARIES" if len(vals) > 1 else "FLAT"))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "campaigns/m5-oq4")
