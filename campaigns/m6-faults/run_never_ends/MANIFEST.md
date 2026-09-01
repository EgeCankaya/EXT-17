# `run_never_ends` - capture manifest

**The captures themselves are not committed.** One 1200-frame run of Atacama Air
Defense writes about 24 MB of JSON Lines, so this campaign's captures are roughly
33 MB. `.gitignore` has excluded them since the repository was created, with the
reason stated there: artifacts to produce and check, not to version.

This manifest is what stands in for them. Everything a **judgement** rests on IS
committed — `run.json`, `verdicts.jsonl`, `campaign.json`, `campaign.log` and
`self-test.json` — because those are the report, and they are small.

**What this establishes, and what it does not.** It does not make a re-run
byte-identical: nothing does, and that is this project's central measurement — two
runs of one configuration are never byte-identical here, so a SHA-256 below will not
match a re-run and is not meant to. What it establishes is that these specific files
existed, what was in them, and what was concluded from them, so that a number in
`docs/m6-assertions.md` can be traced to a file whose contents are pinned even though
the file is not in the tree. **The counts are the part a re-run can be compared**
**against**, because the reports are computed from those rather than from the bytes.

Regenerate with:

```cmd
python tools\m6-checks\campaign_manifest.py campaigns\m6-faults\run_never_ends > campaigns\m6-faults\run_never_ends\MANIFEST.md
```

## One superseded sentence, and why it was not edited

`campaign.json` and `selftest/self-test.json` here record OQ-2 as **`"oq2_ruling": "unanswered"`**.
That was true when this campaign ran and is not true now. **Later the same day —
2026-09-01 — OQ-2 was DECIDED by the DRI (content) and CONCURRED with by the mentor,
independently.** It has still never been ANSWERED by [B]'s author, and those three
words stay apart: see `docs/m7-oq2-oq3.md` and `docs/escalations.md` E-2.

**The artifacts were not rewritten, and that is the point.** They are evidence of an
execution, not documentation of a position. Re-running this campaign to refresh one
sentence would replace these measured numbers with different measured numbers, break
every SHA-256 below, and invalidate the figures `docs/m6-assertions.md` and
`docs/m7-oq2-oq3.md` quote from them - to correct wording that changed no behaviour.
**No code changed when OQ-2 was decided**: `content` was already the default, and
this campaign's gate ran on it.

The binaries print the current three-part wording, and `src/run/SelfTest.cpp` writes
`oq2_decided_by`, `oq2_concurred_by` and `oq2_answered_by_brief_author` as three
separate keys. A campaign run today records `decided` and this section does not
appear - it is keyed on the artifact, not on the calendar.

## The determinism self-test's two runs (NOT campaign runs)

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | - | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 6179705 | 12845 | 2 | yes | yes | `b351c1f9e555e36fad88261618b809247a7f95b40635630b46bddf1ad1415cb9` |
| 001 | - | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 6209069 | 12911 | 2 | yes | yes | `729b2e1ea9e34c66949a77e60cc27dfe6b7abd5fac5abd2309d8f259e850d995` |

## The campaign's runs

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | - | fail | `capture-atacama-air-defense-000.n8rocap.jsonl` | 6196427 | 12883 | 2 | yes | yes | `1fbd3c30e9570889c000e0c199e755b4c40ecf8970a84d1352df052926dba67c` |
| 001 | - | timeout | `capture-atacama-air-defense-001.n8rocap.jsonl` | 8223048 | 17105 | 1 | yes | yes | `2acca91cc7060e5cbcbcdc97c96fc3bab5c2d13e7422e5672e3cbfa1b763b41d` |
| 002 | - | fail | `capture-atacama-air-defense-002.n8rocap.jsonl` | 6198214 | 12887 | 2 | yes | yes | `1259e402dfb595ea61c73543984c7d8878b949ac23565d2bbfef63a070c505b0` |

**5 capture(s), 33006463 bytes (33.0 MB) not committed.**

**Every capture read back conformant** by this project's own reader, on the run
that produced it.
