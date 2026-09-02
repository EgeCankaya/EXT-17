# `m2-oq1` - capture manifest

**The captures themselves are not committed.** One 1200-frame run of Atacama Air
Defense writes about 24 MB of JSON Lines, so this campaign's captures are roughly
486 MB. `.gitignore` has excluded them since the repository was created, with the
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
python tools\m6-checks\campaign_manifest.py campaigns\m2-oq1 > campaigns\m2-oq1\MANIFEST.md
```

## The campaign's runs

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | - | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 24311063 | - | - | - | - | `fb9f2af5144fcd53d39b4c529cd576bb9eaa49af75042213df3b239a40c3e33f` |
| 001 | - | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 24299416 | - | - | - | - | `3070ff7f64fae363f3701831f25427ccf4fcc42d91ca65379a99b3cdc3513ebf` |
| 002 | - | completed | `capture-atacama-air-defense-002.n8rocap.jsonl` | 24317983 | - | - | - | - | `2eff04b18d5ffc028bbb12b71f74ed514d27c5f7ea94b05dc17a3389db4cc15e` |
| 003 | - | completed | `capture-atacama-air-defense-003.n8rocap.jsonl` | 24338870 | - | - | - | - | `b82c3d79ab050aa788a83b7a18c0476f993dd6a0f7974845d9d14ed6a8281d81` |
| 004 | - | completed | `capture-atacama-air-defense-004.n8rocap.jsonl` | 24287007 | - | - | - | - | `0652d9547fa22d412335a6d84b9dd3c3bee5eca1ef9ace0d8bf4c71b853f45e4` |
| 005 | - | completed | `capture-atacama-air-defense-005.n8rocap.jsonl` | 24313788 | - | - | - | - | `c97bb5a1b729a6148375802f84f412cd65745f151dfbe22d5dd0703c5d74f655` |
| 006 | - | completed | `capture-atacama-air-defense-006.n8rocap.jsonl` | 24382694 | - | - | - | - | `c34b29115ba1270784730325f5de286a5cf3cb257dd2f02667ed690a8bf2a96d` |
| 007 | - | completed | `capture-atacama-air-defense-007.n8rocap.jsonl` | 24324864 | - | - | - | - | `0c0b227e794f57551eee8bb73207f133453d116e4ae46a0a242a542aa093dc07` |
| 008 | - | completed | `capture-atacama-air-defense-008.n8rocap.jsonl` | 24334967 | - | - | - | - | `59a0a129d0b95654f4ddb36db8a60bda1b2e3637cf68c914202d2cf64822d12f` |
| 009 | - | completed | `capture-atacama-air-defense-009.n8rocap.jsonl` | 24315450 | - | - | - | - | `4132d6846909a14b73cd56ad7edef9d9072be02d312934ce538c20c577681691` |
| 010 | - | completed | `capture-atacama-air-defense-010.n8rocap.jsonl` | 24344026 | - | - | - | - | `1f87ffbcede28ab73a33ff974e0cd74b04c062c6a51147607625aa6f9861a055` |
| 011 | - | completed | `capture-atacama-air-defense-011.n8rocap.jsonl` | 24299845 | - | - | - | - | `aa710bba5128f1b97219210e0dcd7d8b40eb423e8bb0b801b5c701de38a0ebd2` |
| 012 | - | completed | `capture-atacama-air-defense-012.n8rocap.jsonl` | 24280305 | - | - | - | - | `3e6ee054499145ecc94ab8fe244c5d07674c8ae98a56488f0ee72b5a30193c1e` |
| 013 | - | completed | `capture-atacama-air-defense-013.n8rocap.jsonl` | 24327208 | - | - | - | - | `d01453799e07b9f4d2dd01afcbc828a41f3e4d266b2a5fb10a6015f7bfd07a9d` |
| 014 | - | completed | `capture-atacama-air-defense-014.n8rocap.jsonl` | 24311801 | - | - | - | - | `2cc122c74d6d8ec049bb65bb9fffe116d53c33b9b75c6fc13158e73f213d5247` |
| 015 | - | completed | `capture-atacama-air-defense-015.n8rocap.jsonl` | 24288102 | - | - | - | - | `b682c55cf49259667dc54022cfbbf042bd29861a4efa66bb46e7bc4aa4178c87` |
| 016 | - | completed | `capture-atacama-air-defense-016.n8rocap.jsonl` | 24321853 | - | - | - | - | `e2b0389c48facf26cf8478753373830382788304944989143930d7de2b2c374a` |
| 017 | - | completed | `capture-atacama-air-defense-017.n8rocap.jsonl` | 24280305 | - | - | - | - | `3e6ee054499145ecc94ab8fe244c5d07674c8ae98a56488f0ee72b5a30193c1e` |
| 018 | - | completed | `capture-atacama-air-defense-018.n8rocap.jsonl` | 24360134 | - | - | - | - | `ec8dda107ca2a9eb52c3065f1115559a3f0c9a8ec358032bb7232a2718470b68` |
| 019 | - | completed | `capture-atacama-air-defense-019.n8rocap.jsonl` | 24320078 | - | - | - | - | `b828d48e0d0e2bd2e3bbafc4fb8d85d50a8cc19dd56ca0b8174f71fee01a6a3d` |

**20 capture(s), 486359759 bytes (486.4 MB) not committed.**

**Every capture read back conformant** by this project's own reader, on the run
that produced it.
