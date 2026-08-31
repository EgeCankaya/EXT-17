# Decisions taken without asking, M6 and M7

**Why this file exists.** The DRI went to sleep on 2026-09-01 with M6 unstarted and asked that
the remaining milestones be completed without further questions, with every decision that would
normally have been a question written down for review. This is that list.

**How to read it.** Each entry says what was decided, what the alternative was, why this one was
taken, and — the part that matters — **what it would cost to reverse**. Nothing here is
load-bearing on anything that cannot be changed; where a decision touches measured evidence, that
is said explicitly, because re-deciding then means re-measuring.

Three of these were put to the DRI in a proposal before they went to sleep and were answered
*"do what you think is best"*. They are marked **[proposed]**.

---

## A. Decisions inside OQ-5

### A1. `expect` — one key added to the vendored condition shape

**Decided:** add exactly one key, `expect`, taking `met` (default) or `not_met`.

**The alternative** was to adopt the vendored shape with nothing added, which is what the OQ-5
document said at first draft and what §2 of it originally claimed.

**Why.** Found by running it rather than by reasoning. The committed example was written without
polarity, judged against the seven M5 captures, and the result was that
`command-centre-destroyed` asserted the command centre **should** be destroyed — the exact
inverse of [B]'s own question, *"did anything reach a terminal state it should not have"*.
CR-EX-5 fixes the mapping (`fail` ⇔ a declared condition was evaluated and not met), so every
condition is read as a thing that should hold, and the vendored shape expresses non-occurrence
only for the `area` kind through `test: inside|outside`. Under the PRD's own resolution rule —
*"adopt unless a required condition cannot be expressed"* — a required condition could not be
expressed, so the rule says to act.

**What it cost to keep the adoption honest:** the verdict still records the **fact** in the
vendored schema's own terms (`met` / `not_met`), and the expectation is a separate field
(`satisfied` / `violated` / `undetermined`). So EXT-08's verdicts and EXT-17's re-judgements stay
directly comparable, which ADR-5 names as a benefit of adopting.

**To reverse:** delete the key, and the committed example loses two conditions. `docs/m6-oq5.md`
§3.5 and `src/assert/Conditions.h` carry the reasoning.

### A2. CR-AS-4 classified per **form**, not per kind

**Decided:** four rows, not three. `terminal_state` splits into `removal_reason` (decidable, by
format §8.1's normative invariant) and `field`+`equals` (never decidable in the negative).

**The alternative** was the three-row classification CR-AS-4 literally asks for (*"every
condition kind is classified"*).

**Why.** The two forms of `terminal_state` differ completely and a single row would have to take
the weaker of the two — making every `removal_reason` verdict `indeterminate`, including [B]'s
own example question. §8.1 licenses the stronger answer and says so normatively, so taking the
weaker one would be discarding evidence the format went to trouble to guarantee.

**This is stricter than the requirement, not looser.** Reversing it makes the tool answer fewer
questions, not more.

### A3. The continuity bound, and its acceleration term

**Decided:** a not-met `proximity` or `area` verdict is sound when the observed margin exceeds
`(v_a + v_b) · Δt_max + ½ · 20 m/s² · Δt_max²`.

**The alternative** was to call every not-met verdict `indeterminate`, which is the literal
reading of *"an assertion never reads absence as evidence"*.

**Why.** The literal reading makes the tool useless — every negative answer becomes "cannot say",
including the one the committed sweep exists to show. The bound is computed from **present**
samples (positions, velocities, and the observed gap), so a verdict that clears it is a
conclusion from records rather than from silence. Measured across the committed sweep: the
largest gap is 0.1000 s in all seven M5 runs, and the tightest margin clears its bound by a
factor of 64.

**The acceleration term is F-21's measured 20 m/s² platform clamp**, and it exists so that two
static entities get a bound of 0.10 m rather than exactly 0.00 m — a velocity is read from a
sample field, and a field can change between samples.

**Where this is soft:** the bound assumes the platform cannot exceed its own measured
acceleration clamp inside one frame. That is an inference from F-21, which measured the clamp on
one entity profile in one scenario. It is stated in `src/assert/Judge.h` and it is the weakest
link in the soundness argument. **Recorded as F-28.**

### A4. A run with no running segment is `infrastructure_error`

**Decided:** R14's frozen-segment-0 shape produces `infrastructure_error` with a named stage,
not `fail`, not `pass`.

**The alternative** was `fail`, or a fifth outcome.

**Why.** A fifth outcome is forbidden — [B] fixes the vocabulary at four and CR-AS-4 is explicit
that `indeterminate` is a verdict state. `fail` would say the scenario behaved wrongly, which it
did not. `pass` would be the "all passed having checked nothing" failure. CR-EX-5 already routes
*"an unreadable or structurally unsound capture"* to `infrastructure_error`, and a capture whose
only segment is frozen cannot be judged at all — `sim_time_s` does not order its samples and the
gap the bound needs cannot be measured. Same reasoning routes **"nothing at all was decided"**
there too.

**Consequence to watch:** R14 makes this roughly 1 run in 9 under parameterisation, so a
twenty-run campaign is expected to lose one or two runs this way. They are reported as
infrastructure errors, which is honest, but a reader skimming the four counts will see a number
that is not a platform fault in the usual sense. The README says so.

---

## B. Decisions about the campaign and its evidence

### B1. A separate `campaigns/m6-campaign/`, leaving M5's evidence untouched **[proposed]**

**Decided:** the twenty-run campaign is new evidence in its own directory. `campaigns/m5-sweep/`
is not re-run.

**Why.** M5 §5 explicitly refused to re-run a campaign to improve an artifact, on the grounds
that re-running re-rolls R14's dice and choosing the better execution is choosing evidence. That
rule applies to itself.

### B2. Twenty values rather than seven repeated

**Decided:** `examples/atacama-raid-speed-20.json` declares twenty distinct values from 11 to
380 m/s.

**The alternative** was M5's seven values with some repeated to reach twenty.

**Why.** CR-EX-7 asks for twenty runs; CR-PAR-2 asks for a trend. Twenty distinct values give
both, and they resolve the two thresholds M5 could only bracket — the corridor flip between 82.5
and 110, and the proximity flip between 110 and 165. Repeating values would have given the run
count without improving the trend.

**Staying inside 400 m/s is this file's job, not the tool's** (R13, unchanged). The top value is
380.

### B3. `C7` (`health equals degraded`) as the committed `indeterminate` case **[proposed]**

**Decided:** the committed example carries a condition that is `indeterminate` in every run, and
it is a real one rather than a contrivance: `RedUAV_N_01` carries `health: "nominal"` in all
1 196 of its samples in every measured run.

**Why.** CR-AS-4 asks that `indeterminate` be visible in the report. A demonstration that only
fires on a synthetic capture would leave the committed campaign never exercising the path a
reader most needs to understand.

### B4. Fault injection is a documented flag, not a separate build

**Decided:** `--inject-fault` and `--inject-at-run`, in `--help`, in the run record, and in the
campaign summary.

**The alternative** was a test-only build, or editing configuration by hand per fault.

**Why.** CR-EX-6 asks that each fault be *"exercised deliberately, by injection"*, and the
evidence has to be re-runnable by a reviewer. The risk of a visible flag is a campaign run under
injection being mistaken for a clean one, so the fault is recorded in three places rather than
one.

**Two of the four faults are real misconfiguration rather than simulation**, which is stronger
evidence: `scenario_load_refusal` asks the platform for a scenario the catalogue does not contain
and gets the platform's own refusal — which takes the dangerous shape F-5 named, the host sitting
idle rather than failing. `run_never_ends` sets a predicate the run cannot reach and lets
CR-EX-4's real backstop fire.

### B5. `host_dies_mid_run` fires at a frame, not on a stopwatch

**Decided:** the host is terminated at a quarter of the frame budget, by the handle this run
created.

**Why.** A fault injected on a wall clock lands somewhere different every time, which is the one
thing an injected fault must not do if the record of it is to mean anything. The frame is what
this project measures runs in. Termination is by handle and never by image name — the security
posture's rule and CR-EX-1's.

---

## C. Decisions about escalation and process

### C1. E-5 raised as a GitHub issue, not left `drafted` **[proposed]**

**Decided:** E-5 (the vendored condition-file digest is truncated) is filed against EXT-08,
matching E-3 and E-4.

**The alternative** was to leave it `drafted` and let the DRI file it.

**Why.** `findings.md` §E is emphatic that `drafted` is not `raised` and that a finding written
down and not delivered is *recorded*, not raised. E-3 and E-4 set the precedent — both went as
issues, both from this project, both non-blocking. E-5 is the same shape and the same recipient.
E-1 is the one the DRI reserved, and it is untouched.

**To reverse:** close the issue. Nothing in EXT-17 depends on the answer; the geodesy is decided
here and stated with its constants.

### C2. E-1 is not touched, and its M7 cost is carried forward

**Decided:** nothing. Per `findings.md` §E, E-1 is deferred by DRI decision and its delivery is
the DRI's. M7 reports the sweep-legibility success metric as **unmet**, because its named method
is mentor review and no mentor has reviewed it.

**This is the item M6 was told to hand to M7 correctly, and it is handed on unchanged.**

### C3. The 5-minute recording (CR-DOC-2) is reported as not delivered

**Decided:** M7 states it as an outstanding deliverable rather than substituting something else
for it.

**Why.** It needs a person, and the PRD scheduled it explicitly at M7 *because* the equivalent
was not delivered upstream (R10). Substituting a written walkthrough and calling the requirement
met would be exactly the failure R10 exists to name. The walkthrough is written anyway, as the
script the recording would follow, so the remaining work is the recording and not the preparation.

---

## D. Things deliberately NOT decided

- **OQ-2** is still open. The gate basis is still a flag with a default and a ruling still
  changes a default and no code.
- **E-1** is the DRI's, per §E.
- **Cross-machine reproducibility.** ADR-1 measures one machine and still does.
- **The fidelity ceiling is still not enforced in code** (R13). The committed campaign stops at
  380 and says why in the file itself.
- **No retry on a frozen segment** (R12, R14). The refusal names the shape; nothing retries.
