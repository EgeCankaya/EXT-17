# `demo` - capture manifest

**The captures themselves are not committed.** One 1200-frame run of Atacama Air
Defense writes about 24 MB of JSON Lines, so this campaign's captures are roughly
16 MB. `.gitignore` has excluded them since the repository was created, with the
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
python tools\m6-checks\campaign_manifest.py campaigns\demo > campaigns\demo\MANIFEST.md
```

## The determinism self-test's two runs (NOT campaign runs)

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | - | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 4098831 | 8530 | 2 | yes | yes | `a19c262d2ef7ff79b9236abe183073ca73dcb0fa90259da90980a0001ddf37f0` |
| 001 | - | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 4124995 | 8586 | 2 | yes | yes | `3fde0be407241850b6d1f66ccbb6889b1560315642224bb7e6aa3308835f7f8b` |

## The campaign's runs

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | - | fail | `capture-atacama-air-defense-000.n8rocap.jsonl` | 4106939 | 8545 | 2 | yes | yes | `b4c73d739285e7f66b60c28813b321d758d62bc734499696bebab3caeb88315b` |
| 001 | - | infrastructure_error | `(none)` | 0 | - | - | - | - | `-` |
| 002 | - | fail | `capture-atacama-air-defense-002.n8rocap.jsonl` | 4122320 | 8580 | 2 | yes | yes | `3148589efc68834b802a703140320fd07e1e05f34113f7f2a2cba8552fbe01ae` |

**4 capture(s), 16453085 bytes (16.5 MB) not committed.**

**Every capture read back conformant** by this project's own reader, on the run
that produced it.
