# M4 — Prove determinism

**Date:** 2026-08-31
**Milestone:** M4 ([B] step 4 — *"Prove determinism — the same-configuration self-test above.
**Do not build further until it passes.**"*)
**Platform:** N8RO runtime 2.1.328; captures from producer `n8ro-bridge` 0.9.0
**Deliverable:** the content comparison and the byte comparison, both run and both reported;
the self-test at the start of every campaign; CR-DET-2's four hazards closed twice over
**Evidence:** `campaigns/m4-gate/` (a campaign whose gate passed), `campaigns/m4-bytes/` (the
same configuration under the byte gate, which stopped it), `campaigns/m4-overload/` (a capture
with non-zero `samples_not_recorded`), `campaigns/m4-frozen/` (the segment-0 freeze), and all
190 pairs of `campaigns/m2-oq1/`

> Every number in this document was measured here, by this project's own binaries, on this
> machine. Where it reproduces something inherited it is stated as a reproduction and both
> numbers are given.

---

## 1. The headline, and what it does not say

**On this machine, two runs of one configuration agree on every sample present in both at the
same simulation instant, and their captures are never byte-identical.**

Measured two ways.

**Fresh runs, executed by the self-test itself** (`campaigns/m4-gate/selftest/`):

| | |
|---|---|
| samples compared | **50 400** |
| agreeing | **50 400** |
| differing | **0** |
| present in one run only | 0 and 70 |
| coverage | 100.0000% of the smaller run's comparable samples |
| byte comparison | **DIFFER** — 24 310 961 against 24 349 991 bytes, first difference at byte 5 034, line 45 |
| headers | byte-identical; `platform.model_path` excluded per §14 and identical anyway |
| result equality | both runs `completed`; verdicts 0 = 0, and reported as **vacuous** rather than as a pass |

**And, far more strongly, over every pair of M2's twenty runs** — 190 pairs, run through
`n8ro-compare`, the binary that decides the gate:

| | |
|---|---:|
| pairs compared | **190** |
| samples compared | **9 573 667** |
| samples agreeing | **9 573 667** |
| samples differing | **0** |
| pairs with any differing value | **0** |
| pairs passing the content gate | **190 of 190** |
| pairs byte-identical | **0 of 190** |
| smallest intersection in any pair | 50 361 samples |
| worst coverage | 99.8513% |
| best coverage | 100.0000% |

Reproduce with:

```
for each pair (a, b) of campaigns\m2-oq1\runs\*\*.n8rocap.jsonl:
    n8ro-compare <a> <b> --outcome-a completed --outcome-b completed
```

**The inherited figure was 50 358 of 50 358 on one pair** [C1]. This project's own reproduction
agrees with it in kind and exceeds it by a factor of 190 in extent. Nothing here disagrees with
the number that justified ADR-1.

**What the headline does not say, and the report never says either:** that the two runs are
identical. They are not, and they are not expected to be. 0.15% of samples in the pair above
were present in one capture and not the other, and the byte comparison fails on every pair
measured. Every one of those is the publication schedule, which §14 says this platform does not
repeat — not the simulation, which it does.

---

## 2. OQ-2 is still unanswered, and M4 is built so that either answer costs nothing

**Checked at the start of this milestone:** EXT-08's `docs/escalations.md` E-1 — the upstream
half of the same question — still reads *"raised with the brief's author 2026-08-31; awaiting a
reply"*. No ruling exists. This project's E-2 is unchanged.

So M4 states its own status in three claims, and only the first two are its to make:

| | |
|---|---|
| **Deliverable under either answer**, because it is a measurement rather than a verdict | *"Two runs of one configuration agree on every sample present in both at the same simulation instant, and their captures are never byte-identical."* §1 above |
| **Deliverable with the deviation named** | *"CR-DET-1's content gate passes."* ADR-1's decision, marked as a deviation everywhere it appears |
| **NOT deliverable, and not claimed anywhere** | *"[B]'s step-4 gate has passed."* Under the byte reading it has not, and [B] says stop |

**M4 therefore closes as conditionally passed, with the condition named** — and the way that is
made honest rather than a hedge is that **the byte reading is built as a real, selectable gate
that correctly halts the campaign**, not as a printed observation.

### Measured, on one pair of fresh runs, both ways

`campaigns/m4-bytes/` is the same configuration as `campaigns/m4-gate/`, run under
`--gate-basis bytes`:

```
  content comparison    PASS
    compared            50361 sample(s)
    agree               50361
    differ              0
    coverage            99.9147% ... floor 99.0000%
  byte comparison       DIFFER
    sizes               24311271 and 24301091 byte(s)
    first difference    byte 238513, line 550
  GATE                  FAIL   on the bytes basis
```

- exit code **3**
- `campaigns/m4-bytes/runs/` **does not exist** — not one campaign run was attempted
- the content comparison **still ran and still reported PASS**, in the same file

Under the default `--gate-basis content` the identical machinery reports `GATE PASS`, the
campaign runs, and **the byte comparison still ran and still reported DIFFER**. The two
invocations differ in which comparison decided and in nothing else — asserted by test, in
`determinism_test.cpp`, on the same pair of captures.

**A ruling on OQ-2 therefore changes a default and no code.** If the ruling is "bytes", the
project stops at M4 by [B]'s own instruction and the command that demonstrates it already
exists and has been run. If the ruling is "content", the default is already right.

### And the report says it in words, every time

Every self-test, in the log, in `self-test.json` and in `campaign.json`:

```
  gate basis            content   (ADR-1: the content basis is THIS PROJECT'S decision, not the client's)
  OQ-2                  UNANSWERED. Whether the gate is keyed on content or on bytes is out with the
                        owner of the brief and has not been ruled on. ...
  GATE                  PASS   on the content basis
                        This does NOT discharge [B]'s acceptance criterion 2 as written. It
                        discharges it under the content reading, which is this project's named
                        deviation (ADR-1) and is unruled (OQ-2).
```

`self-test.json` carries the same thing machine-readably, as `gate.basis_is_a_named_deviation`
and `gate.oq2_ruling: "unanswered"`, so a report generated from it at M6 cannot lose it.

---

## 3. The hardest design question: a sample in one run and not the other

M2 measured seventeen distinct sample counts across twenty runs of one configuration, a 0.38%
spread, up to five whole frames missing — with all nine drop and bus counters reading zero. So
the two captures being compared **do not have the same number of samples, ever**, and what to
do about that decides whether the gate means anything.

**It is never a difference and never a failure.** It is `only_in_A` / `only_in_B`, reported
inside the verdict line rather than as a footnote, and the verdict is phrased as what was
actually established:

> every sample present in **both** runs at the same simulation instant agrees. This is not a
> claim that the two captures are identical: 0 sample(s) were present only in 000 and 70 only
> in 001, which is the publication schedule and not the simulation (§14).

Concluding "the runs are identical" from an intersection would be concluding from absence, which
tenet 3 forbids. Declining to conclude it is not the same thing, and the wording is the
difference.

**But an intersection is bounded, because "they all agreed" over three samples is a wrong
number.** The comparison carries a **coverage floor**: the intersection must reach 99% of the
smaller run's comparable samples, or the verdict is `indeterminate` — not `pass`, and not
`fail`. Unmatched samples are evidence about the publication schedule and may not fail a gate;
a collapsed intersection may not silently pass one either (tenet 1).

**The floor is measured, not chosen.** Over the 190 pairs the worst coverage was 99.8513%, so
99% sits about seven times the worst observed shortfall clear of anything this machine has
produced. It is a guard against a collapsed intersection, not a tolerance on the platform's
behaviour, and the report prints both the percentage and the two counts it came from so it is
checkable by hand.

### Three verdicts, and the third is the one that matters

| verdict | when | what the campaign does |
|---|---|---|
| `pass` | every compared sample agreed, and enough were compared | runs |
| `fail` | a compared sample disagreed, **or** an `(entity, occupancy)` appears in one run and not the other | stops, exit 3 |
| `indeterminate` | the comparison ran and cannot support a verdict — the intersection was below the floor, or nothing was compared | stops, exit 3 |

A key present in one run and absent from the other is a **fail**, not an unmatched-sample count:
an entity that existed in one run and not the other is a difference in the roster, not in which
frames were published. All twenty of M2's runs carry exactly 47 keys in segment 0, so this has
never fired on real data; it is tested synthetically, and it is the reason
`occupancy` is part of the key rather than a field — two tenures of one name are two entities
(§8.1), and a comparison keyed on the name alone would call them equal.

---

## 4. `indeterminate` is not `running`, and it earned its exclusion immediately

CR-DET-1 says *"running segments only"*. M3 built the three-valued clock class for this. M4 is
where it paid.

**Across M2's twenty runs, segment 1 carries samples in only five** — at 42, 16, 30, 37 and 14
entity keys. It is `indeterminate` in fifteen and never `running` in any. Comparing run 000
against run 001 on segment 1 would align a 42-key teardown against a 16-key one and report
**26 samples present in one run only**, every one an artifact of comparing two teardowns that
were never the same event.

The report says which segments were excluded and why, with their sample counts, so the exclusion
is visible rather than silent:

```
    compared            (part 0, segment 0)  running in both, 47 entity key(s)
    excluded            (part 0, segment 1)  clock is indeterminate in 000 and indeterminate in 001;
                                             only a running segment can be aligned across two runs
                          42 and 16 sample(s) in it were not compared
```

And **having no comparable segment left is not a pass.** Two identical one-frame runs — where
every key has exactly one sample and the format's test therefore cannot fire — are `refused`,
not passed. There is a test for it.

---

## 5. A new platform observation: segment 0 can be `frozen`, and it is not a reset clock

**This is M4's finding, and it was not predicted by anything inherited.**

`campaigns/m4-frozen/selftest/runs/000` — an ordinary 1200-frame run, no parameter manipulation
— produced a capture whose **segment 0 classifies as `frozen`** by the format's exact §5.1 test.
Until this milestone, segment 0 was `running` in every capture this project had ever read.

What is actually in the file:

- **13 entities published their `sim_time_s 0` sample twice**, at lines 45–57 and again at lines
  87–99, all at `phase: "uninitialized"` — the start-up roster burst, published twice for part of
  the roster.
- **Every duplicate carries byte-identical values.** Not one of the thirteen pairs differs.
- The segment's clock plainly did **not** reset: **1 200 distinct `sim_time_s`** spanning
  `0` to `59.999999999998728`.

§5.1's test is *"more than one sample for one `(entity, occupancy)` at one `sim_time_s`"*, and
§5.1 reads a positive result as the engine's stop path having reset the clock. Here the test is
right and the reading is not: this is a **duplicated publication**, which is a different
phenomenon with a different consequence.

### Frequency, measured over every capture this project holds

Every `.n8rocap.jsonl` under `campaigns/` — **42 files**:

| | |
|---|---:|
| captures with a **frozen** segment 0 | **2 of 42** |
| …of which every duplicate carried identical values | **2 of 2** |
| `m2-axis/p2-update-post` | 20 duplicated instants, 20 identical — a run that **deliberately published an entity update after start**, so it has a candidate cause |
| `m4-frozen/selftest/000` | 13 duplicated instants, 13 identical — an **ordinary run**, nothing published into it |
| ordinary unmanipulated full runs measured | **1 of 27** — M2's twenty, M4's six, M3's overload probe |

So roughly **3.7% of runs**, and therefore **about 7% of pairs**, on this machine.

### What M4 does about it, and what it does not

**It does not change the exclusion rule.** `frozen` is excluded, exactly as CR-DET-1 and §14
require. Relaxing the format's test to make a run comparable would be working around
`contract/`, which this project does not do, and it would be doing so on the gate that every
other result rests on. The comparison refuses, the campaign stops with exit 3, and an operator
looks.

**It does make the refusal attributable**, which is CR-DET-3's whole subject — *"you must be
able to tell which"*. The comparison now counts, per segment and per run, how many instants were
published more than once and how many of those repeats carried identical values, and says which
shape it found:

```
  REFUSED               no_comparable_segment
                        ... (part 0, segment 0): clock is frozen in 000 and running in 001;
                        only a running segment can be aligned across two runs. In 000, 13
                        instant(s) were published more than once for one (entity, occupancy),
                        13 of them carrying IDENTICAL values — so this is a DUPLICATED
                        PUBLICATION rather than a reset clock, and the segment is excluded
                        anyway because that is what the format's test says to do (§5.1) and
                        working around it is not this project's to do.
```

The counts are in `self-test.json` per segment as `duplicated_instants_*` and
`duplicated_identical_*`. There are tests for both shapes.

**And it goes back to EXT-08 as E-4** rather than being absorbed here. §5.1 presents one test as
detecting one phenomenon; two have now been measured through it. That is the same class of
finding as E-3 and it is handled the same way.

**The operational consequence is stated rather than hidden:** on this machine a campaign has
roughly a **1-in-14 chance of stopping at its own self-test** for a reason that is not a
determinism failure. There is no retry, deliberately — a harness that re-rolls its gate until it
gets an answer it likes has no gate. This belongs in CR-DOC-2's unexplained-observations section
at M7 and is recorded here for it.

---

## 6. `samples_not_recorded` was never zero-tested until now

CR-DET-1 requires that *"a capture with non-zero `trailer.drops.samples_not_recorded` is not
compared; it is already an incomplete record of its run"*. That counter read **0 in all twenty**
of M2's runs and in every M3 probe, so the rule had never fired. A rule that has never fired is
a rule nobody has tested.

`n8ro-campaign` now passes `--queue-size` through to the recorder, and
`campaigns/m4-overload/runs/000` is a real 1200-frame run recorded at `--queue-size 4`:

```
  counts        trailer segments 2 samples 47702 adds 89 removes 47 verdicts 0
  drops         samples_not_recorded 2755
  CONFORMS      47844 lines, 22982093 bytes
```

**2 755 samples not recorded** — non-zero for the first time in this project. The capture is
still complete, valid and conformant, exactly as §14 promises. And the comparison refuses it:

```
  REFUSED               samples_not_recorded
                        capture-...-000.n8rocap.jsonl: trailer.drops.samples_not_recorded is 2755.
                        It is already an incomplete record of its run; the self-test says so
                        rather than diffing it (CR-DET-1).
```

**Absent is refused too, and that is not the same rule.** §11 requires a missing counter to be
read as *unknown* rather than as zero, and tenet 3 is that absence is not evidence. A capture
whose completeness cannot be established is not one to diff, so it gets its own named refusal,
`samples_not_recorded_unknown`, distinct from the non-zero case.

*One thing worth recording for whoever reads `header.subscription` later:* the recorder's
`--queue-size` bounds its **handler-to-writer** queue and is **not** what
`header.subscription.queue_size` reports — that stayed at 1024 across the default runs and the
overload run alike. So the like-for-like subscription check cannot see a queue-size difference
between two captures. It does not need to: the drop counter does, and it is checked first.

---

## 7. The preconditions, each refused by name

A comparison is only meaningful between two captures that are comparable, and §6.4 says to check
before concluding rather than to be mystified afterwards. Twelve named refusals, every one
tested, and eight of them exercised against real captures from this project's own campaigns:

| refusal | means | exercised against |
|---|---|---|
| `capture_rejected` | the reader would not read it at all | synthetic |
| `capture_not_conformant` | it read, and the reader found faults in it | synthetic |
| `capture_does_not_cover_whole_run` | `end_reason` is `size_limit` with no continuation — it covers **part** of its run | **real**: `m3-oq6/stop/000`, which recorded to `sim_time_s` 19.5 of 60.0 |
| `samples_not_recorded` | non-zero | **real**: `m4-overload/000`, 2 755 |
| `samples_not_recorded_unknown` | absent, which is unknown and not zero | synthetic |
| `producer_mismatch` | two producer versions are not like for like (§6.4) | synthetic |
| `subscription_mismatch` | a different subscription records a different stream | synthetic |
| `limits_mismatch` | two runs bounded differently cut in different places (§14) | **real**: `m3-oq6/rotate/000` against an unbounded run |
| `schema_mismatch` | different schema identities: the values are not the same values | synthetic |
| `scenario_mismatch` | | synthetic |
| `segment_set_mismatch` | the two runs do not have the same segment structure | synthetic |
| `no_comparable_segment` | nothing is running in both — see §4 and §5 | **real**: `m4-frozen`'s pair |

`covers_whole_run` is a precondition and not a footnote, which is what OQ-6 committed M4 to.
Two runs are comparable because both were bounded at frame 1200, not because their captures
ended alike.

---

## 8. CR-DET-2: four hazards, closed twice each, and neither check subsumes the other

[B] names three — *"a timestamp in the compared output, an unordered container iterated, a value
read from a clock"* — and this project adds a fourth, the comma-decimal locale it actually has.

**By design, first.** The comparison never converts a number for comparison. Two runs are
aligned by matching the **verbatim text** of `sim_time_s` straight out of the file, and values
are compared through a digest over a type-tagged encoding that feeds each number its original
text. §8.3 makes doubles shortest round-trip and uniquely determined, so equal text and equal
double are the same relation — which means the verdict never depends on a numeric conversion at
all. Ordering the merge uses the double the reader parsed, and the reader already does that
through the C locale explicitly rather than hopefully.

**By build-time search.** `tools/n8ro-compare/build.cmd` searches the comparison's own sources
and fails the build on a hit — `chrono`, `GetTickCount`, `GetSystemTime`,
`QueryPerformanceCounter`, `time(`, `localtime`, `gmtime`, `strftime`, `GetLocalTime`;
`unordered_map`, `unordered_set` and their multi- variants; `sprintf`, `snprintf`,
`ostringstream`, `strtod`, `atof`, `setprecision`. It carries the same two searches
`n8ro-capture` does, for the SDK's and the producer's names and for any sort of a capture.

**By behaviour.** `tests/determinism_test.cpp`:

| hazard | the test |
|---|---|
| a clock read, a timestamp in compared output | the same comparison run twice produces **byte-identical report text**. Any wall-clock value anywhere on the path breaks this |
| an unordered container iterated | two captures whose entities appear in the **opposite order** in the file produce byte-identical report text — and a run ordered one way still compares equal against the same run ordered the other |
| the locale | the whole report is regenerated under **`German_Germany.1252`** and must be byte-identical to the C-locale one, and still contain `100.0000%` rather than `100,0000%`. The verdict is re-derived under it too |

Neither check subsumes the other. The search catches a reintroduction on a path no test happens
to exercise; the test catches one the search does not know the spelling of.

**Run identifiers in compared output are ordinals, never timestamps** — unchanged since M2, and
the self-test's two runs are `000` and `001`.

---

## 9. What was built

| Component | Owns | Requirement |
|---|---|---|
| `src/compare/Compare` | The comparison: alignment, the digest, the coverage floor, the twelve refusals, the byte comparison, and the report. **Links nothing** | CR-DET-1, CR-DET-3 |
| `src/run/SelfTest` | Two runs, compared, with `infrastructure_error` kept distinct from `failed` | CR-DET-1, CR-EX-5 |
| `tools/n8ro-compare` | The CLI, its golden `--help`, and the build script that is the boundary's and CR-DET-2's proof | CR-DET-2, ADR-2 |
| `tests/determinism_test` | 75 checks | CR-DET-1/2/3 |
| `n8ro-campaign` (extended) | The self-test at the start of `repeat`; the `self-test` command; `--gate-basis`, `--coverage-floor`, `--queue-size`; exit 3; the self-test in `campaign.json` | CR-DET-1 |
| `src/capture/CaptureSet` (one line) | A `RecordSink` passthrough on `readSet`, so a rotated set streams to a consumer | — |

**Two additive changes to M3's tested code, and no behavioural ones.** `readSet` gained an
optional sink parameter defaulting to `nullptr` — without it the comparison would read every
capture twice, once for its structure and once for its samples. `RunConfig` gained
`recorderQueueSize`, defaulting to 0, which leaves the recorder's own default in place. The
reader's 78 conformance checks are unchanged and still pass.

### Memory, because it was a stated constraint

The comparison holds a **128-bit digest, the verbatim `sim_time_s` text and a line number per
sample** — about 3 MB per run against the 24 MB of retaining the samples themselves. It never
asks the reader to retain anything; it streams through the `RecordSink` M3 built for it.

CR-DET-3's *"first differing record"* needs the record's text, and it is fetched by **re-reading
both files at the two line numbers**, only when there is a difference — which on this platform is
never, so the normal cost of that detail is zero.

### Where the self-test runs, and what it costs

`<out-dir>/selftest/runs/000` and `001`. **They are not campaign runs**: `campaign.json` reports
`attempted: 2` for a `--count 2` campaign, not 4. The disk pre-flight counts them —
`projecting 121920000 bytes for 4 run(s) ... (including the self-test's 2 runs)` — because a
projection that ignored them would be projecting a campaign nobody runs.

Cost: **two runs, about 2 minutes and 60 MB**, on every campaign. On a twenty-run campaign that
is 10%.

### And what it does when it fails

**Nothing else runs.** `<out-dir>/runs/` is not created. `self-test.json` is written, the reason
is logged, and the exit code is **3** — a new code, added deliberately, because collapsing
[B]'s hard gate into exit 1 alongside an ordinary failing run is the collapse tenet 2 forbids.
Verified: `campaigns/m4-bytes/` has a `selftest/` directory and no `runs/` directory.

`failed` and `infrastructure_error` are kept apart inside that. A self-test run that did not
complete, or a comparison that was refused, has established **nothing** about determinism — it
is the harness, not a determinism failure, and reporting it as one would send someone to
investigate the wrong system (CR-EX-5). Both exit 3; `self-test.json`'s `outcome` says which.

**There is no `--skip-self-test`.** CR-DET-1 asks for a self-test *"you run every time, not
something you checked once"*, and a skip flag is how that becomes something everybody skips. The
`self-test` command exists for running the gate alone; `repeat` does not consult it.

---

## 10. The `contract/` pin, re-checked (R4, R11)

**The vendored specification and fixture are current.** `contract/capture-format-v1.md` and
`contract/capture-atacama-air-defense-sample.n8rocap.jsonl` are byte-identical to EXT-08 `main`
at **`eb13485`**.

**The pin string is stale and its content is not.** `PROVENANCE.md` says `78fd4ef`; `main` has
since moved two commits (`2e26e3a`, "Correct two stale test counts", and its merge) and neither
touched a vendored artifact. `PROVENANCE.md` justifies pinning the branch head on the grounds
that it makes *"is this current?"* one comparison against `main` — and that comparison now
answers "no" while the correct answer is "yes". `contract/` is read-only, so this is recorded
here rather than edited there. It is a note, not a defect: nothing a reader does depends on it.

**One thing the pin check cannot verify.** `contract/condition-file-schema.md` does not exist
under that name in EXT-08 at any commit — it is a digest written for EXT-17, not a verbatim
vendored file, though `PROVENANCE.md`'s table lists it beside two files that are. It is
reference-only and belongs to M6. Recorded so that a future pin check does not silently imply it
checked something it could not.

---

## 11. What M4 did *not* do

- **It did not close OQ-2.** It cannot; the implementer must not. What it did is make the
  question cost nothing to answer either way, and reproduce the measurement behind it 190 pairs
  deep on this machine.
- **It did not claim [B]'s acceptance criterion 2.** Under the byte reading step 4 fails, and
  the report says so on every run.
- **It did not widen ADR-1's scope.** [B] claims the guarantee holds *"on every run and every
  machine"*. **This project measures one machine.** Nothing here tests cross-machine
  reproducibility and no claim about it is made.
- **It did not choose a parameterisation axis** (OQ-4, M5), **judge anything** (M6), or **re-judge
  a stored run** (CR-CAP-1's second half, M6 with CR-AS-3).
- **It did not work around `contract/`.** §5.1's test is implemented as written; the finding in
  §5 above is reported and escalated.

## 12. What this raised for the PRD — now applied as revision 4

This section was written before the revision and is kept as the record of what was proposed, with
the outcome marked against each item. **PRD revision 4 has since been applied**, on request, and
carries all five.

- ✅ **A new risk — now R12.** Applied: a run's segment 0 can be `frozen` by a
  duplicated publication rather than a reset clock, at a measured ~3.7% of runs, which makes a
  campaign's own gate refuse at ~7% of attempts. R8 is about a determinism leak *in our*
  comparison path; this is neither ours nor a determinism failure, and it has no row.
- ✅ **R11 extended, and re-rated Low → Medium.** Applied: E-4 is a second instance of an imprecision rather than a staleness in
  `contract/` — this time in what a test is said to detect rather than in how a count is summed.
- ✅ **CR-DET-1's acceptance criteria, one of which rev 3 could not state as measured:** the exclusion for non-zero `samples_not_recorded` has been exercised against a real
  capture rather than asserted.
- ✅ **H1 validated.** Applied — 190 pairs, 9 573 667 samples, zero differing — *and its signal
  qualified*: "repeatedly, not once" is met, and the gate additionally refuses on a small
  fraction of pairs for a reason that is not a determinism failure.
- ✅ **The M4 milestone row** now says what M4 can honestly claim (§2 above)
  rather than "CR-DET-1, CR-DET-2, CR-DET-3" unqualified.

**And two things this section did not anticipate, both applied at rev 4:** OQ-2's decision target
of *"before M4 opens"* was **missed**, which is recorded as a miss and re-targeted to before M7's
documentation rather than quietly re-dated; and a success-metric row was added for the refusal
rate in §5, because a cost that has no row is a cost nobody tracks.

## 13. Where M4's findings are indexed

`docs/findings.md`, new after this milestone: one table over every issue this project has found
across M1–M4, because they were recorded correctly and in five different places. M4's own rows
are **F-12** (the `--queue-size` distinction), **F-13** (the frozen segment), **F-16** (E-4),
**F-18** (the stale pin string) and **F-19** (the unverifiable condition-file schema).
