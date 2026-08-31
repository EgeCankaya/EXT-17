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
- **`N8RO_RELEASE=C:\N8RO` must be set for the headless host too**, not only for
  `n8ro-sim-local`. Measured at M1: with it unset the host resolves its plugin directory from the
  current working directory, skips the plugin scan, never registers `componentPhysics` (from the
  stock `bin\plugins\sim\n8ro-physics.dll`), and **refuses every 42-entity scenario load** — while
  sitting idle rather than failing. `contract/PROVENANCE.md` finding 6 omits it; that is the
  contract's to correct, not ours. See `docs/m1-lifecycle.md` §7(a).
- **`C:\N8RO\bin` must be on `PATH`** for any binary of ours that links the SDK — it is where
  `n8ro-sim.dll` and `n8ro-core.dll` resolve from, and there is nowhere else. Measured at M2: a
  client launched from a scratch directory without it exits 53 having produced no output at all,
  which reads like a crash and is a missing DLL. This is a **second, separate** precondition from
  `N8RO_RELEASE`; setting one does not cover the other. `n8ro-campaign` sets both for its
  children and needs `PATH` itself.
- **Run any N8RO binary from a scratch directory.** `n8ro-sim-local.exe` writes a per-entity
  JSONL dump into its working directory, and `n8ro-sim-app.exe` creates `data/db/` and `logs/`
  there — it did so in this repo's root during M1 before the rule was known.
- **The host keeps its real log in the install tree, and appends to it across runs.**
  `C:\N8RO\logs\n8ro-logger-n8ro-sim-app.log` is one fixed filename that every host process
  **appends** to, so after twenty runs it holds all twenty (measured at M2: two identical runs
  produced a file of exactly twice one run's size, carrying two startup banners). Two
  consequences: redirected stderr is not a reliable record of a run — the same run wrote 5 777
  navigation errors to the log and, under PowerShell's redirection, three lines to `host.err` —
  and a campaign that wants one log per run must record the file's size before starting the host
  and copy only the bytes after it. `n8ro-campaign` does exactly that (`host_logger_offset` in
  each run record). A `.running` marker sits beside it, and an unclean exit makes the *next* host
  start rename the previous log as `.crash-<timestamp>.log`.
- **There is no shutdown command; ending the host is a process signal.** `sim/engine/command`'s
  vocabulary is closed at `start` / `stop` / `pause` / `step`
  (`docs/modules/n8ro-sim/dev/engine-lifecycle-and-control.md`). Measured at M2: a
  `CTRL_BREAK_EVENT` to the host's own process group shuts it down in an orderly way — the
  `.running` marker is removed and no crash rename follows — but Windows still reports exit
  `0xC000013A` (`STATUS_CONTROL_C_EXIT`), so **a non-zero host exit code is the normal case here
  and is not evidence of a crash**. `TerminateProcess` leaves the marker behind and does cause
  the crash rename, so prefer the control event and keep termination-by-handle as the fallback.
- The install ships **no geoid grid and no elevation service**, so every run floods with
  `TerrainElevationServiceClient` / `GeoidGridModel` errors. **Do not fix this.** Every
  measurement inherited from EXT-08 was taken in this configuration; provisioning terrain would
  invalidate the comparability of all of it. Setting `N8RO_RELEASE` does not change it — there is
  no grid under `C:\N8RO\data\geoid` to find.

## Conventions

- Files in this repo are ours — **no Arkheon proprietary header**. That convention applies only
  to files created inside `C:\N8RO`, which this project does not do.
