# `m4-frozen` - capture manifest

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
python tools\m6-checks\campaign_manifest.py campaigns\m4-frozen > campaigns\m4-frozen\MANIFEST.md
```

## The determinism self-test's two runs (NOT campaign runs)

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | - | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 24368977 | 50550 | 2 | yes | yes | `aab86f767c0d42af3b7665c125676e36ebb9167383e84bf0d16dd1ec60009bc5` |
| 001 | - | completed | `capture-atacama-air-defense-001.n8rocap.jsonl` | 24327507 | 50464 | 2 | yes | yes | `46d035c5c61759be464a05dfe5def78febc9b7aee82a6d8c1c710520af566d67` |

**2 capture(s), 48696484 bytes (48.7 MB) not committed.**

**Every capture read back conformant** by this project's own reader, on the run
that produced it.
