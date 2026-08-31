# M6 — Assertions, reporting, and the ugly reality

**Date:** 2026-09-01
**Milestone:** M6 ([B] steps 6, 7 and 8 — *"decide pass or fail from what the run published"*,
*"reporting — a result per run, a summary across the campaign, and enough detail on a failure
that someone can go and look"*, and *"handle the ugly reality"*)
**Platform:** N8RO runtime 2.1.328; captures from producer `n8ro-bridge` 0.9.0
**Deliverable:** conditions declared outside the code, three-valued verdicts that locate their
own evidence, per-run records and a campaign summary, a re-judge mode, the four faults injected,
and twenty runs unattended
**Decision:** OQ-5, resolved by trying rather than by reading — `docs/m6-oq5.md`
**Evidence:** `campaigns/m6-campaign/` (the committed twenty-run campaign),
`campaigns/m6-gate-refused/` (its first execution, kept — see §4),
`campaigns/m6-faults/` (the four injections)

> Every number in this document was measured here, by this project's own binaries, on this
> machine.

---

## 1. What was built

| Component | Owns | Requirement |
|---|---|---|
| `src/assert/Conditions` | The condition model and its loader. **Links nothing** | CR-AS-1, CR-AS-3 |
| `src/assert/Geodesy` | The distance and region arithmetic `contract/` does not carry (E-5) | CR-AS-2, CR-AS-3 |
| `src/assert/Judge` | The evaluator, the three-valued verdict, and CR-AS-4's bounds | CR-AS-2, CR-AS-4 |
| `tools/n8ro-judge` | The re-judge mode, and `--verify` | CR-CAP-1 |
| `n8ro-campaign` (extended) | `--conditions`, the live judgement, the four outcomes, the verdict sweep table, `--inject-fault` | CR-EX-5, CR-EX-6, CR-EX-7, CR-REP-1..3 |
| `src/compare` (repaired) | A diff that prints an array value instead of nothing (F-31) | CR-DET-3, CR-REP-4 |
| `tests/assertion_test` | 166 checks, run twice under two locales | CR-AS-1..4, CR-CAP-1 |
| `n8ro-compare --changed-input` | The half of the diff that had no implementation (**F-33**) | CR-REP-4 |
| `examples/atacama-raid.conditions.json` | The committed conditions | CR-AS-3, CR-DOC-2 |
| `examples/atacama-raid-speed-20.json` | The committed twenty-run campaign | CR-EX-7, CR-DOC-2 |

**One of those rows is a requirement gap rather than a new feature.** **F-33**: [B] asks for two
diffing questions — *"run the same configuration twice and show that the results are identical;
change one input and show exactly where the two runs diverged"* — and only the first had been
built. `n8ro-compare` would compare any two captures, but it framed every answer as a **gate**,
which is wrong in both directions for the second question. `--changed-input` is the same
machinery with the right framing: no gate line, no pass/fail word, the first divergence named by
segment, `(entity, occupancy)`, `sim_time_s` and field — and **agreement flagged loudly**, because
between two different inputs it means the input did not take effect. Found by auditing the brief
against the built thing, not by a test failing.

**`src/assert/` links nothing, and that is the fourth time this project has said so.** Not
EXT-08, not the N8RO SDK, not a third-party JSON library. `tools/n8ro-judge/build.cmd` is where
it is visible in one file, and it carries two searches beyond the ones `n8ro-compare` has: one
that fails the build if the assertion path ever names a process, a bus or the control path, and
one for a global sort. The first is CR-CAP-1's third acceptance criterion — *"nothing in the
assertion path can start a host, load a scenario, or write into a capture"* — turned from a
promise into a property.

**Both searches fired during development, on this file's own prose**, matching the words
*"subscribe"* and *"snprintf"* inside comments explaining that the code does neither. That is
worth recording rather than quietly fixing: a check that fires on its own documentation is a
check somebody eventually switches off. The tokens are now call- and include-shaped, and the
comments that tripped them say why they name no forbidden spelling.

---

## 2. CR-CAP-1, met by construction and then checked anyway

**There is one evaluator and one kind of input: a stored capture.** The live campaign judges the
capture it has just written and read back; `n8ro-judge` judges the same file later. There is
deliberately no second, "live" code path — a verdict produced while a host was running would be
a verdict a re-judgement could disagree with.

So CR-CAP-1's second acceptance criterion, *"verdicts produced by re-judging a stored capture
are identical to those produced during the live run"*, is **structural**. And it is demonstrated
anyway, because a structural argument nobody checked is how a structural argument stops being
true:

```
  000  fail   satisfied 3  violated 3  indeterminate 1   (met 1, not met 5)
    verify: 7 verdict(s) byte-identical to the live run's verdicts.jsonl
  001  fail   satisfied 3  violated 3  indeterminate 1   (met 1, not met 5)
    verify: 7 verdict(s) byte-identical to the live run's verdicts.jsonl
```

**One design detail makes the byte comparison possible and is easy to get wrong.** A verdict does
*not* carry the capture's path. A live judgement runs against a path inside the run directory and
a re-judgement is handed an absolute one, so including it would make the identity check fail on a
difference that means nothing. The run record carries the path; the verdict carries the finding.

`verdictJson` lives in `src/assert/Judge.cpp` and only there, for the same reason: two renderers
eventually differ by a space.

---

## 3. What a verdict is entitled to say — CR-AS-4, per form rather than per kind

CR-AS-4 asks that *"every condition kind is classified as absence-dependent or not"*. Classifying
per **kind** turned out to be the wrong granularity, and the classification shipped is finer than
the requirement asks:

| form | not-met is decidable when | mechanism |
|---|---|---|
| `proximity` | the closest observed approach clears the threshold by more than the pair could have closed inside the largest unobserved window, and both tracks are bounded over the segment | continuity over present samples |
| `area` | the same, measured to the region's boundary | continuity over present samples |
| `terminal_state` + `removal_reason` | every occupancy of the entity is closed by a record stating some *other* reason, or carries a sample at the segment's last sampled instant | **`capture-format-v1.md` §8.1**, normative |
| `terminal_state` + `field`+`equals` | **never** | nothing bounds a string field's rate of change |

**The third row is the one that matters, and it is not our inference.** §8.1 states the invariant
and states that it holds: *"Within one `(entity, occupancy)` pair, no `sample` ever appears after
that pair's `entity_remove`. That invariant does hold, and it is the one worth asserting."* So a
sample carrying `(E, k)` at time *t* is **positive evidence** that occupancy *k* had not been
removed at *t* — and a sampling gap does not weaken it, because a re-created entity carries a
*higher* occupancy.

**That is what makes [B]'s own dangerous example answerable.** *"Did anything reach a terminal
state it should not have"* is the question the PRD singles out as *"naturally implemented as 'no
record says it did'"*, which *"returns 'passed' from a file that is missing the frame in which it
happened"*. Here it returns a sound `not_met` — because the entity kept publishing, not because
nothing said it did.

**And the fourth row is the honest limit.** `field`+`equals` is never decidable in the negative:
a string field could take a value and leave it inside one sampling gap. `health` was measured
moving through nominal → degraded → disabled → wrecked → destroyed with **zero regressions in
seven runs**, which is a direction and not a guarantee, and the classification deliberately does
not lean on it.

### The bound, and its weakest link

`(v_a + v_b) · Δt_max + ½ · 20 m/s² · Δt_max²`, over the largest gap observed in either track.
The acceleration term is F-21's measured platform clamp and exists so the bound for two static
entities is 0.10 m rather than exactly zero — a velocity is read from a sample field, and a field
can change between samples.

**Its weakest link is stated rather than buried:** the term assumes the platform cannot exceed
its own measured clamp inside one frame, and F-21 measured that clamp on one entity profile in
one scenario. **F-28.** Its practical weight is small — across the committed sweep the term
contributes 0.10 m to bounds of 1.20–22.10 m — but it is the assumption a reviewer should attack
first.

---

## 4. What running it found, and none of it came from a test

Three defects and two platform findings, and **every one needed a real run**. This is F-24's
lesson from M5 arriving again, and it arrived harder: M6 added far more report surface than M5,
and the unit suite grew to 166 checks without seeing any of the following.

### F-29 — the axis update races the roster burst, and the loser is the whole campaign

**The first execution of the twenty-run campaign never ran a campaign run.** Its determinism
self-test failed, correctly, and the campaign stopped at exit 3 with zero runs attempted, exactly
as [B] step 4 requires.

| | |
|---|---:|
| samples compared | 50 396 |
| **differing** | **23** |
| every difference at `sim_time_s` | **0** |
| every difference in field | **`velocityNed`** |
| every differing entity | a **raider the axis updates** |

The mechanism is F-22's, in a fourth observable form. The axis acts through
`afterLoadBeforeStart`; the start-up roster burst is published at *load*. Normally the burst is
complete before the update lands, and both runs' `t = 0` samples carry the scenario's authored
velocity. When the burst is republished after the update in **one** run of the pair and not the
other, one capture holds the authored value and the other holds ours.

**The consequence is worse than the shape M5 measured.** A frozen segment 0 costs one run. This
costs **the whole campaign** — exit 3, zero runs attempted — because it fails the gate rather
than one run's judgement. R14 named the frozen shape; this is the same mechanism with a
campaign-level blast radius, and it strengthens E-4 for the second time.

**The gate was not weakened and no retry was added.** The campaign was executed again and both
executions are kept — `campaigns/m6-gate-refused/` and `campaigns/m6-campaign/` — which is M5's
own rule applied to itself: the re-run was because the dice came up badly, not because the tool
was wrong, and reporting only the clean execution would be choosing evidence.

**One thing this execution is uniquely worth keeping for.** It is the **first time the content
gate has ever failed on a real pair for a real reason**. M4 could only demonstrate a failing gate
by forcing `--gate-basis bytes`. That difference turned out to matter, because it exposed the
next finding.

### F-30 — the platform round-trips an injected velocity to 1e-14, and the authored one to zero

The two differing samples, in full:

```
run 000  RedUAV_E_01  t=0  velocityNed [-1.0103336092965664e-14, -55, 0]
run 001  RedUAV_E_01  t=0  velocityNed [0, -55, 0]
```

`positionGeodetic` is identical in both. **Our arithmetic is exact** — `velocityFor` is
`direction × value` with no normalisation, so the declared `[0, -1, 0]` at 55 m/s produces
exactly `[0, -55, 0]`. The `-1.01e-14` is introduced between `sendEntityUpdate` and the capture,
by the platform.

That is what makes F-29's race *visible*. With a bit-identical vector, an update landing before
or after the burst would produce identical values and there would be no difference to find. It is
the platform's, it is not worked around, and it is recorded because a consumer comparing an
injected value against an authored one will meet it.

### F-31 — every difference in an array field printed two empty values

Reading F-29's failure report showed this:

```
    FIRST DIFFERENCE    segment (part 0, segment 0)  entity RedUAV_E_01@1
                          sim_time_s 0
                          field "velocityNed":    against
```

The field is named. **Both values are blank.**

M4's diff rendered a value as `isNumber() ? raw() : text()`, and `Value::text()` is empty for an
array. `positionGeodetic`, `velocityNed` and `orientationYprRad` are all arrays — three of the
four fields a divergence is most likely to be in. CR-DET-3 and CR-REP-4 both require a failure to
name the deciding **values**, not only the field, so this is a requirement defect and it shipped
in M4.

**Why it survived three milestones is the part worth keeping.** It takes a real content-gate
failure on a real pair to reach that code, and until F-29 the content gate had never failed on
one: M4's failing-gate evidence came from forcing the byte basis, which reports a byte offset and
never gets there. And every synthetic capture in `determinism_test` carried only scalar fields,
so 75 checks could not see it either. **Both halves are now fixed** — a verbatim renderer, and an
array field in the suite's own capture builder, with a check that asserts on both values.

### F-35 — F-24's fix broke again, on the same table, one milestone later

The twenty-run campaign finished, and its sweep table printed this on **every single row**:

```
  11     000   fail                         -        -     50534  (no bar - this run did not complete)
  55     004   fail                         -        -     50410  (no bar - this run did not complete)
```

Eighteen of those runs measured perfectly well. `-` is F-24's "a missing measurement is not a
measurement of zero" guard, and it was firing on runs that had measured — because the guard asked
`outcome == Completed`, and **M6 renamed the outcome a judged run gets**. There is no `Completed`
in a judged campaign; there is `pass` and `fail`.

**The lesson is the same one twice, and it is worth stating as a rule.** A guard keyed on one
value of an enum is a guard that breaks silently when the enum grows, and the growth was M6's own
deliberate change. The predicate now asks what it always meant: did this run *execute*.

**And it prompted something worth having on its own.** Re-checking a report cost a 25-minute
re-run, which is why the defect survived long enough to reach a committed artifact. `n8ro-campaign
report <dir>` now re-renders a stored campaign from the run records it already wrote — starting
nothing, reading no capture, needing no N8RO install — through **the same printer** the live
campaign uses, so a re-rendered report cannot disagree with the one that was printed. That is
[B]'s *"cheap to iterate on"* applied to the report rather than to the judgement.

### F-26 — the locale-safety test was itself locale-unsafe

`assertion_test` runs every check twice, the second time under `German_Germany.1252`, and its
capture builder formatted numbers through the C library. The second pass therefore wrote
`"sim_time_s":0,05` into what is supposed to be JSON: every sample was rejected as malformed, the
segment classified `indeterminate`, and the pass would have "agreed" with the first about a pile
of verdicts that were never computed. It crashed instead, which is how it was found.

The builder now uses the product's own locale-free `fixed()`. **The finding is that a test
written to prove locale-independence was itself locale-dependent**, and it went unnoticed because
M5's parameter suite never formats a number, so the same double pass was genuinely clean there.

### F-32 — the vendored digest is a verbatim excerpt that stops one heading early

Found while deciding OQ-5, and it upgrades **F-19** rather than adding to it. F-19 recorded that
`contract/condition-file-schema.md` *"cannot be verified by a pin check"*, which is true and is
not the end of the check: it **can** be verified by **correspondence**, and it was. The digest's
own pin (`eedc228`) resolves; its cited README section exists there; its content is verbatim; and
that section is byte-identical at `main`.

What the check found instead is that the excerpt **stops one heading before the arithmetic every
geometric verdict rests on** — *"How distance is computed"* and *"Boundary semantics"*, neither of
which crossed. From the digest alone, `within_m` is a distance with no stated metric. Raised as
**E-5** ([issue #3](https://github.com/EgeCankaya/EXT-08/issues/3)) and detailed in
`docs/m6-oq5.md` §5.

**It is the first `contract/` finding that could not be handled by implementing what the text
says**, because there is no text. E-3 and E-4 are imprecisions in frozen, vendored specification —
EXT-17 implements what it says and names the gap beside it. Here EXT-17 had to *decide*, and
`src/assert/Geodesy.h` states the decision with its constants so a verdict is reproducible with a
calculator.

### F-27 — a dead host is not noticed until the run timeout expires

Found while injecting fault 4. The wait blocks on engine-state publications; when the host goes
away they stop arriving, and there is deliberately **no second timed quantity** watching for
silence — CR-EX-4 makes the run timeout the only clock in a run.

So an unattended campaign meeting [B]'s fourth ugly reality survives it, which is what CR-EX-6
asks, and pays `--run-timeout-ms` in wall clock for each occurrence — ten minutes at the default.
A heartbeat-silence detector would be a second clock in a run; the cheaper answer is to size
`--run-timeout-ms` against the frame budget, and the README says so.

---

## 5. The four ugly realities, injected deliberately

[B] step 8 names four and says *"a campaign of a hundred runs will meet all four"*. Each was
injected into run 001 of a three-run campaign, and in each case **the campaign continued to run
002**.

| fault | how it was injected | outcome | stage named |
|---|---|---|---|
| a host that fails to start | the host executable is a path that does not exist | `infrastructure_error` | `host_start` |
| a scenario that refuses to load | **a real request** for a name the catalogue does not contain | `infrastructure_error` | `scenario_load` |
| a run that never ends | **a real predicate** the run cannot reach, against CR-EX-4's real backstop | **`timeout`** | — |
| a host that dies mid-run | terminated **by the handle this run created**, at frame 51 | `infrastructure_error` | `host_died` |

**Two of the four are real misconfiguration rather than simulation**, which is stronger evidence.
The scenario refusal gets the platform's own behaviour and it takes the dangerous shape F-5
named — the host does not fail, it **sits idle**, and what times out is our wait for `loaded`.

**`run_never_ends` produces `timeout`, its own outcome, and never `fail`.** That distinction is
CR-EX-5's and it is the one [B] insists on: a run that did not finish has told you nothing about
the scenario.

**Telling a timeout from a dead host takes asking the process.** Both look identical from the
wait — the predicate was not satisfied in time — so the run asks `st.host->isAlive()`, and a host
that has gone away is an infrastructure failure rather than a slow run.

**The other two acceptance criteria, checked:**

- *"No fault leaves a host process running"* — **0 host processes alive** after all four
  campaigns.
- *"…or a partially-written per-run record"* — **12 run directories, 12 `run.json`, 0 missing.**

**An injected campaign can never be mistaken for a clean one.** The fault is named in `--help`,
in that run's `run.json` as `injected_fault`, and in the campaign summary. Both of those
recordings were added *after* reading the first injection's `run.json` and finding the field
empty — the flag was plumbed into the run and not into the record.

**And an injected run is not judged.** A run whose outcome is `infrastructure_error` or `timeout`
carries **0 verdicts against 7 declared conditions**, with `judged_this_run: false` and a reason.
That is CR-EX-5 taken literally — inventing verdicts for a run the harness broke would make an
infrastructure failure into a test result — and it is also CR-AS-2's cut-short signal working:
*"a reader seeing fewer verdicts than conditions treats the run as cut short, not as passing"*.

---

## 6. The twenty-run campaign

**One command, twenty runs, no manual step** — CR-EX-7 and [B]'s first acceptance criterion:

```cmd
n8ro-campaign repeat --out-dir campaigns\m6-campaign ^
                     --campaign examples\atacama-raid-speed-20.json ^
                     --conditions examples\atacama-raid.conditions.json ^
                     --recorder <path-to-n8ro-bridge.exe>
```

**The gate passed first**, at the declared value 55 m/s: 50 361 samples compared, 50 361
agreeing, **zero differing**, coverage 100.0000%. It establishes determinism **for that value**,
which is one claim at a named point and not one per run.

### The four outcomes

| | |
|---|---:|
| attempted | **20** |
| `pass` | **8** |
| `fail` | **10** |
| `timeout` | 0 |
| `infrastructure_error` | **2** |
| **sum** | **20** |

No aggregate anywhere collapses two of them. **32 indeterminate verdicts** are reported beside the
four and never folded into any of them, and the 32 decomposes exactly:

- **18** — one per judgeable run, every one `raid-leader-degraded`, the `field`+`equals` form
  that is never decidable in the negative.
- **14** — 7 conditions × the 2 runs that could not be judged at all.

**Those two are R14's shape**, at 150 and 210 m/s: segment 0 classified `frozen`, so `sim_time_s`
does not order its samples and the gap a not-met verdict is bounded against cannot be measured.
Their records say so in three places — `outcome: infrastructure_error`, `error.stage: "judge"`,
and a `not_judgeable_reason` that states in the file that this is *"not a determinism failure and
not a failing scenario"*. Each still carries **one verdict per declared condition**, all
indeterminate, so nothing about them is silent.

**2 of 20 is 10%, against R14's measured 11.4% — the predicted rate, met live.** No retry was
added.

### CR-PAR-2's third criterion, finished

M5 recorded it as **half-met**: a *result* changed across the sweep and a *verdict* could not,
because no condition existed. It is now met, and by three conditions at three thresholds:

| condition | not satisfied up to | satisfied from |
|---|---:|---:|
| `raid-leader-crosses-corridor` (area, polygon) | 85 m/s | **100 m/s** |
| `raid-leader-enters-depot-ring` (area, circle) | 100 m/s | **115 m/s** |
| `raid-leader-reaches-airfield` (proximity) | 130 m/s | **170 m/s** |

**And the run outcome flips with them**: `fail` from 11 through 130, `pass` from 170 through 380.
The report prints one column per condition **whose outcome changes** and lists the rest below as
constant, because a column that never changes is not a trend.

### The count column, and a trend that is not a line

```
  value  run   outcome                   adds     keys   samples
  11     000   fail                        48       48     50534  ##
  55     004   fail                        47       47     50410  .
  85     006   fail                        54       54     50240  ###############
  115    008   fail                        63       63     48970  ###################################
  170    011   pass                        65       65     48927  ########################################
  190    012   pass                        65       65     48273  ########################################
  240    014   pass                        59       59     47916  ##########################
  300    016   pass                        56       56     47990  ####################
  380    019   pass                        54       54     47929  ###############
```

`adds` counts entities the run created — weapons fired. It rises 47 → 65 to a **peak at 170–190
m/s** and falls back to 54 at 380. That is the non-monotone behaviour M5 could only see at the
top of its range, now resolved across twenty points: **a raid fast enough to overfly is engaged
less, not more.** A sweep reporting "higher is more" would be reporting a line that is not there.

The bar is scaled between 47 and 65 rather than from zero, and the table says so.

### CR-CAP-1, over the whole campaign

```
  no host is started and no bus subscription is made - this binary links nothing that could
  ...
    verify: 7 verdict(s) byte-identical to the live run's verdicts.jsonl
```

### What is committed, and what is not

The configuration, every `run.json`, every `verdicts.jsonl`, `campaign.json`, `campaign.log`,
`self-test.json` and `report.txt`. **Not the captures** — they are about 480 MB, and
`.gitignore` has excluded captures since this repository's first commit for that reason.
`MANIFEST.md` carries each one's size, SHA-256 and read-back counts instead. **F-34**, and
`docs/decisions-m6-m7.md` B6 says what that buys and what it does not.

**`campaign.log` and `report.txt` are both kept, and they disagree about one table.** The log is
what the campaign printed live, with F-35's defect in it — every row of the count table reading
`-`. `report.txt` is the same records re-rendered after the fix, by the same printer. Keeping
both is this project's habit with a re-run: the evidence of the defect is worth more than a tidy
directory.

---

## 7. What M6 did *not* do

- **It did not close OQ-2.** The gate basis is still a flag and a ruling still changes a default
  and no code. Re-checked at M6: still awaiting a reply.
- **It did not deliver E-1.** Deferred by DRI decision (`findings.md` §E); its cost is carried
  forward to M7 unchanged, including that M7 must report the sweep-legibility success metric as
  **unmet** rather than claim it.
- **It did not weaken the determinism gate or add a retry**, and F-29 was the first real
  opportunity to do either. R12 and R14 hold.
- **It did not enforce the fidelity ceiling in code** (R13). The committed campaign stops at
  380 m/s and says why in the file itself.
- **It did not add a fourth condition kind.** The vocabulary is closed and a fourth spelling is a
  named parse error. A fourth kind is a PRD revision, which is ADR-5's containment.
- **It did not judge a rotated capture set.** The reader supports rotation (M3) and the campaign
  does not produce one under `stop`; a condition over a multi-part set is untested.
- **It did not widen ADR-1's one-machine scope.**
