# `m4-bytes` - capture manifest

**The captures themselves are not committed.** One 1200-frame run of Atacama Air
Defense writes about 24 MB of JSON Lines, so this campaign's captures are roughly
49 MB. `.gitignore` has excluded them since the repository was created, with the
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
python tools\m6-checks\campaign_manifest.py campaigns\m4-bytes > campaigns\m4-bytes\MANIFEST.md
```

## The determinism self-test's two runs (NOT campaign runs)

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | - | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 24311271 | 50428 | 2 | yes | yes | `8e8af5bdc18542a24f6fc77f22d81751a0b56eee683ced0f4fd4cd604e9fe9eb` |
| 001 | - | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 24301091 | 50404 | 2 | yes | yes | `37184be2e2f0b270ab60a1669987f920966bf827ef7fa317fa28ec0d377d2f75` |

**2 capture(s), 48612362 bytes (48.6 MB) not committed.**

**Every capture read back conformant** by this project's own reader, on the run
that produced it.
