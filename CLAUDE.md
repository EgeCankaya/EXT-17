# CLAUDE.md — EXT-17 Headless Campaign Runner

> **Provisional.** Written at repo creation, before the PRD exists, to carry the two things
> that would be expensive to get wrong first. Rewrite it properly once `docs/prd.md` lands.

## What this repo is

The downstream half of a two-project pair. EXT-08 records a simulation run into a durable,
versioned capture file and judges it; **EXT-17 runs the campaign** — executing many runs,
varying parameters, asserting over the results, and reporting across them.

The client brief is `docs/EXT-17-Headless-Campaign-Runner.docx`. It is the binding contract.
It is untracked (proprietary), so read it off disk.

## The rule that matters most

**EXT-08 and EXT-17 are separate repositories with no shared source.** Not a header, not a
snippet, not a class name relied upon.

Everything EXT-17 gets from EXT-08 crosses that boundary as a **documented, versioned
artifact**, and all of it is vendored in `contract/`:

- `contract/capture-format-v1.md` — the capture format, **frozen** at EXT-08's M7
- `contract/capture-*.n8rocap.jsonl` — a real capture, as a test fixture
- `contract/condition-file-schema.md` — the referee's condition shape, reference only
- `contract/PROVENANCE.md` — **read this first.** What crossed, what did not, and the seven
  measured findings from EXT-08 that bind what EXT-17 should build

`contract/` is read-only from here. If something in it is wrong or insufficient, that is a
defect in EXT-08's contract and it goes back there rather than being worked around.

The practical proof this is possible: EXT-08 wrote a conformance reader for its own format
from `capture-format-v1.md` alone, linking neither its bridge nor the N8RO SDK. If it could be
done there it can be done here.

## Pin the format version, and reject anything else

A reader that meets a `format_version` it does not implement **rejects the file with a named
error and stops**. It does not attempt a partial parse. That is the entire compatibility
mechanism and it is deliberately blunt: partial parsing of an unknown format is how
silently-wrong analysis happens.

Currently `n8ro-capture/1`.

A key you do not recognise inside a record type you *do* recognise is **not** a version change
and must be ignored, not rejected (format §13). That is the whole reason the version has held
across three producer releases, and it has already mattered twice: producer 0.8.0 added
`header.sample_form`, and 0.9.0 added `header.limits`, `header.part`, `header.continues_from`
and `trailer.continued_in`. Reject on the **version**; ignore unknown **keys**.

## Three things that will bite before anything else

Full detail and numbers in `contract/PROVENANCE.md`. In short:

1. **A byte-for-byte determinism gate cannot pass on this platform.** Measured: two headless
   runs stopped at the same frame agree on 50 358 of 50 358 samples and are still not
   byte-identical, because ~0.2% of frames go unpublished, differently each run. Key any
   determinism self-test on **content**, not bytes.
2. **A single ordinary run contains two segments**, because the engine's stop path unloads and
   reloads. Any statistic computed over a whole capture without segmenting it is wrong.
3. **An entity's identity is `(name, occupancy)`, never name.** The engine re-creates entities
   under the same name, mid-run and at teardown.

And one that is a choice rather than a trap: the recorder can bound and **rotate** its captures
(`--capture-max-bytes`, `--on-size-limit`, producer 0.9.0). Choose `rotate` and a run's capture
becomes a set of `.partNNN` files whose segment ordinals restart in each part; choose `stop` and
it stays one file. Prefer `stop` unless there is a reason not to — see PRD OQ-6 and
`contract/PROVENANCE.md` finding 8.

## Verified environment — do not re-derive

- Install: `C:\N8RO`, runtime 2.1.328, SDK component `com.n8ro.dev` 2.1.328.
- `C:\N8RO` is **read-only** for this project. Read headers, read docs, run binaries. Never
  write into it.
- Headless host: `n8ro-sim-app.exe --sim-config SimEngineHost_SharedMemory --model-path <dir>
  --schema-file <name>`. **It takes no scenario argument** — load and start are published on
  `sim/scenario/command` and `sim/engine/command`. See `contract/PROVENANCE.md` finding 6.
- `n8ro-sim-local.exe` needs `N8RO_RELEASE` set or it refuses the scenario load, and it writes
  a per-entity JSONL dump into its working directory — run it from a scratch dir.

## Conventions

- Files in this repo are ours — **no Arkheon proprietary header**. That convention applies only
  to files created inside `C:\N8RO`, which this project does not do.
