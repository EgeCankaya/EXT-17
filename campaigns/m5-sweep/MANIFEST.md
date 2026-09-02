# `m5-sweep` - capture manifest

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
python tools\m6-checks\campaign_manifest.py campaigns\m5-sweep > campaigns\m5-sweep\MANIFEST.md
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
| 000 | 55 | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 24389976 | 50507 | 2 | yes | yes | `46bf4542c491f07256f4d41610cd707dd65943a3442c9524d9230dda490d0b47` |
| 001 | 55 | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 24351714 | 50426 | 2 | yes | yes | `832b158a399fb298c1f199a1c8bd70955711a9fb7e40449f241072d8c5b93dd2` |

## The campaign's runs

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | 11 | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 24431833 | 50588 | 2 | yes | yes | `51a2d33612e76ce1bc35cc83a44d1c87f7d893776a4720dc400aa1a273bbb839` |
| 001 | 27.5 | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 24533007 | 50626 | 2 | yes | yes | `8731a35859314d12fc767657818f71b3b320d91bb628a32a38542ffefc741273` |
| 002 | 55 | completed | `capture-atacama-air-defense-002.n8rocap.jsonl` | 24373272 | 50471 | 2 | yes | yes | `c01c365365dc4ddccb8efbf08d8353bc7a8592cf260fef842b5d73cda5120a31` |
| 003 | 82.5 | completed | `capture-atacama-air-defense-003.n8rocap.jsonl` | 24250346 | 50032 | 2 | yes | yes | `a9fb0a02884a8ecbb21bb6d11d0854a47d3b4d5f230839ff1f1f52641deff07d` |
| 004 | 110 | completed | `capture-atacama-air-defense-004.n8rocap.jsonl` | 23902558 | 49210 | 2 | yes | yes | `4885f50073890c466e64bb439ba97625d93794fc671b35417cac95c3609cdfa3` |
| 005 | 165 | completed | `capture-atacama-air-defense-005.n8rocap.jsonl` | 23780640 | 48635 | 2 | yes | yes | `27056aae5ef37de6889322fedc875b38b6172a635251b13034f4a3664be80322` |
| 006 | 220 | completed | `capture-atacama-air-defense-006.n8rocap.jsonl` | 23845233 | 48884 | 2 | yes | yes | `1012c637ec3ccbbf886d0fa29ca7683f0d5f32e3e8fc2da0cda8a5976205adef` |

**9 capture(s), 217858579 bytes (217.9 MB) not committed.**

**Every capture read back conformant** by this project's own reader, on the run
that produced it.
