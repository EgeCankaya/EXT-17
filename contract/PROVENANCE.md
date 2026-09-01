# What crossed the boundary from EXT-08, and what did not

EXT-08 and EXT-17 are **separate git repositories with no shared source**. That is not
incidental — several of EXT-08's requirements exist only because of it, and its capture format
was designed as a deliverable rather than an implementation detail specifically so that this
boundary could hold.

Everything in this directory is **vendored, pinned and read-only from EXT-17's point of view**.
Do not edit these files. If one of them is wrong or insufficient, that is a defect in EXT-08's
contract and it goes back there.

**Pinned at EXT-08 commit `ca5118c`**, format version `n8ro-capture/1`, producer `0.9.0`.

The specification last changed at EXT-08 `1dc2861` — **clarifications only**, see "The fourth
pin" below; `ca5118c` is EXT-08's `main` at the time of pinning and carries that file unchanged.
Pinning the branch head rather than the last commit that touched the file is deliberate — it
makes "is this current?" one comparison against `main` rather than a question about which commit
last mattered.

This is the **fourth** pin, and the drift is worth stating as a live hazard rather than a
footnote:

| Pin | EXT-08 commit | Producer | Went stale because |
|---|---|---|---|
| 1st | `eedc228` | 0.7.0 | Stale within the hour — producer 0.8.0 added `header.sample_form` |
| 2nd | `063b5ba` | 0.8.0 | Producer 0.9.0 added `header.limits`, `header.part`, `header.continues_from` and `trailer.continued_in` (BTB-CAP-6) |
| 3rd | `78fd4ef` | 0.9.0 | **EXT-08 fixed the four defects EXT-17 raised against this directory** — E-3, E-4, E-5 and E-6 |
| 4th | `ca5118c` | 0.9.0 | Current |

### The fourth pin, and why it is the different one

**The first three went stale because the producer grew. This one moved because EXT-17 pushed
four defects back across the boundary and EXT-08 fixed all four.** That is the `contract/`
discipline completing a full circuit for the first time — a defect found here, raised there,
corrected there, and vendored back — rather than being worked around here, and the circuit is
worth more than any of the four corrections.

**Nothing in this pin is a behaviour change.** All four fixes are documentation. No capture
changes, no key gains or loses a meaning, no reader that conformed before fails after, and
`n8ro-capture/1` did not move — EXT-08 lists both specification edits under §13's new
"Clarifications made after the freeze" for exactly that reason. What the re-pin had to verify is
that claim, and it did: the full test suite and `n8ro-capture read` on the vendored sample both
pass unchanged against the re-pinned files.

| Was | Now | Where it landed here |
|---|---|---|
| **E-3** — §6.7 said a run's totals are the sum across parts, which is false for `segments` | §6.7 rule 2 and §11 both say only four of the five counters sum, and how to correct `segments` | Finding 8 below |
| **E-4** — §5.1's frozen-clock test was read as "the clock was reset" | §5.1 states what a positive result establishes and lists **three** shapes; §14 says an emptied self-test is a refusal, not a pass | Finding 2 below |
| **E-5** — the condition digest was an excerpt of EXT-08's README that stopped one heading early | EXT-08 now owns `docs/condition-file-schema.md` carrying all four sections, with a test there that fails if it drifts from the README | `condition-file-schema.md` is now **vendored by identity** |
| **E-6** — EXT-08's README documented the headless invocation without `N8RO_RELEASE` | The R8 block states both preconditions and both failure modes | Finding 6 below |

**How to run the pin check, because a naive one fails on Windows.** Compare the **git blobs**,
not the working-tree files: both repositories store these with LF and check them out with CRLF,
so `diff` between two working trees reports a difference on every line while the tracked content
is identical. From EXT-17:

```
git rev-parse HEAD:contract/capture-format-v1.md
git -C ..\EXT-08 rev-parse ca5118c:docs/capture-format-v1.md
```

Equal hashes mean equal content. At this pin they are `a6974af…` for the format and `9d65ed0…`
for the condition schema.

**F-19 closes with this pin.** The condition digest could not be checked by identity, because no
file of that name existed upstream to check it against. One does now, and this directory's copy
is byte-identical to it — so a pin check is one comparison for all three vendored artifacts
rather than two comparisons and a judgement call about the third.

### The rule this pin adds

**A `fixed` row in `docs/escalations.md` is not finished until the pin has moved and the suite has
re-run.** For about an hour on 2026-09-01 this directory held a copy that this project's own
escalations file described as wrong while the upstream file was right — a state nothing here can
detect, because an issue closing on another repository is not an event anything watches. The
three earlier drifts were the producer growing and were caught by re-reading this file; this one
was **caused by this project's own success** and had no trigger at all.

So: raise it, and when it comes back fixed, re-pin in the same breath. That is F-38, and it is a
written rule rather than a check — which is stated plainly because the next one will be caught by
somebody remembering.

**What EXT-17 changed as a result: nothing.** Every correction confirmed a behaviour this
project had already implemented and stated — the corrected segment sum, the three frozen shapes,
the geodesy, the two environment preconditions. That is the useful outcome of raising them
rather than working around them: the arithmetic in `src/assert/Geodesy.h` is still this
project's own decision, and it now agrees with a vendored sentence instead of with an inference.

**In every earlier case the addition was non-breaking and the freeze held — which is the
pattern, not a coincidence.** EXT-08's format was designed so its producer can grow without moving the version:
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
| `condition-file-schema.md` | The referee's condition-file shape — **and since the fourth pin the arithmetic and the boundary rules too** | Reference only, in that EXT-08's OQ-6 resolved EXT-17 may adopt or supersede it. But it is now a real upstream file (`docs/condition-file-schema.md`) vendored **by identity** rather than an excerpt assembled here. E-5 fixed, F-19 closed |
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

**The test is exact; what a positive result MEANS is narrower than "the clock was reset", and
since the fourth pin §5.1 says so.** It establishes that *the segment cannot be aligned on
`sim_time_s`* — which is all a consumer needs, and is why the instruction to exclude does not
depend on the cause. Three shapes satisfy it: a reset clock; a burst published twice with
byte-identical values inside a segment whose clock ran normally; and a publication landing in
that same burst carrying values that **differ**. EXT-17 measured the second and third and raised
them as **E-4** — the second at 1 of 27 ordinary runs, the third at 4 of 35 parameterised ones,
and the third is the one this project causes for itself by updating entity state before `start`.
§14 now also carries the consequence EXT-17 pays: excluding these segments can leave a self-test
with nothing left to compare, at roughly 1 pair in 14, and **that is a refusal rather than a
pass** — retrying until a pair happens to come out comparable would turn a real refusal into a
silent one.

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
set N8RO_RELEASE=C:\N8RO
set PATH=C:\N8RO\bin;%PATH%
n8ro-sim-app.exe --sim-config SimEngineHost_SharedMemory ^
                 --model-path C:\N8RO\data\db --schema-file N8roSimSchema
```

**The two environment lines are not decoration, and until the fourth pin they were missing from
this finding and from EXT-08's README alike.** That omission is **E-6**, corrected upstream now.
With `N8RO_RELEASE` unset the host resolves its plugin directory from the current working
directory, skips the plugin scan, never registers `componentPhysics`, and **refuses every
42-entity scenario load while sitting idle rather than failing** — so an unattended campaign
hangs instead of breaking, which is the worse of the two. `C:\N8RO\bin` on `PATH` is a
**second, separate** precondition for anything linking the SDK: without it a binary exits **53**
having produced no output at all, which reads like a crash and is a missing DLL. Setting one
does not cover the other. Measured at EXT-17's M1 and M2 (F-17); the mentor confirmed on
2026-09-01 that `N8RO_RELEASE` **is** expected to be set in production, which is what turned
this from a local provisioning quirk into a defect worth another project's time.

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

**Four of OQ-3's six parts were answered by the mentor on 2026-09-01** — the environment
variable, the degraded terrain configuration, the console control event and its non-zero exit,
and `PATH`. Parts (a) and (c) were not covered by what was relayed and are **not** recorded as
answered. See `docs/escalations.md` E-1.

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

**Third, a rotated run's `counts.segments` is NOT the sum of its parts', and since the fourth
pin §6.7 rule 2 says so.** It follows directly from the sentence above — a cut segment is *one*
segment appearing in two files, so it is counted in both — but §6.7 used to state the summing
rule for all five counters with no exception. EXT-17 measured it on a real four-part capture:
parts reading 1, 1, 1, 2, summing to **5**, for a run with **2** segments, while the other four
counters summed correctly. Raised as **E-3**. Subtract one per cut — a cut being a part whose
trailer carries both `end_reason: "size_limit"` and a `continued_in`.

### 7. Start the recorder before the host

The `entity_created` burst that fills the roster is published once, at scenario load. A
recorder attached later sees samples for entities it never saw created and records nothing but
orphans. The capture says which happened — `header.attached_mid_run` and
`trailer.drops.samples_orphaned` — but a campaign that gets the order wrong has collected
nothing.

One consequence that reads like corruption and is not: in a late-attached capture
`entities_added` and `entities_removed` **do not balance**.
