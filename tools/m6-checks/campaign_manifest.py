"""Write a verifiable manifest for a committed campaign's captures.

EXT-17 M6. Evidence, not product — the same status as `tools/m2-checks/` and `tools/m5-checks/`.

## Why this exists

[B]'s third deliverable is *"a real campaign — its configuration, its captured runs and its
report, committed as an example"*, and the captured runs are the problem: one 1200-frame run of
Atacama Air Defense writes about 24 MB of JSON Lines, so twenty of them are roughly **480 MB**.
`.gitignore` has excluded them since the repository was created, with a reason stated there —
*"artifacts to produce and check, not to version"*.

So the campaign is committed **without its raw captures and with a manifest instead**: for each
run, the capture's byte size, its SHA-256, the counts this project's own reader made of it, and
whether that reader found it conformant. A reviewer who re-runs the campaign from its committed
configuration can compare their captures against these lines.

**What that does and does not buy, stated rather than implied.** It does not make a re-run
byte-identical — nothing does, and that is this project's central measurement: two runs of one
configuration are never byte-identical here, so a SHA-256 will not match on a re-run and is not
meant to. What the manifest establishes is that these specific files existed, what was in them,
and what was concluded from them — so a claim in `docs/m6-assertions.md` can be traced to a file
whose contents are pinned even though the file is not in the tree. The counts are the part a
re-run can be compared against, because those are what the reports are computed from.

Everything a *judgement* rests on IS committed: `run.json`, `verdicts.jsonl`, `campaign.json`,
`campaign.log` and `self-test.json`. Those are the report, and they are small.

## Usage

    python tools/m6-checks/campaign_manifest.py campaigns/m6-campaign > campaigns/m6-campaign/MANIFEST.md
"""

import hashlib
import json
import os
import sys

# Redirected stdout on Windows defaults to the ANSI codepage, which turns an em dash into a
# replacement character in the committed manifest. Force UTF-8 so the file is the same on any
# machine that regenerates it.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def captures_in(run_dir):
    out = []
    for name in sorted(os.listdir(run_dir)):
        if name.endswith(".n8rocap.jsonl"):
            out.append(os.path.join(run_dir, name))
    return out


def read_json(path):
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def rows_for(runs_dir):
    rows = []
    for run_id in sorted(os.listdir(runs_dir)):
        run_dir = os.path.join(runs_dir, run_id)
        if not os.path.isdir(run_dir):
            continue
        record = read_json(os.path.join(run_dir, "run.json")) or {}
        for capture in captures_in(run_dir):
            rows.append(
                {
                    "run": run_id,
                    "value": record.get("parameter", {}).get("value", "-"),
                    "outcome": record.get("outcome", "?"),
                    "capture": os.path.basename(capture),
                    "bytes": os.path.getsize(capture),
                    "sha256": sha256_of(capture),
                    "samples": record.get("capture", {}).get("samples"),
                    "segments": record.get("capture", {}).get("segment_keys"),
                    "conformant": record.get("capture", {}).get("conformant"),
                    "covers": record.get("capture", {}).get("covers_whole_run"),
                }
            )
        if not captures_in(run_dir):
            rows.append(
                {
                    "run": run_id,
                    "value": record.get("parameter", {}).get("value", "-"),
                    "outcome": record.get("outcome", "?"),
                    "capture": "(none)",
                    "bytes": 0,
                    "sha256": "-",
                    "samples": None,
                    "segments": None,
                    "conformant": None,
                    "covers": None,
                }
            )
    return rows


def oq2_wording_note(campaign_dir):
    """Emit a dated note when a campaign's stored artifacts predate the OQ-2 decision.

    A campaign's `campaign.json` and `self-test.json` are written by the run that produced
    them and are never rewritten afterwards: they are evidence, and re-running to refresh a
    sentence would replace measured numbers with different measured numbers and invalidate
    every SHA-256 in the manifest. So a campaign executed before 2026-09-01's OQ-2 decision
    carries `"oq2_ruling": "unanswered"`, which the tools no longer print.

    This is read from the artifact rather than hard-coded, so it retires itself: a campaign
    run by the current binary records `decided` and gets no note.
    """
    path = os.path.join(campaign_dir, "campaign.json")
    if not os.path.isfile(path):
        return
    try:
        with open(path, "rb") as f:
            summary = json.loads(f.read().decode("utf-8-sig"))
    except (ValueError, OSError):
        return
    if summary.get("self_test", {}).get("gate", {}).get("oq2_ruling") != "unanswered":
        return

    quoted = '"oq2_ruling": "unanswered"'
    print()
    print("## One superseded sentence, and why it was not edited")
    print()
    print("`campaign.json` and `selftest/self-test.json` here record OQ-2 as **`%s`**." % quoted)
    print("That was true when this campaign ran and is not true now. **Later the same day —")
    print("2026-09-01 — OQ-2 was DECIDED by the DRI (content) and CONCURRED with by the mentor,")
    print("independently.** It has still never been ANSWERED by [B]'s author, and those three")
    print("words stay apart: see `docs/m7-oq2-oq3.md` and `docs/escalations.md` E-2.")
    print()
    print("**The artifacts were not rewritten, and that is the point.** They are evidence of an")
    print("execution, not documentation of a position. Re-running this campaign to refresh one")
    print("sentence would replace these measured numbers with different measured numbers, break")
    print("every SHA-256 below, and invalidate the figures `docs/m6-assertions.md` and")
    print("`docs/m7-oq2-oq3.md` quote from them - to correct wording that changed no behaviour.")
    print("**No code changed when OQ-2 was decided**: `content` was already the default, and")
    print("this campaign's gate ran on it.")
    print()
    print("The binaries print the current three-part wording, and `src/run/SelfTest.cpp` writes")
    print("`oq2_decided_by`, `oq2_concurred_by` and `oq2_answered_by_brief_author` as three")
    print("separate keys. A campaign run today records `decided` and this section does not")
    print("appear - it is keyed on the artifact, not on the calendar.")


def emit(campaign_dir):
    runs_dir = os.path.join(campaign_dir, "runs")
    rows = rows_for(runs_dir) if os.path.isdir(runs_dir) else []
    selftest_dir = os.path.join(campaign_dir, "selftest", "runs")
    self_rows = rows_for(selftest_dir) if os.path.isdir(selftest_dir) else []

    name = os.path.basename(os.path.normpath(campaign_dir))
    as_written = os.path.normpath(campaign_dir).replace("/", "\\")
    print("# `%s` - capture manifest" % name)
    print()
    print("**The captures themselves are not committed.** One 1200-frame run of Atacama Air")
    print("Defense writes about 24 MB of JSON Lines, so this campaign's captures are roughly")
    print("%.0f MB. `.gitignore` has excluded them since the repository was created, with the"
          % (sum(r["bytes"] for r in rows + self_rows) / 1e6))
    print("reason stated there: artifacts to produce and check, not to version.")
    print()
    print("This manifest is what stands in for them. Everything a **judgement** rests on IS")
    print("committed — `run.json`, `verdicts.jsonl`, `campaign.json`, `campaign.log` and")
    print("`self-test.json` — because those are the report, and they are small.")
    print()
    print("**What this establishes, and what it does not.** It does not make a re-run")
    print("byte-identical: nothing does, and that is this project's central measurement — two")
    print("runs of one configuration are never byte-identical here, so a SHA-256 below will not")
    print("match a re-run and is not meant to. What it establishes is that these specific files")
    print("existed, what was in them, and what was concluded from them, so that a number in")
    print("`docs/m6-assertions.md` can be traced to a file whose contents are pinned even though")
    print("the file is not in the tree. **The counts are the part a re-run can be compared**")
    print("**against**, because the reports are computed from those rather than from the bytes.")
    print()
    print("Regenerate with:")
    print()
    print("```cmd")
    # The path AS GIVEN, not the basename. A nested campaign - m6-faults/<fault>/ - would
    # otherwise be told to regenerate itself from a directory that does not exist.
    print(r"python tools\m6-checks\campaign_manifest.py %s > %s\MANIFEST.md"
          % (as_written, as_written))
    print("```")

    oq2_wording_note(campaign_dir)

    def table(title, data):
        if not data:
            return
        print()
        print("## %s" % title)
        print()
        print("| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |")
        print("|---|---|---|---|---:|---:|---:|---|---|---|")
        for r in data:
            print("| %s | %s | %s | `%s` | %d | %s | %s | %s | %s | `%s` |" % (
                r["run"], r["value"], r["outcome"], r["capture"], r["bytes"],
                "-" if r["samples"] is None else r["samples"],
                "-" if r["segments"] is None else r["segments"],
                "-" if r["conformant"] is None else ("yes" if r["conformant"] else "**NO**"),
                "-" if r["covers"] is None else ("yes" if r["covers"] else "**NO**"),
                r["sha256"]))

    table("The determinism self-test's two runs (NOT campaign runs)", self_rows)
    table("The campaign's runs", rows)

    total = sum(r["bytes"] for r in rows + self_rows)
    print()
    print("**%d capture(s), %d bytes (%.1f MB) not committed.**"
          % (len([r for r in rows + self_rows if r["capture"] != "(none)"]), total, total / 1e6))
    nonconf = [r for r in rows + self_rows if r["conformant"] is False]
    print()
    if nonconf:
        print("**%d capture(s) were NOT conformant**, and they are named rather than summarised: %s."
              % (len(nonconf), ", ".join(r["run"] for r in nonconf)))
    else:
        print("**Every capture read back conformant** by this project's own reader, on the run")
        print("that produced it.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        raise SystemExit(2)
    emit(sys.argv[1])
