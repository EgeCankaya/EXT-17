# `m5-sweep-first` - capture manifest

**The captures themselves are not committed.** One 1200-frame run of Atacama Air
Defense writes about 24 MB of JSON Lines, so this campaign's captures are roughly
218 MB. `.gitignore` has excluded them since the repository was created, with the
reason stated there: artifacts to produce and check, not to version.

This manifest is what stands in for them. Everything a **judgement** rests on IS
committed — `run.json`, `verdicts.jsonl`, `campaign.json`, `campaign.log` and
`self-test.json` — because those are the report, and they are small.

**What this establishes, and what it does not.** It does not make a re-run
byte-identical: nothing does, and that is this project's central measurement — two
runs of one configuration are never byte-identical here, so a SHA-256 below will not
match a re-run and is not meant to. What it establishes is that these specific files
existed, what was in them, and what was concluded from them, so that a number in
this project's own report can be traced to a file whose contents are pinned even though
the file is not in the tree. **The counts are the part a re-run can be compared**
**against**, because the reports are computed from those rather than from the bytes.

Regenerate with:

```cmd
python tools\m6-checks\campaign_manifest.py campaigns\m5-sweep-first > campaigns\m5-sweep-first\MANIFEST.md
```

## One superseded sentence, and why it was not edited

`campaign.json` and `selftest/self-test.json` here record OQ-2 as **`"oq2_ruling": "unanswered"`**.
That was true when this campaign ran and is not true now. **Later the same day —
2026-09-01 — OQ-2 was DECIDED by the DRI (content) and CONCURRED with by the mentor,
independently.** It has still never been ANSWERED by [B]'s author, and those three
words stay apart: see `docs/escalations.md` E-2.

**The artifacts were not rewritten, and that is the point.** They are evidence of an
execution, not documentation of a position. Re-running this campaign to refresh one
sentence would replace these measured numbers with different measured numbers, break
every SHA-256 below, and invalidate the figures the README quotes from them - to
correct wording that changed no behaviour.
**No code changed when OQ-2 was decided**: `content` was already the default, and
this campaign's gate ran on it.

The binaries print the current three-part wording, and `src/run/SelfTest.cpp` writes
`oq2_decided_by`, `oq2_concurred_by` and `oq2_answered_by_brief_author` as three
separate keys. A campaign run today records `decided` and this section does not
appear - it is keyed on the artifact, not on the calendar.

## The determinism self-test's two runs (NOT campaign runs)

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | 55 | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 24357617 | 50441 | 2 | yes | yes | `2034f10933184bd909b817bcf03cd83c20fd280c637e7f1a957043af0cbb8285` |
| 001 | 55 | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 24364820 | 50456 | 2 | yes | yes | `218f3d219efc21b580f211ee9854dbf863efa3b74de7522108eb75c896ce1d5d` |

## The campaign's runs

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | 11 | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 24438234 | 50607 | 2 | yes | yes | `fad098ed60c3c3b72a461fe15532d9eecb1fda4b6ee2248199539bd26ce46284` |
| 001 | 27.5 | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 24494866 | 50550 | 2 | yes | yes | `07eb559435758112921551c8b449e8a396a411bccef9efe847997331d0ce885d` |
| 002 | 55 | completed | `capture-atacama-air-defense-002.n8rocap.jsonl` | 24334802 | 50390 | 2 | yes | yes | `671cec2b50036257a1076b8ea87b4e8b4334e3a551177f477540b5c24ec04ddc` |
| 003 | 82.5 | completed | `capture-atacama-air-defense-003.n8rocap.jsonl` | 24163368 | 49856 | 2 | yes | yes | `36b11926724b3fdae47fe9c1b30b0cead10a2e0baf774e765921c6577ba7e18f` |
| 004 | 110 | completed | `capture-atacama-air-defense-004.n8rocap.jsonl` | 23846988 | 49089 | 2 | yes | yes | `c4d4e04383668549113f5f2fedbcc9a0833b7bbfb019ba3447118ceca4ec3bd2` |
| 005 | 165 | completed | `capture-atacama-air-defense-005.n8rocap.jsonl` | 23852537 | 48780 | 2 | yes | yes | `3247506925897860ef92e3bad41170fbc7568d77c8c09eecec04a78f99e6b7b4` |
| 006 | 220 | completed | `capture-atacama-air-defense-006.n8rocap.jsonl` | 23728823 | 48646 | 2 | yes | yes | `392fc20ef0b7717981a8594a63730d3c6f74e7bb100de87c1e3ed952242458aa` |

**9 capture(s), 217582055 bytes (217.6 MB) not committed.**

**Every capture read back conformant** by this project's own reader, on the run
that produced it.
