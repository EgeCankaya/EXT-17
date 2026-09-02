# `m4-overload` - capture manifest

**The captures themselves are not committed.** One 1200-frame run of Atacama Air
Defense writes about 24 MB of JSON Lines, so this campaign's captures are roughly
23 MB. `.gitignore` has excluded them since the repository was created, with the
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
python tools\m6-checks\campaign_manifest.py campaigns\m4-overload > campaigns\m4-overload\MANIFEST.md
```

## The campaign's runs

| run | value | outcome | capture | bytes | samples | segs | conformant | covers run | sha256 |
|---|---|---|---|---:|---:|---:|---|---|---|
| 000 | - | completed | `capture-atacama-air-defense-000.n8rocap.jsonl` | 22982093 | 47702 | 2 | yes | yes | `e76dbdf97025a3bfa3e94fde669fdabcfbaee3fb5415f0ffeda62ee16c5f0abe` |

**1 capture(s), 22982093 bytes (23.0 MB) not committed.**

**Every capture read back conformant** by this project's own reader, on the run
that produced it.
