# `m6-campaign` - capture manifest

**The captures themselves are not committed.** One 1200-frame run of Atacama Air
Defense writes about 24 MB of JSON Lines, so this campaign's captures are roughly
527 MB. `.gitignore` has excluded them since the repository was created, with the
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
python tools\m6-checks\campaign_manifest.py campaigns\m6-campaign > campaigns\m6-campaign\MANIFEST.md
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
| 000 | 55 | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 24360050 | 50446 | 2 | yes | yes | `1a2f4dfeb6ee420ef628ea8a7b3d071328da669431147249fa6c90ce9b62ee0b` |
| 001 | 55 | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 24332167 | 50386 | 2 | yes | yes | `95889cb0d28fe84324d94678a077c9cef7079919271e2d515d3491828a8dbf0f` |

## The campaign's runs

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | 11 | fail | `capture-atacama-air-defense-000.n8rocap.jsonl` | 24404910 | 50534 | 2 | yes | yes | `0877ba63d86f263523703144c1216cf22ec0895e6e3fbd427f46089e69286f0a` |
| 001 | 22 | fail | `capture-atacama-air-defense-001.n8rocap.jsonl` | 24525968 | 50723 | 2 | yes | yes | `3df6895381e14f7ee7bf465958ba187e9b05e4e7b0d58f681f383e14a3e6e863` |
| 002 | 33 | fail | `capture-atacama-air-defense-002.n8rocap.jsonl` | 24376804 | 50447 | 2 | yes | yes | `2d88e0248a1c7e34be86d46be421d211f4cb85cf1c9d5947564ea696c3298ad0` |
| 003 | 44 | fail | `capture-atacama-air-defense-003.n8rocap.jsonl` | 24318999 | 50347 | 2 | yes | yes | `51b2413d5925cbac687806ff305c6e96f4f86884282c0eae914522d60e66f477` |
| 004 | 55 | fail | `capture-atacama-air-defense-004.n8rocap.jsonl` | 24342376 | 50410 | 2 | yes | yes | `f394441b82cb71ccb3d456b7896faa79de6772eeaf88624a7574fe73446a4b12` |
| 005 | 70 | fail | `capture-atacama-air-defense-005.n8rocap.jsonl` | 24361098 | 50495 | 2 | yes | yes | `2c9fd79f9b411cf9fc107bbbced1b88bd47ce838ac7809842bed27e344506266` |
| 006 | 85 | fail | `capture-atacama-air-defense-006.n8rocap.jsonl` | 24311941 | 50240 | 2 | yes | yes | `932400a5f5b25c8a1eda09b45b7567e0894c1247754d77fad6c6706b0b16454d` |
| 007 | 100 | fail | `capture-atacama-air-defense-007.n8rocap.jsonl` | 24180453 | 49828 | 2 | yes | yes | `aed865549b98dce9fe1e3d9624e5ce2afe8c6ed1476ba2397d3244f02a59dd40` |
| 008 | 115 | fail | `capture-atacama-air-defense-008.n8rocap.jsonl` | 23824689 | 48970 | 2 | yes | yes | `076cf7d0e499c85546d5c610360655d74a646b06cac90249de500a2304f88b55` |
| 009 | 130 | fail | `capture-atacama-air-defense-009.n8rocap.jsonl` | 23592218 | 48476 | 2 | yes | yes | `f657e52b3d74f62b28b640a816b4a4b6d985d1d8283cc7d8fcde2a9cee073b0e` |
| 010 | 150 | infrastructure_error | `capture-atacama-air-defense-010.n8rocap.jsonl` | 23925805 | 49007 | 2 | yes | yes | `64b4affdc1abb559cfc25ded21aa5044a7d07ec64f9af95d7d9547c4501f8013` |
| 011 | 170 | pass | `capture-atacama-air-defense-011.n8rocap.jsonl` | 23935308 | 48927 | 2 | yes | yes | `063ea8c2d18522d2f7432361d3321a0bc4ed4bfbd1559dda8388ef1039929f5d` |
| 012 | 190 | pass | `capture-atacama-air-defense-012.n8rocap.jsonl` | 23586984 | 48273 | 2 | yes | yes | `8a80c8b2f26e7a7d988d348878581ea20e4c929afd88ad5ea6d6ff7d0b0392bf` |
| 013 | 210 | infrastructure_error | `capture-atacama-air-defense-013.n8rocap.jsonl` | 23346149 | 47866 | 2 | yes | yes | `3773ef44a7213fd603c02a9d9bfa5d0fb8e86c77f065013ed72256d6b6a6cbac` |
| 014 | 240 | pass | `capture-atacama-air-defense-014.n8rocap.jsonl` | 23286367 | 47916 | 2 | yes | yes | `0854503a1a9ff050ee80f9605974a51206e5eb91d7e702aa13cb11e5f4fd29f5` |
| 015 | 270 | pass | `capture-atacama-air-defense-015.n8rocap.jsonl` | 24533682 | 50275 | 2 | yes | yes | `13c992246798359ac677c076bec6b42ef562576baf02069187c41e8b1e77e791` |
| 016 | 300 | pass | `capture-atacama-air-defense-016.n8rocap.jsonl` | 23296115 | 47990 | 2 | yes | yes | `3b3297358c6113f3ee753ad5f950b680bc583f1a7015aed15308b15921bb92b1` |
| 017 | 330 | pass | `capture-atacama-air-defense-017.n8rocap.jsonl` | 23435236 | 48274 | 2 | yes | yes | `1d19d173a11d6623277180d83b26d23b23ca6356d83440d0e62071f6882459f7` |
| 018 | 355 | pass | `capture-atacama-air-defense-018.n8rocap.jsonl` | 23169481 | 47746 | 2 | yes | yes | `f7c880a5397672a75b56290ffde727ef5b592a3c0cf875d14dedb0f48b9f03ab` |
| 019 | 380 | pass | `capture-atacama-air-defense-019.n8rocap.jsonl` | 23215747 | 47929 | 2 | yes | yes | `53ab1a9d770e2414f423fe23411f526f092610d117b226e72f506cd4f9ae19f4` |

**22 capture(s), 526662547 bytes (526.7 MB) not committed.**

**Every capture read back conformant** by this project's own reader, on the run
that produced it.
