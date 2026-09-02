# `m4-gate` - capture manifest

**The captures themselves are not committed.** One 1200-frame run of Atacama Air
Defense writes about 24 MB of JSON Lines, so this campaign's captures are roughly
97 MB. `.gitignore` has excluded them since the repository was created, with the
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
python tools\m6-checks\campaign_manifest.py campaigns\m4-gate > campaigns\m4-gate\MANIFEST.md
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
| 000 | - | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 24310961 | 50427 | 2 | yes | yes | `22b80a7cd87753e4779d9caf74e147a9ab5e912c4f0ccf7858cf903776eb7e3f` |
| 001 | - | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 24349991 | 50512 | 2 | yes | yes | `e3a3883f7e69da5687a6262dac1aedcc64382e8ae54a470c08854d2286cd5996` |

## The campaign's runs

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | - | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 24320663 | 50451 | 2 | yes | yes | `842acfb65cd3c8bfa0f6ef0a757fb9e76cd37c32cbe435a1a08c2b9eda39bfe0` |
| 001 | - | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 24311131 | 50425 | 2 | yes | yes | `6a04f174df35f8366fd8863340933637cb97cc42ef8e06a1aa413aeb0dc4042a` |

**4 capture(s), 97292746 bytes (97.3 MB) not committed.**

**Every capture read back conformant** by this project's own reader, on the run
that produced it.
