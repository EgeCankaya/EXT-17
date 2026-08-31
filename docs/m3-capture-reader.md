# M3 — capture the run, and read it back

**Date:** 2026-08-31
**Milestone:** M3 ([B] step 3 — *"Capture the run. Subscribe as in EXT-08. Keep the recording
separate from the assertions so a stored run can be re-judged later."*)
**Platform:** N8RO runtime 2.1.328; captures from producer `n8ro-bridge` 0.9.0
**Deliverable:** `n8ro-capture`, a conformant reader for `n8ro-capture/1` that **links nothing**;
its conformance suite; CR-CAP-5's disk ceiling; OQ-6 decided
**Evidence:** `campaigns/m3-oq6/` (three probes), `campaigns/m2-oq1/` (M2's twenty runs, re-read),
`contract/capture-atacama-air-defense-sample.n8rocap.jsonl` (the vendored fixture)

> Every number in this document was measured here. Where it reproduces something from M2 or from
> `contract/PROVENANCE.md` it is stated as a reproduction. Nothing is inherited silently.

---

## 1. What was built

| Component | Owns | Requirement |
|---|---|---|
| `src/common/JsonParse` | A JSON parser. **Order-preserving**, because §8.2's field-order guarantee is checkable only with one. Locale-independent doubles, depth-limited, never throws | CR-CAP-2 |
| `src/capture/Capture` | The model of a capture, and the closed set of named things that can be wrong with one — with two severities, because the format has two | CR-CAP-2, CR-CAP-3 |
| `src/capture/CaptureReader` | The reader. Streaming, one `getline` at a time; retains statistics, never samples; offers a `RecordSink` for a consumer that needs the samples | CR-CAP-2, CR-CAP-4 |
| `src/capture/CaptureSet` | §6.7 stitching: a rotated run is a *set* of files whose segment ordinals restart in every part | CR-CAP-5 |
| `tools/n8ro-capture` | The CLI — `read`, `read-set`, `campaign` — and a golden `--help` | CR-CAP-1, CR-DOC-1 |
| `tests/capture_reader_test` | The conformance suite: 78 checks over four tiers | CR-CAP-2/3/4 |
| `n8ro-campaign` (extended) | The campaign disk ceiling, the pre-flight check, the per-run bound, and reading each capture back as it is produced | CR-CAP-5 |

**`tools/m2-checks/capture_structure.py` is superseded and is deliberately not deleted.**
M2 said to delete it once the reader could answer the same questions, and the reader now does —
`n8ro-capture campaign <dir>` replaces it outright, and tier 4 of the conformance suite re-derives
M2's twenty-run structural findings with it. But `tools/m2-checks/oq1_table.py` imports it, and
that script is the published reproduction command for `docs/m2-oq1.md`'s twenty-run table. Deleting
the module would break the reproduction line of a merged milestone document to tidy up a file
nobody runs. It stays, for that reason and no other. Nothing new should use it.

---

## 2. The boundary, and how it is checked rather than asserted

ADR-2's M3 gate is *"no EXT-08 source has been read, and no EXT-08 identifier appears in EXT-17's
source"*. Three things make that a fact rather than a claim:

**The reader is its own binary, and its build script is the proof.** `tools/n8ro-capture/build.cmd`
has no `/I`, no `/LIBPATH` and no `.lib` on its compile line. It builds with no N8RO install
present and runs with none. Anyone can verify the claim by reading one file.

**The build searches its own sources and fails on a hit.** `n8ro-sim`, `n8ro-core`, `n8ro-bridge`
and `N8RO\include` are searched for across `src/capture/`, `src/common/JsonParse.*` and the CLI. A
match fails the build, which is the only way a gate like this stays true after the milestone that
wrote it.

**And a second search covers CR-CAP-4's fourth criterion** — *"no code path sorts a capture by
`sim_time_s` globally"*. `std::sort`, `std::stable_sort` and `qsort` are searched for across
`src/capture/` and must be absent. They are: the reader is streaming and the file's own record
order is authoritative (§5.2). The only sorting `n8ro-capture` does is of directory entries, in
`main.cpp`, which is not a capture.

The direction that *is* allowed: `n8ro-campaign` links `src/capture/`, since M3, so a run can read
back the capture it just produced. The requirement is that the reader links no SDK, not that
nothing linking the SDK may link the reader.

---

## 3. The reader, and the four decisions inside it that are not obvious

### (a) Two severities, because the format has two

`Reject` means stop and produce nothing. There are exactly three: unreadable, not a capture, and a
`format_version` this reader does not implement. §3 step 2 is explicit — *"reject the file with a
named error … and stop. Do not attempt a partial parse."*

`Defect` means a named finding with a line number, while the records already read stay available.
That is equally the format's instruction, from the other end: §3 step 5 says everything before a
truncation *"is still valid and may be used."*

Collapsing the two would either throw away a usable prefix or keep parsing a file whose version
cannot be interpreted. Both are ways to produce a confidently wrong number.

There are 24 named codes plus `ok`, and the names are interface: the tests assert on them and reports print
them.

### (b) The clock classification is three-valued, and M2 is why

`Running` — §5.1's exact test fired and passed: the maximum number of samples any one
`(entity, occupancy)` carries at a single `sim_time_s` is 1.
`Frozen` — that maximum exceeds 1. The clock was reset; the segment cannot be aligned at all.
`Indeterminate` — **the test could not fire.**

M2 measured segment 1 empty in 15 of 20 runs, and this milestone met the other shape too: run 000
of `m2-oq1` has a segment 1 with 42 samples, one per entity, all at `sim_time_s` 0.0 — enough
samples to look like a segment and not enough for any key to repeat a time. A boolean has to call
both of those "running", which is asserting the result of a test that never ran. Both
`Frozen` and `Indeterminate` are excluded from comparison; only `Running` is comparable, and it is
comparable *because* the test fired.

### (c) The segment list is built from `segment_open`

This is the M2 bug, named in the prompt as the one M3 was positioned to repeat. A list built from
sample records alone loses a segment that was opened and closed with nothing in it, and then
disagrees with `trailer.counts.segments` for a reason that has nothing to do with the file. The
suite has a synthetic capture for exactly this, and tier 4 re-reads all twenty of M2's runs.

### (d) `sim_time_s` is not assumed monotonic — it is checked, because a count is derived from it

§5.1 gives one positive rule: within a segment, sample records are non-decreasing. The reader uses
it to count distinct `sim_time_s` values in constant memory, and therefore **checks it** and
reports `sample_time_decreased` if it fails. A reader that assumes an invariant and computes from
it should say so when the invariant breaks rather than quietly report a smaller number.

The per-key frozen-clock counter is per `(segment, entity, occupancy)` and not per line: 42
entities sharing one `sim_time_s` is an ordinary frame, and a reader counting records per
`sim_time_s` would call every running segment frozen. There is a synthetic test for that too.

---

## 4. The conformance suite — 78 checks, four tiers

`tests\build.cmd` builds and runs it. It links nothing and needs no N8RO install.

### Tier 1 — the vendored fixture, untouched

`contract/` is read-only, so nothing is written into it. This tier is also the `contract/` drift
check the PRD asks be re-run at the start of every milestone (R4).

The fixture parses completely and conformantly, and the reader's own tally agrees with
`trailer.counts` exactly — **7 180 lines: 6 945 samples, 132 adds, 90 removes, 7 verdicts, 2
segments**, which is the figure the PRD states as re-derivable [C3], now derived by the artifact
the requirement is about. Segment 0 scores `max = 1` (running); segment 1 scores `max = 11`
(frozen) and carries the 42 entities re-created at occupancy 2.

It also checks the things the *fixture's own age* makes checkable: it is producer 0.5.0, so
`sample_form`, `limits` and `part` are all absent, and the reader reports each as **absent** —
never as `"predicted"`, never as "unbounded", never as anything but unknown (§6.3a, §6.6).
`runtime_version` is the literal `"unknown"` and `schema_version` is empty, and both are carried
rather than treated as errors.

### Tier 2 — five mutations and one positive one, generated into `build/tests/mutations/`

Copies, edited in the build tree. Nothing mutated is committed, so no mutant can go stale against
a re-pinned fixture, and `contract/` is never touched.

| mutation | named error | what else it asserts |
|---|---|---|
| (a) `format_version` → `n8ro-capture/2` | `unsupported_format_version` | see below |
| (b) last 40 lines removed | `truncated_no_trailer` | the prefix is still counted; no `counts_disagree` is invented, because there is no trailer to disagree with |
| (c) one line's JSON broken | `malformed_line` | it names the line; and the lost record makes the tally disagree with the trailer, which is *also* reported |
| (d) a `fields` key the schema does not declare | `undeclared_field` | it is the **only** finding — a renamed key is not also an order or type error |
| (e) `trailer.counts.samples` off by one | `counts_disagree` | it names both numbers |
| (f) keys from a producer that does not exist yet | **none — conformant** | §13's non-breaking rule, and R4's own mitigation |

**(a) is booby-trapped, and that is the point.** Asserting "it rejected" proves half of CR-CAP-3.
The other half is *no partial parse*, so **line 2 of that mutant is deliberate garbage**. If the
reader ever reaches it, the test fails with a *different* error. The assertion is therefore that
`tallies.lines == 1`, that the diagnostic list is **empty** — no `malformed_line` was ever
reported — and that nothing was produced: no segments, no counts, no trailer. "No partial parse"
is proved by construction rather than asserted.

**(f) is the one that matters most for the long run.** `contract/` has drifted twice in two
producer releases and both times it was an added key. The version has held across three releases
*because* readers ignore unknown keys. So a header, a sample, a `segment_open` and a trailer are
each given a key this reader has never heard of, and the file must still read identically —
6 945 samples, 2 segments, zero findings. R4 asks for this test in as many words.

### Tier 3 — 17 synthetic micro-captures, 26 checks, one rule each

Written inline in the test, because a capture small enough to read in the test that asserts on it
is worth more than one hidden in a fixture file. They cover what the fixture cannot reach:

a valid `header`+`trailer` empty capture with zero segments (§7); **an empty segment** and its
`indeterminate` classification; the frozen-clock test firing, and *not* firing on 42 entities in
one frame; a name re-created at occupancy 2 treated as a distinct entity, and a sample under a
closed occupancy named `sample_after_remove` (§8.1's one assertable invariant); an *unheard-of*
`entity_remove.reason` accepted because that vocabulary is **open** (§9), against an unexpected
`segment_close.reason` and `end_reason` both named as violations because those vocabularies are
**closed** (§7, §11, §13); `5` parsed as a double from the schema and `"nan"`/`"inf"`/`"-inf"`
accepted in a double field only; a fractional value in an `int` field and a short array both
*reported and tolerated*, with all four samples still counted; declared-and-never-sent
distinguished from not-declared (§8.2); fields out of declaration order named; a ninth record type
named and skipped rather than fatal; a sample outside an open segment; a sample going backwards
in `sim_time_s`; a reused segment ordinal; and a first record that is not a header.

**Tier 3b covers rotation** — two hand-written parts whose segment ordinals both restart at 0,
stitched into `(0,0)` and `(1,0)`; an unrotated capture read as a one-part set, which is the
correct answer; and a `continued_in` on a part whose `end_reason` is not `size_limit`, named
`part_link_broken`.

### Tier 4 — the real producer-0.9.0 captures

R4's mitigation says it directly: *"test against a capture written by the pinned producer, not
only against the vendored fixture"*. The fixture is 0.5.0 and predates every header key added
since. So when `campaigns/m2-oq1/` is present the suite reads **all twenty** of M2's captures —
486 359 759 bytes — and asserts:

- every one is **conformant**;
- `limits` and `sample_form: "published"` are present and read in all twenty;
- **the roster lifecycle is identical in every run: 89 adds, 47 removes**, splitting **47 at
  occupancy 1 and 42 at occupancy 2** — M2's sharpest finding, re-derived by the real reader
  rather than by the throwaway script that first found it;
- segment 0 is `running` in all twenty by the format's exact test;
- segment 1 is **never** `running` — it is `frozen` or `indeterminate`, and is excluded either way.

It is **skipped with a printed message** when the directory is absent — it is untracked and
595 MB — never silently. A tier that disappears quietly is a tier nobody notices is gone.

**Speed, since it matters for whether this gets run:** 24 MB and 50 573 lines in 0.24 s; all
twenty captures in 4.7 s.

---

## 5. Three defects M3 found in this project's own code, all by exercising rotation

The prompt asked that `stop` not win by looking simpler. Running the probes cost three bugs found,
which is a better argument for having run them than anything written here.

### (a) A relative `--out-dir` silently broke the recorder

The first rotation probe was launched with `--out-dir campaigns\m3-oq6\rotate`. Every child
process is started in its own working directory, so the recorder received a *relative*
`--out-dir`, resolved it against the run directory it had just been placed in, found nothing, and
refused — correctly, and with a clear message, in a file nobody was reading.

M2 never saw it because M2 was invoked with an absolute path. `--out-dir` is now resolved with
`GetFullPathName` before anything is handed to a child.

### (b) A run that recorded nothing was reported `completed`

That is the serious one. The run above started no recorder, executed to frame 1200, and
`n8ro-campaign` classified it **`completed`** — logging *"no capture file in …"* on the way past.

A run reported as completed is a run M6 will later judge, and it would be judging nothing. Tenet 1
is that a wrong number is worse than no number; CR-EX-5 is that infrastructure is never a test
result. A recorder that was asked to record and produced no file is infrastructure. It is now an
`infrastructure_error` with a named stage — and only where the run had otherwise succeeded, since
a run that already failed keeps the fault it actually had.

### (c) A capture covering a third of its run was indistinguishable from one covering all of it

The `stop` probe recorded to `sim_time_s` 19.5 of 60.0 — a complete, valid, **conformant** capture
of the first third of a run, exactly as §6.6 promises. The run reported `completed`, and nothing
outside the file said so.

`n8ro-campaign` now **reads back the capture it just produced**, with this milestone's own reader,
and writes what it found into the run record:

```json
"capture": {
  "parts": ["capture-atacama-air-defense-000.n8rocap.jsonl"],
  "end_reason": "size_limit",
  "covers_whole_run": false,
  "conformant": true,
  "samples": 16626,
  "segment_keys": 1,
  "run_segments": 1
}
```

It costs a quarter of a second per run against a 61-second run, and it buys two answers nothing
else could give: whether the file just written is well formed, and whether it covers the run.

*A fourth was found in M3's own new code and is noted for honesty:* `directorySizeBytes` shipped
its first draft with a mangled path separator and returned 0, so the mid-campaign ceiling check
silently never fired — a campaign ran four runs and 30.8 MB against a 10 MB ceiling without
stopping. It was found by testing the ceiling rather than by reading the code, which is the
argument for having tested it. Both halves of CR-CAP-5's check are now verified against real runs
(see `m3-oq6.md`).

---

## 6. One imprecision found in `contract/`, and it goes back rather than being worked around

§6.7 says of a trailer's `counts`: *"The run's totals are the sum across parts."*

For `samples`, `entities_added`, `entities_removed` and `verdicts` that is true. For `segments` it
is **false**: a segment cut by a rotation is closed in one part and opened in the next, so the sum
counts it twice. Measured on the four-part probe — **5 summed, 2 in the run**.

`contract/` is read-only and a defect in it goes back to EXT-08. It is raised as **E-3** in
`docs/escalations.md`. Meanwhile the reader implements what §6.7 says, reports what is true
beside it, and names the gap in its own output — which is the behaviour the rule asks for: not
working around it, and not propagating it either.

---

## 7. What M3 did *not* do

- **No determinism comparison.** That is M4, gated on OQ-2, still out. What M3 owes M4 is a reader
  that classifies a segment's clock exactly, keys identity on `(entity, occupancy)`, never sorts
  globally, and offers a `RecordSink` so the comparison can stream sample values without the
  reader retaining them. All four exist and are tested.
- **No parameterisation axis chosen.** That is OQ-4 at M5.
- **No assertions and no report.** That is M6. CR-CAP-1 is therefore **half met and said so**: the
  reader takes a *stored* capture and starts no host and makes no bus subscription — there is no
  code in `n8ro-capture` that could — but the re-judge mode that produces verdicts needs
  conditions to judge, and arrives at M6 with CR-AS-3. The milestone does not claim CR-CAP-1
  complete.
- **It did not confirm the invocation.** OQ-3 is still drafted and unsent.

## 8. One contract question M3 raises for the PRD

M2's axis spike measured all three of [B]'s parameterisation axes reachable over the bus with no
authoring. The PRD still rates **R9** *"Unknown — not yet investigated"* and still calls the
catalogue axis OQ-4's *"fallback … because it needs no authoring at all"*. Both statements are now
false, and OQ-6 is now decided as well.

**Nothing in `docs/prd.md` has been changed.** A revision 3 folding in R9's re-rating, OQ-4's
framing and OQ-6's resolution is drafted for approval rather than applied — silently revising the
contract to match what was built is the failure this project's own provenance discipline exists to
prevent.
