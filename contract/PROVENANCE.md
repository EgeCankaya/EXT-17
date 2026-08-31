# What crossed the boundary from EXT-08, and what did not

EXT-08 and EXT-17 are **separate git repositories with no shared source**. That is not
incidental — several of EXT-08's requirements exist only because of it, and its capture format
was designed as a deliverable rather than an implementation detail specifically so that this
boundary could hold.

Everything in this directory is **vendored, pinned and read-only from EXT-17's point of view**.
Do not edit these files. If one of them is wrong or insufficient, that is a defect in EXT-08's
contract and it goes back there.

**Pinned at EXT-08 commit `78fd4ef`**, format version `n8ro-capture/1`, producer `0.9.0`.

The specification last changed at EXT-08 `9e63315` (producer 0.9.0, BTB-CAP-6); `78fd4ef` is
EXT-08's `main` at the time of pinning and carries that file unchanged. Pinning the branch
head rather than the last commit that touched the file is deliberate — it makes "is this
current?" one comparison against `main` rather than a question about which commit last
mattered.

This is the **third** pin, and the drift is worth stating as a live hazard rather than a
footnote:

| Pin | EXT-08 commit | Producer | Went stale because |
|---|---|---|---|
| 1st | `eedc228` | 0.7.0 | Stale within the hour — producer 0.8.0 added `header.sample_form` |
| 2nd | `063b5ba` | 0.8.0 | Producer 0.9.0 added `header.limits`, `header.part`, `header.continues_from` and `trailer.continued_in` (BTB-CAP-6) |
| 3rd | `78fd4ef` | 0.9.0 | Current |

**In both cases the addition was non-breaking and the freeze held — which is the pattern, not a
coincidence.** EXT-08's format was designed so its producer can grow without moving the version:
§13 makes an added key non-breaking, and CAP-6 needed no new record type and no new value in any
closed vocabulary, because `size_limit` was already in the closed sets for `trailer.end_reason`
and `segment_close.reason`. `n8ro-capture/1` has never changed.

**What that means for EXT-17's rules: none of them move.** CR-CAP-3's version rejection,
CR-CAP-2's ignore-unknown-keys criterion, CR-CAP-4's segment and occupancy handling and
CR-DET-1's content comparison all hold exactly as written. A vendored copy still drifts, though,
and a reader working from a stale one cannot interpret a key it meets. Re-check this pin before
relying on it, and treat a drifted `contract/` as a defect to fix rather than a difference to
tolerate.

**What the current pin adds**, and what EXT-17 can now use: see finding 8 below and PRD rev 2.
In one line — the recorder can bound and rotate its own captures, so CR-CAP-5 can hand each run
a per-capture byte bound instead of only projecting one.

## What is here

| File | What it is | Status in EXT-08 |
|---|---|---|
| `capture-format-v1.md` | The capture format specification, field by field | **FROZEN** at EXT-08's M7. A change to what it specifies is now a version bump and a downstream change — for us |
| `capture-atacama-air-defense-sample.n8rocap.jsonl` | A real capture from a real run, trimmed to 3.2 MB | Every structural record kept; two entities' samples. Reports CONFORMS against the spec above |
| `condition-file-schema.md` | The referee's condition-file shape | Reference only. EXT-08's OQ-6 resolved that EXT-17 may adopt or supersede it |
| `example.conditions.json` | A working condition file for the reference scenario | Reference only |

## What is deliberately NOT here

**No EXT-08 source.** Not a header, not a snippet, not a class name relied upon. If EXT-17
needs a behaviour from EXT-08 that is not in the specification above, **that is a defect in the
specification** — raise it there rather than reading around it. EXT-08's own PRD says so in
those words, and the whole point of freezing the format was to make that possible.

The practical test: a reader for this format was written inside EXT-08 from
`capture-format-v1.md` alone, linking neither the bridge nor the N8RO SDK. If it could be done
there it can be done here.

## The measured findings that bind EXT-17's design

These are the ones that change what EXT-17 should build, not merely what it should know.
Each was measured, not reasoned about; the sections named carry the numbers.

### 1. A byte-for-byte determinism gate cannot pass on this platform — and the simulation is still reproducible

EXT-08's brief and this project's both reach for "two identical runs produce identical
captures". Measured on the **headless** `n8ro-sim-app.exe`, two runs each stopped at exactly
frame 1200:

- **Byte comparison fails.** The files differ, and differ in length.
- **Content comparison passes completely.** 50 358 samples compared per `(entity, occupancy)`
  aligned on `sim_time_s` — **50 358 agree, zero differ**.

The runs disagree only about *which frames were published* — 83 samples across 4 of ~1 198
frames, about 0.2%. The wall-clock-paced `n8ro-sim-local` is worse, at ~1%.

**So the simulation is reproducible and its publication schedule is not.** A determinism
self-test built on byte equality will fail, and it will be reporting the schedule rather than
the simulation. **Key it on content.** See `capture-format-v1.md` §14.

### 2. `sim_time_s` is not a key, and a frozen-clock segment cannot be aligned at all

The engine's stop path resets the simulation clock *before* republishing the whole roster, so
inside that segment every sample carries `sim_time_s = 0.0` — about **93 per entity**. Nothing
distinguishes one from the next, so two runs cannot be aligned there: lose one early sample and
everything after shifts by one.

Detect such a segment and exclude it. The test is exact rather than a heuristic: in a running
segment each entity publishes once per frame, so the maximum number of samples any one
`(entity, occupancy)` carries at a single `sim_time_s` is 1. See §5.1 and §14.

### 3. A single ordinary run contains TWO segments

Because the stop path unloads and reloads, one run of one scenario produces segment 0 (the run)
and segment 1 (the teardown reload, holding the re-created roster at occupancy 2 and a short
tail of samples at `sim_time_s = 0.0`). This is not a producer artifact. Any statistic computed
over a whole capture without segmenting it is wrong. See §16.

### 4. An entity's identity is `(name, occupancy)`, never name

A scenario entity name is unique within a tenure, not across a run. The engine destroys and
re-creates entities under the same name — both mid-run and at teardown. Keying on name alone
silently merges two different bodies. See §8.1.

### 5. All-zero counters are not proof that nothing was lost

Measured against the host's own record, a capture was short by 30 samples in a single frame
with every counter reading zero. Under three times the load it was complete. **And the host's
own record loses whole frames too**, so it bounds completeness from one side only and is not
ground truth.

A campaign that reads the absence of a message as evidence it never happened will draw a wrong
conclusion from a file that looks perfectly clean. See §14, "Known loss".

### 6. The headless host's invocation, which EXT-17 needs outright

```
n8ro-sim-app.exe --sim-config SimEngineHost_SharedMemory ^
                 --model-path C:\N8RO\data\db --schema-file N8roSimSchema
```

**It takes no scenario argument.** It hosts an engine and subscribes to its command topic;
loading a scenario is a separate step published on `sim/scenario/command`
(`{"command":"load_scenario","scenarioName":"..."}`), and starting is `{"command":"start"}` on
`sim/engine/command`.

**Bound a run by frame number, not by wall-clock time.** Two runs stopped after the same number
of seconds have not covered the same simulation, and their captures are then guaranteed to
differ for a reason that has nothing to do with determinism. This is what made finding 1
measurable at all.

Established by observation inside EXT-08, not by the client — worth confirming with the mentor,
since what is demonstrated is that it *works*, not that it is the intended production shape.
**EXT-08 has closed its own version of this question** (its OQ-2, at PRD rev 12) on the grounds
that nothing it ships changes with the answer, and passed the confirmation here: it is
**EXT-17's OQ-3**, and this is the project that runs the host in production. `[B]` asks its own
reader to confirm it, so the ask lands here by the brief's own instruction, not by delegation.

### 8. The recorder can bound its own captures, and rotate them — as of producer 0.9.0

Not a finding about the platform but about the upstream tool, and it changes what EXT-17 has to
build. EXT-08's `n8ro-bridge` accepts:

```
--capture-max-bytes <n>        maximum size of ONE capture file; 0 (default) means unbounded
--on-size-limit stop|rotate    what happens on reaching it; stop is the default
```

`stop` closes the capture with a well-formed `trailer` carrying `end_reason: "size_limit"` and
ends the run. `rotate` closes it the same way and continues into
`capture-<scenario>-<label>.partNNN.n8rocap.jsonl`. **No line is ever cut**: the bound is checked
against a record's exact length before the record is written, and space is reserved in advance
for the close. The bound in force is recorded in `header.limits`, so it is recoverable from the
file rather than only from the command that produced it.

**Two things follow for EXT-17.**

First, **the disk risk is smaller than rev 1 assumed but has not moved to someone else.** The
bound is per *capture file*. A campaign is many of them, so a campaign-level ceiling is still
EXT-17's own concern (CR-CAP-5) — but each run can now be given a bound whose overrun produces a
closed, valid, explicitly-truncated capture instead of an ENOSPC-corrupted one.

Second, **if you configure `rotate`, a run's capture is a set of files, not a file.** Every part
is a complete, independently valid capture with its own header, its own schemas and its own
trailer, so a reader that knows nothing about rotation reads any part correctly. But the parts
are linked only by `header.part` / `header.continues_from` / `trailer.continued_in`, and
**segment ordinals restart at 0 in every part** — so a per-segment statistic across a set must
key on `(part, segment)`, and a segment cut by a rotation shows as a `segment_close` with
`reason: "size_limit"` in one part and a `segment_open` in the next. Choosing `stop` avoids all
of it and keeps one file per run. See upstream §6.6 and §6.7, and EXT-17's OQ-6.

### 7. Start the recorder before the host

The `entity_created` burst that fills the roster is published once, at scenario load. A
recorder attached later sees samples for entities it never saw created and records nothing but
orphans. The capture says which happened — `header.attached_mid_run` and
`trailer.drops.samples_orphaned` — but a campaign that gets the order wrong has collected
nothing.

One consequence that reads like corruption and is not: in a late-attached capture
`entities_added` and `entities_removed` **do not balance**.
