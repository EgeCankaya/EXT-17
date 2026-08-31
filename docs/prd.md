# EXT-17 — Headless Campaign Runner

> **One-liner:** A standalone C++17 program that executes many unattended N8RO simulation runs, varies one input across them, judges each run against conditions declared outside the code, and reports pass / fail / timeout / infrastructure-error across the campaign — with a run-to-run diff that names the first point of divergence.

**Date:** 2026-08-31
**Revision:** 3 — two open questions resolved and two risks re-rated, all on this project's own measurements. OQ-6 decided in both halves; OQ-4 freed; R9 closed; H4 validated; one new risk. No requirement removed. See §"Revision history".
**Status:** Draft
**Owner:** EXT-17 implementer
**DRI:** egemencankaya@icloud.com
**Audience:** Engineering (implementer), Mentor (reviewer), the owner of EXT-17's brief (one escalation), EXT-08 author (upstream defects)
**Platform baseline:** N8RO runtime 2.1.328, SDK component `com.n8ro.dev` 2.1.328
**Consumed contract:** `n8ro-capture/1`, frozen at EXT-08 M7

## Revision history

| Rev | Date | Change |
|----:|------|--------|
| 3 | 2026-08-31 | **Two open questions resolved and two risks re-rated, every one of them on this project's own measurements rather than on inherited numbers.** **(a) OQ-6 is decided in both its halves** (M3, `docs/m3-oq6.md`): a campaign ceiling of **8 GiB over the whole campaign directory** — captures *and* logs, because a capture-only projection under-states a campaign by 22.6% and a pre-flight check that passed on it could still let the disk fill with logs — checked against free space before run 1 and against actual usage after every run, with reaching it stopping the campaign under a named outcome; and **`--on-size-limit stop`** per run, with `--capture-max-bytes` defaulting to `61 000 × --frames`. **The choice was made by exercising rotation on a real run rather than by reading about it**: `rotate` works completely and is not lossy — a four-part capture stitched back to an unrotated run's roster lifecycle exactly — and was declined because one run's two segments become five `(part, segment)` keys, four of them fragments of one segment that nothing in any file identifies as such. CR-CAP-5's ceiling stops being unset. **(b) R9 is closed by measurement** and OQ-4's framing corrected: M2's spike found **all three** of [B]'s axes reachable over the bus with no authoring into `C:\N8RO`, so the axis choice at M5 is free on merit rather than forced, and "which scenario from the catalogue" is no longer a *fallback*. The rev-1 rating of "Unknown — not yet investigated" was correct when written and is now false. **(c) H4 is validated**: the reader passes 78 conformance checks over the vendored fixture, six mutations and thirty real captures, links neither EXT-08 nor the SDK, and cost well under a day against the ~2–3 days the entity picture cost upstream. **(d) A new risk, R11**: a specification can be imprecise as well as stale, and §6.7's summing rule is wrong for `segments` — raised with EXT-08 as E-3 and not worked around. **(e) M3's validation line is corrected** to claim only the half of CR-CAP-1 that M3 can meet. No requirement was removed, no quotation changed, and no provenance marker relaxed. |
| 2 | 2026-08-31 | **Upstream built the thing R3 was a mitigation for, and this document said six times that it had not.** EXT-08 delivered BTB-CAP-6 at its PRD rev 11 / producer 0.9.0: a real byte bound on a capture file (`--capture-max-bytes`), an operator-chosen `stop` or `rotate` action (`--on-size-limit`), and both stated in the capture's own `header.limits`. **(a) The format did not move**, which is the part that matters most here — four keys were added to two existing record types, `size_limit` was already in the closed sets for `trailer.end_reason` and `segment_close.reason`, and §13 makes that non-breaking. `n8ro-capture/1` is unchanged and **CR-CAP-3's version rejection and CR-CAP-2's ignore-unknown-keys criterion both hold as written**. `contract/` has been **re-pinned to EXT-08 `0fe7cd5` / producer 0.9.0** and now carries the current specification; R4 is re-rated and its recurrence noted, since this is the second added-key drift in two producer releases. Every capture written before it stays valid. **(b) R3 is re-rated from High/Medium to Medium/Low and its premise corrected.** It read "specified but unbuilt — no rotation, no size ceiling in the header". All three clauses are now false. **(c) CR-CAP-5 survives, and is better for it.** The upstream bound is **per capture file**; a 20-run campaign is twenty of them, so a campaign-level ceiling is still this project's own concern and [B] still does not mention disk. What changes is that the requirement is no longer a lone mitigation for someone else's gap — it now *composes* with an upstream control it can configure, and gains an acceptance criterion saying so. A campaign that passes `--capture-max-bytes` per run gets a bounded, well-formed, explicitly-truncated capture instead of one cut off mid-line by ENOSPC, which is the failure CR-CAP-5's customer scenario is actually about. **(d) OQ-6 gains a second decision**: not only the campaign ceiling and its stop-or-abort choice, but which upstream action to configure per run — `stop` (bounded total, run's tail lost) or `rotate` (tail kept, unbounded aggregate). **(e) Two upstream items in §"Cross-service impact" are discharged** — the byte-limited capture, and the demo recording, which EXT-08 has now published. R5 is untouched and remains the one that binds. **(f) OQ-2's escalation was sent** to the brief's author on 2026-08-31; still awaiting a reply, and CR-DET-1 stands unchanged in the meantime. No requirement was removed, no quotation changed, and no provenance marker was relaxed. |
| 1 | 2026-08-31 | Initial PRD. Written against the client brief read directly off disk. Carries one **named deviation** from the brief (CR-DET-1, the determinism gate — see ADR-1), one **named defect in the brief** (`EntityStateSample.h`, R7), and one **named unsolved problem** promoted to a rev-1 open question rather than left for milestone 2 (OQ-1, end-of-run detection). **Conformance-audited against the .docx before issue**, which changed seven things: three requirements were found stricter than [B]'s words and are now marked [ORIGINATED] in place (CR-EX-3's wall-clock prohibition, CR-REP-1's machine-readability, CR-DOC-1's limits contents); two [B] elements were found uncovered and are now covered (paragraph 9's *"the results are identical"* → a new CR-DET-1 criterion; paragraph 15's determinism-by-contract claim → ADR-1 context, with its untested *"every machine"* half stated); two surface-table rows were found unaddressed and are now addressed (the client/import-library linkage boundary; the scripting-namespace route, declined as Option 5, and the catalogue-query dependency added to CR-PAR-1); and the backward-coverage denominator was corrected from 33 to 53. No requirement was weakened and no quotation changed. |

---

## Purpose and scope

This PRD covers **EXT-17 only**: a Track C standalone program that runs a campaign of headless N8RO simulation runs, varies one parameter across them, evaluates declared conditions against what each run published, and reports across the whole campaign.

It exists as a separate document because **EXT-08 and EXT-17 are separate git repositories with no shared source**. Everything EXT-17 receives from EXT-08 crosses that boundary as a documented, versioned artifact — the `n8ro-capture/1` format specification and real capture files, vendored read-only in `contract/`. EXT-17 binds to the artifact. It does not read EXT-08's source, and no requirement in this document cites an EXT-08 internal.

The boundary of this work: **EXT-08 observes and judges one run; EXT-17 orchestrates and reports across many.** EXT-08 supplies the capture and a reference referee shape. EXT-17 supplies execution, parameterisation, campaign-level assertion, and reporting.

### Source inputs

**Authoritative — and there is exactly one.**

- **[B]** `docs/EXT-17-Headless-Campaign-Runner.docx` — the client brief. Read directly off disk during PRD preparation (paragraphs and its one table). It is the only binding source in this document. Every `[QUOTED]` marker below quotes it.

**Contextual — informational, never binding. None of it may be read as something the client asked for.**

- **[C1]** `contract/` — the vendored, read-only artifacts from EXT-08: `capture-format-v1.md` (frozen), `PROVENANCE.md` (seven measured findings), a real capture fixture, and a reference condition-file schema.
- **[C2]** `C:\Projects\EXT-08\docs\prd.md`, `escalations.md`, `decisions-m5-m7.md` — the upstream project's own record. Read for context and process lessons. **Its decisions were scoped to its own situation and bind nothing here.** EXT-08's own OQ-6 explicitly leaves EXT-17 free to supersede the condition-file schema.
- **[C3]** Measurements taken by this project during PRD preparation — enumerated with their numbers in §"Prior art and lessons learned", so that anything sourced to [C3] is checkable by re-running the same command.
- **[C4]** Briefing notes supplied with the request to write this PRD. Context. Where a claim in [C4] mattered, it was verified against [B], [C1] or [C3] before being used, and this document cites what it verified against rather than [C4].

> **A note on provenance discipline, and why this section is unusually strict.**
>
> The upstream project's PRD listed its briefing notes as a **binding** source alongside the client brief, then made them the sole authority for a major scope decision. Its own rev-9 audit found the citation was unverifiable and the provenance fabricated; the underlying decision had in fact been authorised by the brief all along. Separately, two of its requirements were tightened beyond the brief's words, read thereafter as brief-traced, and then had to be "rescoped" across two revisions — which looked like renegotiating a client contract and was not, because the client had never asked for the strict form.
>
> **This document does not create a second binding source.** [C4] is context. If a requirement here cannot quote [B], it says so in the requirement itself.

### How to read the provenance markers

Every functional requirement, acceptance criterion and constraint below carries exactly one marker.

| Marker | Meaning |
|---|---|
| **[QUOTED]** | [B] says this. Its words appear in quotation marks inside the requirement. |
| **[DERIVED]** | Follows from a quoted line of [B]. The line is quoted **and** the derivation is stated in one sentence. |
| **[ORIGINATED]** | Nobody asked for this. This project decided it is needed. Who decided and why is stated. |

**The rule that governs the split:** a requirement that is **stricter than [B]'s words is [ORIGINATED]**, even when it is about a topic [B] raises. Sharing a subject with the brief does not make a requirement brief-traced. Anything taken from [C1], [C2] or [C3] is [ORIGINATED] or [DERIVED]-from-context — **never [QUOTED]**, because those documents are not the client.

---

## Problem statement

> **When** an engineer needs to know not what a simulation did once, but where its behaviour changes across a range of inputs,
> **they struggle with** the fact that the only available instrument is a person watching one run at a time and describing it,
> **which means** every claim about the system is anecdote rather than evidence, no regression can be detected, and no parameter's effect can be shown.

[B] states the value directly: *"One watched run tells you what happened once. A hundred unwatched runs tell you where the behaviour changes, and that is the difference between a demonstration and evidence."*

The current workaround is EXT-08: it turns **one** run into a durable, judgeable artifact. That is the prerequisite, and it is done. What does not exist is anything that produces many such runs without a human, varies an input across them, and reports across the set. EXT-08's own PRD says so in its scope boundary: it *"does not orchestrate runs, does not vary parameters, does not manage a campaign, and does not start or stop simulation hosts"* [C2].

There is a second cost, and [B] names it as the reason the project is worth doing at all: *"Building this is also the best possible way to understand the platform's determinism guarantee, because you are the first person who will actually depend on it."* Nobody has yet depended on it. This project is where the guarantee is either confirmed or characterised.

### Prior art and lessons learned

EXT-08 is the prior art, and it is unusually well-documented prior art: it hit several walls this project would otherwise hit again, and wrote down what was on the other side. What follows is **context [C1] [C2], verified where cheap against this project's own measurements [C3]**. None of it is a client requirement.

**Measured by this project during PRD preparation [C3]** — each line is a command anyone can re-run:

- **`EntityStateSample.h` does not exist in release 2.1.328.** `C:\N8RO\include\n8ro-sim\infrastructure\` contains exactly two headers, `SimulationEngineClient.h` and `SimulationEngineHost.h`. A tree-wide search for the name returns nothing. **[B]'s own surface table cites it** as the answer to "What a run publishes". See R7 — this is a defect in the brief, and EXT-17 is not blocked by it.
- **The headless host binary exists.** `C:\N8RO\bin\n8ro-sim-app.exe` is present, alongside `n8ro-sim-local.exe` and three others.
- **One ordinary run really does contain two segments.** Reading the vendored fixture `contract/capture-atacama-air-defense-sample.n8rocap.jsonl`: 7 180 records, 2 `segment_open` / `segment_close` pairs. Segment 0 holds 6 924 samples spanning `sim_time_s` 0 → 200.05 and closes `scenario_unloaded`; segment 1 holds 21 samples, **every one stamped 0.0**, and closes `host_lost`. A statistic computed over the file without segmenting it is wrong.
- **The frozen-clock test from the format spec works, exactly as specified.** Maximum samples for any one `(entity, occupancy)` at a single `sim_time_s`: **1 in segment 0, 11 in segment 1**. The test is exact, not a heuristic, and it is what lets a comparison exclude the unalignable segment.
- **Occupancy 2 is real and common.** The fixture carries 132 `entity_add` records: 90 at occupancy 1 and **42 at occupancy 2** — the whole roster re-created under the same names by the teardown reload. Keying on name alone silently merges two different bodies.
- **`contract/` has drifted again, and this PRD found it.** `PROVENANCE.md` states the pin is at *"producer `0.8.0`"* and explains that the previous pin went stale within the hour because 0.8.0 added `header.sample_form`. The vendored fixture's header reports **producer `0.5.0` and carries no `sample_form` key at all.** The file is still a valid `n8ro-capture/1` capture — adding a key is non-breaking under the format's own §13 — but a reader tested only against this fixture never meets the newest header key. See R4.
- **Capture volume, independently sanity-checked.** The fixture is 3.34 MB for 7 180 records — about **465 bytes per record**. EXT-08 reports roughly **64 MB per 200-second run** for the 42-entity reference scenario [C2]; at 42 entities × ~4 000 frames × 465 B that lands in the same order of magnitude. A 20-run campaign on unbounded captures is therefore a **~1.3 GB** disk commitment. See R3.

**Established by EXT-08 and inherited rather than re-derived [C1] [C2]:**

- **A byte-for-byte determinism gate cannot pass on this platform, and the simulation is still reproducible.** Two runs of the reference scenario on the shipped headless host, each stopped at **exactly frame 1200** so both cover the same simulation: byte comparison **fails** (the files differ, and differ in length); content comparison per `(entity, occupancy)` aligned on `sim_time_s` over running segments finds **50 358 samples compared, 50 358 agreeing, zero differing**. The runs disagree only about *which frames were published at all* — 83 samples across 4 frames of ~1 198, about **0.2%**. The wall-clock-paced `n8ro-sim-local` is worse, at ~1%. **The simulation is reproducible; its publication schedule is not.** This is the single most consequential inherited fact in this document; see ADR-1 and CR-DET-1.
- **The isolation rule already has its proof, and it cost a milestone to buy.** EXT-08's shutdown harness first tried sharing one host across twenty cycles. Every cycle after the first attached mid-run and recorded nothing but orphaned samples — twenty runs, one usable. That is [B]'s isolation rule's *why*, already paid for. EXT-17 starts a fresh host per run and does not spend a milestone learning this. See CR-EX-1.
- **The recorder must start before the host.** The `entity_created` burst that fills the roster is published once, at scenario load. A recorder attached later records orphans. The capture says which happened (`header.attached_mid_run`, `trailer.drops.samples_orphaned`), but a campaign that gets the order wrong has collected nothing.
- **Twenty consecutive unattended load-run-teardown cycles have been measured clean** — host and bridge both exit 0 every time, no `0xC0000005`. EXT-17 needs 20+ unattended runs, so its single biggest platform risk is already measured. **The closure is scoped**: it says nothing about the plugin-loaded configuration that produced the original access-violation observation, which is not a configuration EXT-17 uses. See R6.
- **A capture is a very high-fidelity sample of the published stream, not a guaranteed-complete transcript, and no counter reports the difference.** Loss has been measured with every platform counter reading zero — 30 samples in a single frame, 0.023%. It is frame-shaped, it is *not* driven by rate (three times the message rate produced a complete capture), and the host's own in-process dump loses whole frames too, so it bounds completeness from one side only and is not ground truth. **This is not a footnote. It determines what an assertion is allowed to conclude** — see CR-AS-4 and ADR-6.

**Two lessons about how the upstream PRD was written, adopted here as process constraints [C2]:**

1. **Effort is not written as fact.** EXT-08's PRD reported "9–10 working days", marked milestones "delivered, inside budget", and called a hypothesis "validated by milestone burn-down" — its own rev-9 audit withdrew all of it, because no elapsed effort was ever recorded. Every day figure in this document is an **estimate**, labelled as one, and this project will not claim a measured cost unless it measures one.
2. **A table declared an authority must be kept true by something.** EXT-08's PRD declared a CLI table "the source of truth"; it listed an option the binary has never accepted and omitted four that ship, and a post-delivery audit caught it. §"Naming and interface conventions" below states, for each authority table, **the mechanism that keeps it true** — see CR-DOC-1.

---

## Goals and success metrics

### Goals

- **G1 — Many runs, no human.** A campaign of twenty or more runs executes start to finish unattended, and the failure of any one run does not end the campaign.
- **G2 — A result you can act on.** Every run lands in the report as exactly one of four outcomes, with infrastructure failure never counted as a test result, and a failure carries enough detail to go and look.
- **G3 — A varied input produces a visible trend.** One parameterisation axis, done properly, sweeps across a campaign and the result's dependence on the parameter is legible.
- **G4 — Determinism is proven, not assumed, and proven every time.** The tool checks for itself that the same configuration twice produces the same result, on a comparison whose passing means what it claims to mean.
- **G5 — A stored run is re-judgeable.** Assertions are declared outside the code and evaluate against stored captures, so a new question about an old campaign costs no re-run.

### Success metrics

> **Baselines are all "does not exist" because EXT-17 does not exist.** That is stated rather than dressed up. Where a target number comes from an inherited measurement rather than from this project, the source column says so.

| Metric | Baseline (current) | Target | How measured | Timeline |
|---|---|---|---|---|
| Unattended campaign length | 0 — no runner exists | ≥ 20 runs, start to finish, zero manual steps | Campaign exit code and run count in the report | M6 |
| Runs surviving an injected infrastructure failure | n/a | Campaign continues; the failed run is reported as `infrastructure_error`, not `fail` | Fault-injection matrix (four faults, §"Validation and test plan") | M6 |
| Determinism self-test, content comparison | n/a | 100% of compared samples agree, across running segments only. **Inherited reference point: 50 358 / 50 358 on the headless host** [C1] | `compare` over two same-configuration captures | M4 — hard gate |
| Determinism self-test, byte comparison | n/a | **Expected to FAIL, and that is not a defect.** Recorded and explained, never used as the gate | Same pair of captures | M4 |
| Comparison-path variability introduced by us | n/a | Zero: no wall-clock value, no unordered iteration, no locale-dependent formatting anywhere in the compared output | Unit tests on the comparison path, run on every change | M4 |
| Sweep legibility | n/a | A result that varies with the parameter, presented so the trend is visible without further processing | Mentor review of the sweep report | M5 |
| Re-judgement without re-running | n/a | A stored campaign re-judged against a new condition file, producing verdicts, with no host started | Timed re-judge over the committed example campaign | M6 |
| Diff precision | n/a | The diff names the **first** point of divergence, not merely that two runs differ | Two deliberately-diverged runs from the sweep | M6 |
| Peak campaign disk footprint | n/a | Bounded and stated in the README, at two levels: **per capture** by the upstream recorder's `--capture-max-bytes`, defaulted to `61 000 × --frames`, and **per campaign** by CR-CAP-5's **8 GiB ceiling over the campaign directory** | **Measured at M3** (rev 3): 29 788 003 bytes per 1200-frame run — 24 823 per frame, of which 18.4% is host logs — so ~99 MB per 200 s run and ~1.99 GB for twenty. Supersedes the inherited ~64 MB/run [C2] and ~465 B/record [C3] estimates | M3 ✔ |

### Non-goals / deferred scope

Orientation only; the contract-level deferrals live in §"Out of scope".

- **Building a recorder.** [B] step 3 says *"Capture the run. Subscribe as in EXT-08."* EXT-08's capture format is frozen and vendored. EXT-17 consumes it. See ADR-2.
- **All three stretch goals** — parallel runs, regression baseline mode, per-failure evidence pages. [B] lists them as stretch.
- **Modifying anything under `C:\N8RO`.** That tree is read-only for this project.

## Out of scope

| Item | Status | Rationale | Target | Added |
|---|---|---|---|---|
| Parallel execution of several simulations | Deferred | [B] stretch goal. It multiplies the isolation risk that CR-EX-1 exists to close, and a concurrency bug in the campaign loop is indistinguishable in the report from a platform non-determinism finding — the exact confusion G4 exists to prevent. Bring it in only after the serial campaign and the self-test are both green. | Stretch, after M6 | rev 1 |
| Regression baseline mode ("report only what changed") | Deferred | [B] stretch goal. Cheap once CR-REP-4's diff exists, since a baseline is a stored campaign and a diff against it. Deliberately not in v1 so the diff is built for the run-to-run case [B] requires first. | Stretch, after M6 | rev 1 |
| Human-readable per-failure evidence report with trajectories | Deferred | [B] stretch goal. Requires a plotting or rendering dependency in a program whose whole appeal is that it links four import libraries. CR-REP-2 delivers the machine-readable half — the data a person needs to go and look — which is what [B] requires. | Stretch, after M6 | rev 1 |
| More than one parameterisation axis | Out of scope for v1 | [B] settles this itself: *"One axis done properly beats four done loosely."* A second axis is a PRD revision, not an implementation choice. | MVP+1 | rev 1 |
| A general expression language for conditions | Out of scope | A parser is a project of its own and a known scope explosion. v1 ships a closed vocabulary (CR-AS-3). A fourth condition kind requires a PRD revision — which is the point. | N/A | rev 1 |
| Writing into the scenario database under `C:\N8RO` | Out of scope | The install tree is read-only for this project. If the chosen parameterisation axis turns out to require authoring scenario variants in place, that is a blocking finding, not an implementation detail — see R9 and OQ-4. | N/A | rev 1 |
| Live-feed or externally-timed inputs during a campaign | Out of scope | [B] excludes them: *"Campaign runs are for the closed configuration."* | N/A | rev 1 |
| Cross-machine / distributed campaigns | Deferred | v1 assumes one workstation. Nothing in the design forbids it; nothing validates it either. | TBD, on first request | rev 1 |
| Fixing `contract/`'s drift ourselves | Out of scope | `contract/` is read-only from here. A defect in it goes back to EXT-08 rather than being worked around (R4). | N/A | rev 1 |

## Key hypotheses

- **H1 — The gate holds on content.** We believe **keying the determinism self-test on per-`(entity, occupancy)` value sequences aligned on `sim_time_s`, excluding frozen-clock segments**, will pass repeatably on the headless host, because the property the gate protects — that the same scenario produces the same simulation — was measured to hold exactly while the byte-level representation did not.
  *Signal: two same-configuration captures compare with zero differing samples, repeatedly, not once. Validated by: the M4 gate, re-run on every subsequent milestone.*
  *If false — two runs disagree on a value at the same simulation instant — that is the "something far more interesting" [B] warns about. It stops the project at step 4, goes to the mentor immediately, and goes into the determinism notes deliverable.*
- **H2 — A fresh host per run is sufficient isolation.** We believe **starting and tearing down a host process per run** will satisfy [B]'s isolation rule without any explicit cleanup logic, because a process boundary carries no cached handle and no still-running host by construction, and the upstream twenty-cycle teardown measurement found no residue.
  *Signal: run N's capture is byte-comparable in structure to run 0's — same segment count, same roster size, zero orphaned samples, `attached_mid_run: false`. Validated by: the 20-run campaign's per-run structural check.*
  *If false — state leaks across a process boundary, most likely through the filesystem or a stale shared-memory segment — isolation becomes explicit teardown-and-verify per run, and the campaign gets slower rather than wrong.*
- **H3 — End-of-run has a cheap, exact definition.** We believe **a stated stop predicate over the published stream — a frame budget being the leading candidate — will define "the run is finished" precisely enough to bound every run**, because the upstream determinism measurement was only possible by stopping two runs at exactly frame 1200, which demonstrates the predicate is both expressible and observable.
  *Signal: twenty runs each stop at the same simulation frame, and no run ends by timeout. Validated by: the M2 end-detection spike.*
  *If false, this is the project's largest schedule risk — [B] itself says it is "harder than it sounds". Containment is OQ-1: decide the definition at M2 against explicit criteria, with the run timeout (CR-EX-4) as the always-present backstop so that no run can hang the campaign regardless.*
- **H4 — Consuming the frozen format is cheaper than recording.** We believe **reading `n8ro-capture/1` and driving EXT-08's recorder as an external process** will cost days rather than the ~2–3 days the entity picture cost upstream, because the format is self-describing, frozen, and was demonstrated readable from its specification alone by a conformance reader that linked neither the bridge nor the SDK.
  *Signal: a conformant reader passes against the vendored fixture and rejects a mutated one. Validated by: M3's reader conformance suite.*
  ***Validated at M3.*** *The reader passes **78 checks** over four tiers: the vendored fixture untouched, with its own tally agreeing with `trailer.counts` exactly; five mutations each producing a distinct named error and a sixth adding keys it has never seen, which it must ignore; 17 synthetic micro-captures; and thirty real captures including all twenty of M2's. It links neither EXT-08 nor the SDK, checked by its own build. It cost well under a day against the ~2–3 days the entity picture cost upstream, so **H4 holds** — and the mechanism it credited, a specification sufficient on its own, held too, with one imprecision found and returned rather than worked around (R11, E-3).*
  *If false — the specification proves insufficient — that is a defect in EXT-08's contract and goes back there rather than being worked around, per `contract/PROVENANCE.md`'s own rule.*

## Tenets

Decision tie-breakers for ambiguous trade-offs during implementation — *unless you know better ones.*

1. **A wrong number is worse than no number.** A campaign that reports "20 passed" after quietly running nineteen, loading no conditions, or judging a truncated capture is the failure mode this project exists to prevent. When a choice is between reporting less and reporting confidently, report less, loudly.
2. **The four outcomes are never collapsed.** Pass, fail, timeout and infrastructure error stay four things at every layer — per run, in aggregate, in the exit code. Any code path tempted to turn "the host would not start" into "the scenario failed" is wrong by construction.
3. **Absence is not evidence.** The capture is a high-fidelity sample, not a transcript, and no counter reports the difference. An assertion that concludes something from a missing record is unsound, however clean the file looks.
4. **Isolation is enforced by the process boundary, not by remembering to clean up.** Anything that would survive a run is a design defect, not a cleanup task.
5. **Nothing of ours varies between runs.** No wall-clock value, no unordered iteration, no locale-dependent formatting anywhere in the compared output. [B] names all three; each is easy to reintroduce and silently invalidates the gate everything else rests on.

---

## Personas and access boundaries

### Campaign author — the person who runs the sweep
Writes a campaign configuration and a condition file, launches the campaign, and reads the report. Does not watch it.
**Access level:** Campaign config, condition files, the report, stored captures. Never needs to read EXT-17's source to know what an assertion checked — CR-AS-2 puts that in the failure itself.

### Analyst — the person asking why a run failed
Takes a failing run's identifier from the report and goes to the capture to look. Their entire contract is the report's failure detail plus `contract/capture-format-v1.md`.
**Access level:** Report, captures, condition file. Read-only.

### Mentor / reviewer
Reviews the deliverables, answers the questions this PRD records — chiefly the headless invocation (OQ-3) — and rules on, or forwards, the one escalation (OQ-2).
**Access level:** Everything, including the determinism notes and their unexplained observations.

### The upstream project — EXT-08 (service persona, not a person)
Supplies the frozen `n8ro-capture/1` format and captures. **It is not a dependency EXT-17 can change.** A defect in the contract goes back there as a defect; it is never worked around in EXT-17.
**Access level:** None into EXT-17. EXT-17 reads `contract/` only, and no EXT-08 source at all.

## Security posture and trust boundaries

> Included for completeness. EXT-17 introduces no authentication or authorization surface. The substantive concerns are **process control**, **disk**, and **data classification** — all three sharpened by the fact that this program starts and kills processes unattended, twenty times in a row.

**Trust boundaries.** The campaign runner, the recorder and the simulation host run as one user on one machine over the platform's local bus. There is no network listener and no remote input in v1. `C:\N8RO` is a read-only boundary: EXT-17 reads headers, reads docs and runs binaries there, and writes nothing.

**Enforcement model.** EXT-17 has exactly the privileges of the user who launched it. Nothing in this PRD authorizes a credential store, a listener, or a remote-control path.

| Threat | Impact | Mitigation |
|---|---|---|
| The campaign kills a process it did not start | An unrelated N8RO session is destroyed mid-work | Terminate only by the process handle the campaign itself created; never by image name. CR-EX-1's acceptance criteria state this. |
| An orphaned host survives a crashed campaign and poisons the next one | Every subsequent run attaches mid-run and records orphans — the exact twenty-wasted-cycles failure measured upstream | CR-EX-1: a run refuses to start if a host it did not create is live, and says so, rather than proceeding. |
| Disk exhaustion mid-campaign | Campaign lost, host possibly destabilised, and the report is a truncated file | Two controls that compose. **Per capture:** the upstream recorder's `--capture-max-bytes`, which closes a bounded capture with a well-formed trailer rather than a line cut in half. **Per campaign:** CR-CAP-5's ceiling and pre-flight free-space check, which is still ours because the upstream bound is per file (R3). |
| A campaign report or capture leaves the organisation | Disclosure of scenario design and platform behaviour | Captures and reports inherit the classification of the scenario they record. The committed example campaign uses a stock demo scenario only. |
| A traversal component in a run label writes outside the campaign directory | Files land outside the intended tree | Canonicalise the campaign output directory at startup; reject traversal components in run labels. |
| An exception escapes into a bus or platform callback | Undefined behaviour across a library boundary we do not own | Constraint C3: [B] says *"Never throw."* Failures are return values plus logging. |

---

## Functional requirements

Priorities: **P1** = required for v1 acceptance. **P2** = valuable, ship if budget allows.

### Naming and interface conventions

> EXT-17 exposes no REST API and no SDK. It exposes four surfaces that a campaign author binds to. This subsection is the authority for all four — and, per the lesson at §"Prior art", **each authority statement below names the mechanism that keeps it true.** An authority table nobody checks is worse than none.

**CLI surface.** Invocation is `n8ro-campaign [options]`. Long options only, kebab-case, GNU style.
**Authority: the binary's own `--help` output, checked into the repository as a golden file and compared by a test on every build.** This PRD deliberately does **not** enumerate the option list, because a prose list in a document nobody executes is exactly what drifted upstream. The FRs below name *capabilities* the CLI must expose; the golden file names the spelling.

**File and path conventions.**
- A campaign writes one directory per campaign, one subdirectory per run, containing that run's capture, its verdicts, and its per-run record.
- Run identifiers are **zero-padded ordinals** (`000`, `001`, …). **Never timestamps** — two identical runs must be addressable as a pair, and a wall-clock name makes them unaddressable and puts a varying value into a path the report cites.
- The campaign report and the per-run records are the only files EXT-17 authors; captures are authored by the recorder.

**Linkage boundary — three distinct answers, and [B]'s surface table points at two of them.**
- **The control path links the N8RO SDK.** CR-EX-2 requires publishing `load_scenario` on `sim/scenario/command` and `start` on `sim/engine/command`, which needs a bus client. [B]'s surface table names it: *"The client — control, subscribe, query | include\n8ro-sim\infrastructure\SimulationEngineClient.h"*, and *"Import libraries and headers | the release's lib\ and include\<module>\"*. This is the one place EXT-17 links the platform, and it is where [B]'s sibling project records most of the difficulty living — configuration, not logic [C2].
- **The capture reader links nothing** — neither the SDK nor EXT-08 (CR-CAP-2). It is pure file parsing against a frozen specification, which is what makes it testable with no install present.
- **Nothing anywhere links EXT-08** (constraint C8). The host and the recorder are driven as processes.

**Consumed format (authority: `contract/capture-format-v1.md`, frozen, read-only).**
- `format_version` pinned to **`n8ro-capture/1`**. The eight record types are a closed set. An entity's identity is `(entity, occupancy)`. **Kept true by:** the reader's conformance suite runs against the vendored fixture, and CR-CAP-3 makes any other version a named rejection.

**Condition-file surface (authority: this PRD's CR-AS-3 plus the file the repository commits).**
- The vocabulary is closed and every declared condition produces exactly one verdict. **Kept true by:** a parse test over the committed example, plus a negative test that an unrecognised kind is a named error and a non-zero exit.

### Execution (EX)

#### CR-EX-1 (P1): Every run gets a fresh host, and nothing crosses between runs
The system SHALL start a new simulation host process for each run, tear it down at the run's end, and carry **no** state from one run to the next — no file, no cached handle, no still-running host. A run SHALL refuse to start, with a named error, if a host process it did not itself create is already live.

**Provenance: [QUOTED].** [B]: *"Every run is isolated. No state carried from one run into the next — not a file, not a cached handle, not a still-running host. A campaign whose runs affect each other produces results nobody can interpret."*
*The refuse-to-start clause is stricter than [B]'s words and is **[ORIGINATED]** — decided by this project, because [B]'s rule says what must not be carried, not what to do when something already is. The reason is measured upstream: a harness that shared one host across twenty cycles produced one usable run and nineteen recordings of orphaned samples [C1] [C2]. Failing loudly is cheaper than a campaign of worthless captures.*

**Customer scenario:** A campaign author launches a twenty-run sweep on a machine where a previous crashed campaign left a host running, and gets an error in the first second rather than twenty meaningless results in an hour.
**Pain removed:** A campaign whose runs contaminate each other produces a report that is confidently wrong — the worst output this program can produce, because nothing in it looks broken.

**Acceptance criteria:**
- Each run starts a host process, and the campaign terminates only handles it created — never by image name.
- Run *N*'s capture is structurally independent of run *N−1*'s: `attached_mid_run` is `false`, `trailer.drops.samples_orphaned` is `0`, and the roster is complete from the run's first frame.
- A pre-existing host the campaign did not create produces a named error and a non-zero exit before any run starts.
- No file written by run *N−1* is read by run *N*, apart from the campaign report the campaign itself is accumulating.

**Trace:** UAC-CR-EX-1

#### CR-EX-2 (P1): The host is brought up and driven to a running scenario without a sleep
The system SHALL start the headless host, wait for it to be **ready** on an observed condition, load the scenario, and start it — and SHALL NOT use a fixed delay in place of any of those waits.

**Provenance: [QUOTED] for the sequence and the no-sleep rule.** [B] step 2: *"Automate one run: start the host, wait for it to be ready, load, run, detect the end, tear down"*, and *"make it explicit rather than a sleep."*
*The **specific invocation** is **[ORIGINATED]**: `n8ro-sim-app.exe --sim-config SimEngineHost_SharedMemory --model-path <dir> --schema-file <name>`, which takes **no scenario argument** — loading is a separate publish on `sim/scenario/command` and starting is `{"command":"start"}` on `sim/engine/command`. Established by observation inside EXT-08 [C1], verified only to the extent that this project confirmed the binary exists [C3]. **[B] does not specify it. [B] says: "Confirm the invocation with your mentor" — that confirmation is still owed, and is OQ-3.** What is inherited is that this invocation works, not that it is the intended production shape.*

**Customer scenario:** The campaign author's twenty-run sweep does not lose four minutes per run to conservative sleeps, and does not fail intermittently on a slow morning because a sleep was tuned on a fast one.
**Pain removed:** A sleep is either too short — and the run starts against a host that is not ready, producing a mid-run attach and a worthless capture — or too long, and a twenty-run campaign spends most of its wall-clock time waiting. [B] rejects it by name.

**Acceptance criteria:**
- Readiness, scenario-loaded and started are each established by an observed condition, and each has a bounded, logged timeout.
- No fixed delay appears anywhere in the run-start path. A code search for sleep primitives in that path returns nothing.
- The recorder is attached **before** the host publishes its entity-creation burst; the resulting capture has `attached_mid_run: false`. *(Ordering rationale is [ORIGINATED] from [C1] finding 7: the roster burst is published once, at scenario load, and a recorder attached later records only orphans.)*
- A host that never becomes ready is an `infrastructure_error` outcome (CR-EX-5), never a `fail`.

**Trace:** UAC-CR-EX-2

#### CR-EX-3 (P1): "The run is finished" is an explicit, stated definition
The system SHALL determine that a run has ended by an explicit predicate over what the run published, SHALL state that predicate in the README and in every per-run record, and SHALL NOT infer the end from elapsed wall-clock time.

**Provenance: [QUOTED] for the explicit-definition rule; [ORIGINATED] for the wall-clock prohibition.** [B] step 2: *"Getting 'is it finished' right is harder than it sounds — decide what defines the end and make it explicit rather than a sleep."*
*The **"SHALL NOT infer the end from elapsed wall-clock time" clause is stricter than [B]'s words and is [ORIGINATED]** — decided by this project. [B] prohibits **a sleep**; a wall-clock *budget* is not a sleep and would satisfy [B] as written. The prohibition is widened here because a wall-clock-bounded run breaks CR-DET-1: two runs stopped after the same duration have not covered the same simulation, and their captures are then guaranteed to differ for a reason unrelated to determinism [C1]. [B]'s rule 5 (*"not your own clock"*) and its determinism hazard list (*"a value read from a clock"*) point the same way but govern record stamping and the comparison path, not the end-of-run decision — so they support the widening without authorising it. Recorded rather than laundered, because a requirement stricter than the brief that later proves unmeetable is exactly what cost the upstream project two revisions [C2].*
*The predicate itself is **not yet chosen**, and this PRD does not pretend otherwise — it is **OQ-1**, with a decision target of M2 and explicit selection criteria. Promoting it to a rev-1 open question is **[ORIGINATED]**: [B] flags the difficulty, EXT-08's PRD explicitly refused the problem (*"resist implementing end-detection here. It is EXT-17's problem"* [C2]), and nobody has solved it. It belongs in front of a reviewer now, not discovered at milestone 2.*

**Customer scenario:** The campaign author can say, in one sentence, what "finished" meant for every run in a report they are about to show someone.
**Pain removed:** An implicit end makes every cross-run comparison meaningless: two runs stopped at different points have not covered the same simulation, and their captures are then guaranteed to differ for a reason that has nothing to do with determinism. **This is not hypothetical — it is precisely what made the upstream determinism result measurable at all** [C1].

**Acceptance criteria:**
- The predicate is stated in the README in one sentence, and the value it evaluated to is recorded in each per-run record.
- No wall-clock quantity participates in the decision. The run timeout (CR-EX-4) is a **backstop, not the definition**, and a run that ends by timeout is reported as `timeout`, never as a completed run.
- Twenty runs of one configuration end at the same point by the predicate's own measure, and the per-run records show it.

**Trace:** UAC-CR-EX-3

#### CR-EX-4 (P1): A timeout on every run, and it is its own outcome
The system SHALL apply a configurable timeout to every run, and a run that hits it SHALL be reported as a **distinct outcome** from both a pass and a failure.

**Provenance: [QUOTED].** [B] rule 2: *"A timeout on every run, and a run that hits it is a distinct outcome from a failure and from a pass. Three states minimum, never two."*

**Customer scenario:** A run that never ends costs the campaign one run, not the whole night.
**Pain removed:** Without a timeout, [B]'s own ugly-reality list — *"a run that never ends"* — hangs an unattended campaign indefinitely. Without a distinct outcome, it is silently miscounted as a failure and someone goes looking for a scenario bug that does not exist.

**Acceptance criteria:**
- Every run has a timeout; there is no configuration in which a run may run unbounded.
- A timed-out run is torn down, its partial capture retained and marked, and it appears in the report as `timeout`.
- The report's counts satisfy `pass + fail + timeout + infrastructure_error = runs attempted`, with no run in two categories and none in none.

**Trace:** UAC-CR-EX-4

#### CR-EX-5 (P1): An infrastructure failure is never a test result
The system SHALL classify every run into exactly one of four outcomes — **pass, fail, timeout, infrastructure error** — and SHALL NOT report a failure of the harness, the host, or the scenario load as a failing scenario.

**Provenance: [QUOTED].** [B] rule 3: *"Never let an infrastructure failure count as a test result. A host that would not start is not a failing scenario. Report the categories separately or the summary is worthless."* [B] acceptance criterion 5: *"Pass, fail, timeout and infrastructure error are four distinct outcomes in the report."*

**Customer scenario:** The campaign author reads "3 failed" and knows three scenarios behaved wrongly — not that two hosts would not start.
**Pain removed:** [B] states the consequence itself: the summary is worthless. A campaign that mixes the categories cannot distinguish a regression from a flaky workstation, which is the entire question a campaign exists to answer.

**Acceptance criteria:**
- Host-start failure, scenario-load refusal, host death mid-run, recorder failure, and an unreadable or structurally unsound capture are all `infrastructure_error`.
- A run is `fail` only when the capture was read successfully **and** a declared condition was evaluated and not met.
- A run whose capture reports non-zero `trailer.drops.events_not_recorded` is `infrastructure_error`, not `fail`. *([ORIGINATED], from `contract/capture-format-v1.md` §11: a non-zero value there means the file's **structure** is incomplete, so an occupancy may be missing the record that opened it and a verdict computed over it is not trustworthy.)*
- The campaign's exit code distinguishes "ran, some failed" from "could not run".

**Trace:** UAC-CR-EX-5

#### CR-EX-6 (P1): The four ugly realities are handled states, and the campaign continues
The system SHALL survive, without ending the campaign: a host that fails to start; a scenario that refuses to load; a run that never ends; and a host that dies mid-run. In each case it SHALL record what happened and proceed to the next run.

**Provenance: [QUOTED].** [B] step 8: *"Handle the ugly reality: a host that fails to start, a scenario that refuses to load, a run that never ends, a host that dies mid-run. A campaign of a hundred runs will meet all four."* [B] acceptance criterion 8: *"A host crash mid-campaign is survived: the campaign continues and the report says what happened."*

**Customer scenario:** The campaign author arrives in the morning to twenty results, one of which says a host died at run 11 and why, rather than to a campaign that stopped at run 11.
**Pain removed:** A campaign that aborts on the first infrastructure fault is not unattended. [B] says all four will happen in a hundred runs; at twenty runs they are likely rather than certain, which is worse — it is the configuration in which the bug ships undetected.

**Acceptance criteria:**
- Each of the four faults is exercised deliberately, by injection, and each produces a named `infrastructure_error` record and a continued campaign.
- No fault leaves a host process running (CR-EX-1) or a partially-written per-run record.
- The report names which fault occurred, on which run, at what point in the run's lifecycle.

**Trace:** UAC-CR-EX-6

#### CR-EX-7 (P1): Twenty or more runs execute unattended
The system SHALL execute a campaign of at least twenty runs from launch to report with no manual step.

**Provenance: [QUOTED].** [B] acceptance criterion 1: *"A campaign of at least twenty runs executes unattended, start to finish, with no manual step."*

**Customer scenario:** The campaign author types one command and comes back to a report.
**Pain removed:** A "campaign" needing a keystroke per run is a watched run with extra steps, and produces demonstration rather than evidence — the distinction [B] opens with.

**Acceptance criteria:**
- One command; no prompt, no dialog, no manual host start, no manual teardown.
- The 20-run campaign is the committed example deliverable (CR-DOC-2), so the criterion is checkable by re-running it.
- **Platform feasibility is inherited, not assumed:** twenty consecutive load-run-teardown cycles have been measured clean upstream, scoped to the plugin-free configuration [C1] [C2]. See R6.

**Trace:** UAC-CR-EX-7

### Capture and re-judgement (CAP)

#### CR-CAP-1 (P1): Recording is separate from assertion, and a stored run is re-judgeable
The system SHALL keep the recording of a run separate from the evaluation of conditions over it, and SHALL evaluate a condition file against a **stored** capture without re-running the simulation.

**Provenance: [QUOTED].** [B] step 3: *"Keep the recording separate from the assertions so a stored run can be re-judged later."* [B] acceptance criterion 7: *"Stored runs can be re-judged against new assertions without re-running."*

**Customer scenario:** The analyst thinks of a new question a week after the campaign ran, and answers it against the stored captures in seconds.
**Pain removed:** [B] gives the reason: *"that is what makes a campaign cheap to iterate on."* If a new assertion costs a re-run, the campaign is a one-shot experiment rather than an asset.

**Acceptance criteria:**
- A re-judge mode takes a stored campaign directory and a condition file and produces verdicts with **no host started and no bus subscription made**.
- Verdicts produced by re-judging a stored capture are identical to those produced during the live run over the same conditions.
- Nothing in the assertion path can start a host, load a scenario, or write into a capture.

**Trace:** UAC-CR-CAP-1

#### CR-CAP-2 (P1): A conformant reader for `n8ro-capture/1`, written from the specification alone
The system SHALL read `n8ro-capture/1` captures using only `contract/capture-format-v1.md` as its specification, without linking or reading EXT-08 source.

**Provenance: [ORIGINATED].** [B] says only *"Capture the run. Subscribe as in EXT-08"* (step 3). That authorises capturing in EXT-08's manner; it does **not** name a format, a version, or a boundary rule. **The decision to consume the frozen `n8ro-capture/1` artifact rather than build a second recorder was made by this project** — recorded in ADR-2 — because the format is frozen, vendored, and was demonstrated readable from its specification alone by a conformance reader that linked neither the bridge nor the SDK [C1]. Naming this [QUOTED] would be laundering a project decision into a client requirement.

**Customer scenario:** The EXT-17 implementer builds against a stable, documented artifact, and a defect in it is a defect someone else owns and fixes.
**Pain removed:** The two projects are separate repositories with no shared source. Without a documented versioned artifact crossing that boundary, the only available contract is EXT-08's source — the exact coupling the repo split exists to prevent.

**Acceptance criteria:**
- The reader links neither EXT-08 code nor the N8RO SDK, and no EXT-08 identifier appears in EXT-17's source.
- It parses the vendored fixture completely, and its own record counts agree with `trailer.counts`. *(Verified re-derivable [C3]: 7 180 records — 6 945 samples, 132 adds, 90 removes, 7 verdicts, 2 segment pairs — matching the trailer exactly.)*
- It survives deliberate mutation of the fixture: a truncated file, a malformed line, a `fields` key the schema does not declare, and a count that disagrees with the trailer each produce a named error rather than a silent misparse.
- Unknown keys in a known record type are ignored, per the format's §13 non-breaking rule — so a producer newer than the fixture still reads.

**Trace:** UAC-CR-CAP-2

#### CR-CAP-3 (P1): An unrecognised `format_version` is a named rejection, never a partial parse
The system SHALL check `format_version` before parsing further, and SHALL reject a capture whose version it does not implement with a named error and a non-zero outcome, without attempting a partial parse.

**Provenance: [ORIGINATED].** [B] says nothing about format versions — the word does not appear in it. This requirement comes from `contract/capture-format-v1.md` §3 and §13 and from this project's own repository conventions [C1]. **Decided by this project** on the reasoning that partial parsing of an unknown format is how silently-wrong analysis happens, and a campaign runner's whole output is analysis.

**Customer scenario:** After an EXT-08 upgrade, the campaign author is told the capture format changed — instead of getting a report whose numbers are quietly wrong.
**Pain removed:** A reader that guesses at an unknown version produces a plausible campaign report from a file it does not understand. Tenet 1: a wrong number is worse than no number.

**Acceptance criteria:**
- `format_version` is read from the first key of the first line and checked before any other parsing.
- A version other than `n8ro-capture/1` produces a named error naming both the expected and the found version, and the run is `infrastructure_error`.
- A test feeds a capture with a bumped version string and asserts the rejection.

**Trace:** UAC-CR-CAP-3

#### CR-CAP-4 (P1): Segment-aware and occupancy-aware reading
The system SHALL treat a capture as a sequence of segments, SHALL key entity identity on `(entity, occupancy)` rather than on name, and SHALL NOT compute any cross-run statistic over a whole capture without segmenting it.

**Provenance: [ORIGINATED].** [B] raises none of this — it has no concept of segments or occupancies. The requirement comes from `contract/capture-format-v1.md` §5.1, §8.1 and §16 [C1], **and was independently confirmed by this project against the vendored fixture** [C3]. Decided by this project because every statistic the reporting and assertion requirements produce is wrong without it.

**Customer scenario:** The campaign author's per-run duration, sample count and trajectory are computed over the run — not over the run plus the teardown reload that the engine appends to every capture.
**Pain removed:** Three concrete errors, each measured in the fixture this project read [C3]: (a) **one ordinary run contains two segments** — 6 924 samples in segment 0 spanning `sim_time_s` 0 → 200.05, and 21 in segment 1 all stamped 0.0; (b) a segment's duration computed from its boundary records is **zero**, because both `segment_open` and `segment_close` read 0.0 on a reloaded scenario; (c) **42 entities re-appear at occupancy 2** under names already used, so keying on name merges two different bodies.

**Acceptance criteria:**
- Every statistic, verdict and comparison is scoped to a segment, and the segment it used is named in the output.
- Entity identity is the pair everywhere it is used; a test asserts that a name re-created at a higher occupancy is treated as a distinct entity.
- Frozen-clock segments are detected by the format's **exact** test — in a running segment the maximum number of samples any one `(entity, occupancy)` carries at a single `sim_time_s` is 1 — and are excluded from any cross-run comparison. *(Confirmed against the fixture [C3]: 1 in segment 0, 11 in segment 1.)*
- No code path sorts a capture by `sim_time_s` globally.

**Trace:** UAC-CR-CAP-4

#### CR-CAP-5 (P2): The campaign bounds its own disk usage, because the upstream bound is per file
The system SHALL enforce a configurable campaign-level disk ceiling, SHALL check free space before starting, and SHALL stop the campaign with a named outcome rather than filling the disk.

**Provenance: [ORIGINATED].** [B] does not mention disk, size or storage. **Decided by this project** because a 20-run campaign at roughly 64 MB per 200-second run [C2] is a ~1.3 GB commitment, and nothing upstream bounds a *campaign*. This project's own measurement puts the fixture at ~465 bytes per record [C3], consistent with that figure.

*(Rev 2: this previously read "because the upstream byte-limited capture requirement is **specified but unbuilt** — no rotation, no size ceiling in the header". That was true at rev 1 and is not now: EXT-08 producer 0.9.0 ships `--capture-max-bytes`, an `--on-size-limit stop|rotate` choice, and both stated in `header.limits` [C2]. **The requirement stands, on a narrower and more honest footing.** The upstream control bounds **one capture file**; a campaign is twenty of them, and a per-file bound multiplied by twenty is not a campaign bound. What has changed is that this requirement is no longer a lone mitigation for someone else's gap — it composes with an upstream control this project can configure per run, and the acceptance criteria below now say so.)*

**Customer scenario:** An overnight campaign that would exhaust the disk stops with a report of what it completed, rather than leaving a truncated capture and a destabilised workstation.
**Pain removed:** [B] requires the campaign to survive infrastructure faults. Disk exhaustion is the one infrastructure fault that also corrupts the evidence, because a capture cut off mid-line is unparseable by every reader.

**Acceptance criteria:**
- A pre-flight check refuses to start a campaign whose projected footprint exceeds available space, naming both numbers.
- Reaching the ceiling mid-campaign stops the campaign with a named outcome; completed runs and the report remain valid and readable.
- The ceiling and the projection method are stated in the README (CR-DOC-1).
- **Each run is given a per-capture byte bound**, passed to the recorder, so that a run which overruns its projection yields a closed, valid, explicitly-truncated capture (`end_reason: "size_limit"`) rather than one cut off mid-line. The value used and the `stop`-or-`rotate` action are stated in the README and are recoverable from each capture's own `header.limits`. *(Rev 2 [ORIGINATED], from the upstream capability landing. Which action to configure is OQ-6.)*
- **A run whose capture rotated is read as the set it is.** If `rotate` is configured, a run's capture may be several `.partNNN` files linked by `header.continues_from` and `trailer.continued_in`; the reader stitches them per the format's §6.7 rules, and — because segment ordinals restart in each part — every per-segment statistic keys on `(part, segment)`. *(This is a consequence of choosing `rotate` and is a reason OQ-6 might choose `stop`: `stop` keeps one file per run and needs none of this.)*
- **Rev 3, from M3's measurement — the ceiling is `8 GiB` and is measured over the campaign directory, captures and logs together.** The projection is `--bytes-per-frame`, default 25 400, against a measured 24 823. *A ceiling over captures alone under-states a campaign by 22.6% and would let a pre-flight check pass on a campaign that then exhausts the disk on its host logs, which is the same failure this requirement exists to prevent by a different route.*
- **Rev 3 — the configured per-run action is `stop`** (OQ-6), and `--capture-max-bytes` defaults to `61 000 × --frames` so that every run carries a bound and no normal run reaches one. A run that does reach it produces a capture that is complete, valid and **short of its run**, and the run record says so in `capture.covers_whole_run` — measured at M3 at `sim_time_s` 19.5 of 60.0. *The `rotate` clause above stands unchanged: the reader implements it and is tested against a real rotated set, because a capture rotated elsewhere still has to be readable here.*

**Trace:** UAC-CR-CAP-5

### Determinism (DET)

#### CR-DET-1 (P1): The same-configuration self-test, keyed on content, run every time

> **⚠ NAMED DEVIATION FROM [B]. Read this requirement's provenance before implementing it, and see ADR-1 and OQ-2.**

The system SHALL, before it operates as a campaign runner, execute the same configuration twice, capture both runs, and compare them — and SHALL run that self-test on every campaign, not once. The comparison SHALL be over **per-`(entity, occupancy)` value sequences aligned on `sim_time_s`, across running segments only**, and SHALL report the comparison as passing only when every compared sample agrees. A byte comparison SHALL be performed and reported **alongside** it, and SHALL NOT be the gate.

**Provenance: [QUOTED] for the self-test, its position and its recurrence; [ORIGINATED] for the comparison being on content.**

[B] step 4: *"Prove determinism — the same-configuration self-test above. **Do not build further until it passes.**"*
[B] paragraph 16: *"run the same configuration twice, capture both, and **show they match**. Keep that as a self-test you run every time, not as something you checked once."*
[B] acceptance criterion 2: *"The same-configuration self-test passes: **two identical runs produce identical captures**, and the tool checks this itself."*

**What [B] does not say, checked directly:** the words *byte*, *hash*, *bytewise* and *bit* **do not appear anywhere in [B]** — verified by full-text search of the extracted document [C3]. [B] says "identical captures" and "show they match" without stating at what level of representation. A byte reading is the strictest available reading and is defensible; **this PRD adopts the content reading, and adopting it is a decision this project made, not a requirement the client wrote.** That is why this requirement is marked [ORIGINATED] on its central clause.

**Why:** measured on the shipped headless host, two runs each stopped at exactly frame 1200 — **byte comparison fails** (the files differ, and differ in length); **content comparison passes completely**, 50 358 samples compared, 50 358 agreeing, zero differing. The runs disagree only about which frames were published at all, about 0.2% of them, differently each run [C1]. **The simulation is reproducible; its publication schedule is not.** A byte gate would fail every time while reporting the publication schedule rather than the simulation — and, because [B] makes step 4 a hard stop, would halt the project at milestone 4 permanently.

**Status of the deviation:** this is **not** a decision the implementer may make alone. It is escalated as **OQ-2**, with a decision target of *before M4 opens*, and the alternatives rejected are recorded in ADR-1. The measurement is inherited from [C1]; **this project will reproduce it as M4's first act** rather than taking it on trust, because a gate justified by someone else's number is a gate nobody here has checked.

**Customer scenario:** The campaign author sees, at the top of every campaign report, that the platform produced the same simulation twice today — and if it did not, learns that before reading a single result.
**Pain removed:** [B] states the stake: *"without it there is nothing stable to assert against and a diff between two runs is noise."* A gate that cannot pass is worse than no gate, because it trains everyone to skip it.

**Acceptance criteria:**
- The self-test runs at the start of every campaign and its result appears in the report. A campaign whose self-test fails does not report run results as trustworthy.
- The comparison aligns per `(entity, occupancy)` on `sim_time_s`, compares **sequences** rather than treating `sim_time_s` as a key, and **excludes frozen-clock segments** by the exact test in CR-CAP-4.
- Both runs are bounded by the **same stop predicate** (CR-EX-3), not by the same wall-clock duration. *(Two runs stopped after the same number of seconds have not covered the same simulation, and their captures are then guaranteed to differ for a reason that has nothing to do with determinism [C1].)*
- The byte comparison is run and reported as an observation, with its expected-to-fail status stated, so the difference between the two readings stays visible rather than being quietly dropped.
- A capture with non-zero `trailer.drops.samples_not_recorded` is **not** compared; it is already an incomplete record of its run and the self-test says so instead of diffing it.
- **Result equality, not only capture equality.** The two self-test runs SHALL produce the **same verdicts and the same run outcome**, and the self-test SHALL report that separately from the capture comparison. *[QUOTED] — [B] paragraph 9: *"Run the same configuration twice and show that the results are identical"*. This is a distinct check from capture content: it is what a campaign author actually depends on, it is decidable even where the capture comparison is not (a frozen-clock segment carries no verdicts), and a disagreement here is more serious than a byte difference — it means the same configuration was judged two different ways.*
- The pass is reproducible: the self-test passes on repeated invocations, not once.

**Trace:** UAC-CR-DET-1

#### CR-DET-2 (P1): Nothing in our comparison path varies between runs
The system SHALL introduce no run-to-run variation of its own into anything it compares — no value read from a clock, no timestamp in compared output, and no unordered container iterated on a path that produces compared output.

**Provenance: [QUOTED].** [B] rule 4: *"Determinism first. Nothing in your comparison path may vary between identical runs."* [B] names the three hazards itself: *"Anything of yours that varies between runs — a timestamp in the compared output, an unordered container iterated, a value read from a clock."*

**Customer scenario:** When the self-test fails, the campaign author knows the finding is about the platform — because the harness has been ruled out by test, not by assertion.
**Pain removed:** Any one of the three hazards makes CR-DET-1 fail for a reason inside this project, which then reads as a platform defect and sends someone to investigate the wrong system.

**Acceptance criteria:**
- Each of the three hazards [B] names has a dedicated unit test, run on every change touching the comparison path — not once at the end.
- The locale hazard is tested under a comma-decimal locale, since number formatting is silently locale-dependent in the obvious implementations. *([ORIGINATED] specificity, from [C1]'s record that this machine actually has such a locale.)*
- Run identifiers in compared output are ordinals, never timestamps (§"Naming and interface conventions").

**Trace:** UAC-CR-DET-2

#### CR-DET-3 (P1): A failed self-test distinguishes a harness defect from a platform finding
WHEN the self-test fails, the system SHALL report enough to tell the two cases apart: the **first** differing record, whether the headers and record counts agree, and which segment and `(entity, occupancy)` the divergence falls in.

**Provenance: [QUOTED].** [B] paragraph 16: *"If it ever fails, you have found either a defect in your harness or something far more interesting, and **you must be able to tell which**."*

**Customer scenario:** A self-test failure at 3 a.m. leaves the campaign author a report that says where and in what shape the two runs parted, not merely that they did.
**Pain removed:** "The captures differ" is not actionable. The attribution rule is known and cheap: identical headers with divergence deep in the sample stream points at the publisher; a difference in the header or in a counter points at the recorder [C1].

**Acceptance criteria:**
- The failure report names the first differing record and its position, the header agreement, and the record-count comparison.
- Divergence is attributed to a segment and an `(entity, occupancy)`, never merely to a line number.
- Anything the self-test cannot explain goes into the determinism notes deliverable (CR-DOC-2) rather than being smoothed over — [B] asks for exactly that.

**Trace:** UAC-CR-DET-3

### Parameterisation (PAR)

#### CR-PAR-1 (P1): One parameterisation axis, done properly
The system SHALL vary **one** input across the runs of a campaign, declared in the campaign configuration rather than in code, and SHALL record the value used in each run's per-run record.

**Provenance: [QUOTED].** [B]: *"Parameterisation — vary the run: initial positions and velocities, which entities are present, which scenario from the catalogue. **One axis done properly beats four done loosely.**"* [B] step 5: *"Add one parameterisation axis and run a sweep across it."*
*Which of the three axes [B] offers is **not settled here** — it is OQ-4, with a decision target of M5 and explicit criteria, because the choice depends on whether an axis can be varied without writing into the read-only install tree (R9).*

**Customer scenario:** The campaign author changes one line of campaign configuration and gets a twenty-point sweep across it.
**Pain removed:** An axis hard-coded in the runner means every new question is a rebuild. [B]'s own warning is the sharper pain: four loose axes produce a campaign whose results cannot be attributed to any of them.

**Acceptance criteria:**
- The axis and its values are declared in the campaign configuration; changing them requires no rebuild.
- Each run's parameter value appears in its per-run record and in the report.
- Two runs with the same parameter value are identical configurations, and are therefore valid inputs to CR-DET-1's self-test.
- **[ORIGINATED]** IF the axis chosen at OQ-4 is *"which scenario from the catalogue"*, THEN the campaign SHALL enumerate scenarios through the platform's own catalogue queries rather than a hand-maintained list. *([B]'s surface table names the surface — *"Scenario and database catalogue | the client's catalogue queries — the list of databases and the scenarios in one, answered asynchronously"* — and flags that the answers are **asynchronous**, which makes enumeration a bring-up step with its own wait rather than a lookup. [B] does not require this requirement; it is this project's, and it exists so the axis's cost is visible before it is chosen.)*

**Trace:** UAC-CR-PAR-1

#### CR-PAR-2 (P1): A sweep whose trend is visible
The system SHALL present a sweep's results so that the dependence of the result on the parameter is visible without further processing.

**Provenance: [QUOTED].** [B] acceptance criterion 3: *"A parameter sweep shows a result that varies with the parameter, presented so the trend is visible."*

**Customer scenario:** The campaign author opens the report and sees where the behaviour changes, rather than exporting twenty numbers into a spreadsheet first.
**Pain removed:** [B]'s opening claim is that a hundred runs *"tell you where the behaviour changes"*. A report that requires post-processing to reveal that has not delivered it.

**Acceptance criteria:**
- The sweep output orders runs by parameter value and shows the per-run result against it.
- The presentation is legible in the report's own format — a reviewer can see the trend without opening another tool.
- At least one condition in the committed example campaign actually changes outcome across the sweep, so the trend is real rather than a flat line. *([ORIGINATED] — [B] requires a varying result; this criterion makes the deliverable prove it.)*

**Trace:** UAC-CR-PAR-2

### Assertion (AS)

#### CR-AS-1 (P1): Conditions are declared outside the code
The system SHALL evaluate conditions declared in a file separate from the code that runs the simulation, and SHALL reject a malformed condition file with a named parse error and a non-zero exit **before any run starts**.

**Provenance: [QUOTED] for the separation.** [B] acceptance criterion 4: *"Assertions are declared separately from the code that runs the simulation."*
*The **fail-before-anything-starts** clause is stricter than [B]'s words and is **[ORIGINATED]** — decided by this project, adopting the rule stated in `contract/condition-file-schema.md` [C1]: a campaign that reports "all passed" because it quietly loaded nothing is the failure the rule exists to prevent. [B] does not ask for it.*

**Customer scenario:** The campaign author fixes a typo in a condition file in ten seconds, before twenty runs execute against it.
**Pain removed:** A condition file loaded silently as empty produces a campaign of twenty confident passes that checked nothing — a wrong number where no number would have been safer (tenet 1).

**Acceptance criteria:**
- Conditions live in their own file; no condition is expressed in EXT-17's source.
- A malformed file, a duplicate condition id, and an unrecognised condition kind each produce a distinct named error and a non-zero exit before any host is started.
- A campaign never runs with zero conditions loaded unless zero conditions were declared, and that case is reported explicitly rather than as a pass.

**Trace:** UAC-CR-AS-1

#### CR-AS-2 (P1): Every verdict says what was checked, on what data, and why it failed
The system SHALL emit exactly one verdict per declared condition per run — including an explicit **not-met** verdict for a condition that was never satisfied — and each verdict SHALL name the condition, the entities and occupancies involved, the simulation time, and the values that decided it.

**Provenance: [QUOTED].** [B] acceptance criterion 4: *"a failure names what was checked and on what data."* [B] step 6: *"Each assertion says what it checked, on what data, and why it failed."*
*The **one-verdict-per-condition rule and the explicit not-met verdict** are **[ORIGINATED]** — adopted from `contract/condition-file-schema.md` and `capture-format-v1.md` §10 [C1]. [B] requires that a failure be explained; it does not specify verdict cardinality. The reason to adopt it: without an explicit not-met verdict, a condition that was evaluated and never satisfied is indistinguishable from one nobody evaluated.*

**Customer scenario:** The analyst reads a failing verdict and goes straight to the two records in the capture that caused it, without re-running anything.
**Pain removed:** "Condition X failed" sends someone back to the capture to re-derive what the assertion was looking at — which is a re-run of the analysis, and often of the simulation.

**Acceptance criteria:**
- Every declared condition yields exactly one verdict per run; a reader seeing fewer verdicts than conditions treats the run as cut short, not as passing.
- A verdict carries the condition id, the entities **with their occupancies**, the deciding `sim_time_s`, the segment, and the deciding values.
- A verdict's numbers are reproducible: recomputing them by hand from the samples it names gives the same values.

**Trace:** UAC-CR-AS-2

#### CR-AS-3 (P1): A closed condition vocabulary covering the three kinds [B] names
The system SHALL support conditions covering proximity between two entities, an entity's presence in a region, and an entity reaching a terminal state — and the vocabulary SHALL be **closed**: an unrecognised kind is a named error, never a skipped condition.

**Provenance: [QUOTED] for the three kinds.** [B]: *"decide pass or fail from what the run published. Did the two aircraft come within a distance; did the entity reach the area; did anything reach a terminal state it should not have."*
*The **closure** of the vocabulary is **[ORIGINATED]** — decided by this project, adopting the rule from `contract/condition-file-schema.md` [C1] and containing the rabbit hole named below. [B] gives three examples; it does not say the set is closed.*
*Whether to **adopt EXT-08's condition-file schema outright or supersede it** is **OQ-5**. EXT-08's own OQ-6 resolved that EXT-17 may do either [C2]; that is a permission, not an instruction, and this project has not yet decided.*

**Customer scenario:** The campaign author writes the three kinds of question [B] names, and gets an error rather than silence when they mistype a fourth.
**Pain removed:** An unrecognised condition silently skipped is the "all passed" failure again, with a subtler cause: the campaign ran, judged something, and dropped the one condition that mattered.

**Acceptance criteria:**
- All three kinds are implemented and exercised in the committed example, including at least one condition of each kind that is **never met**, so the not-met path is proven.
- A fourth kind is a named parse error and a non-zero exit before any run starts (CR-AS-1).
- Units are the platform's own and are never converted — metres, degrees, and the platform's `[lat, lon, alt]` order. *([ORIGINATED], from `contract/capture-format-v1.md` §15 [C1]: the capture applies no unit conversion, so a consumer that silently converts introduces an error the file cannot detect.)*

**Trace:** UAC-CR-AS-3

#### CR-AS-4 (P1): An assertion never reads absence as evidence
The system SHALL NOT conclude that an event did not occur from its absence in a capture. A condition whose evaluation depends on absence SHALL report an explicit **indeterminate** state rather than a pass or a fail.

**Provenance: [ORIGINATED].** [B] raises none of this. **Decided by this project**, from the measured upstream finding [C1] [C2]: a capture is a very high-fidelity sample of the published stream, **not a guaranteed-complete transcript**, and loss has been measured — 30 samples in a single frame, 0.023% — with **every platform counter reading zero**. The loss is frame-shaped, is not driven by rate, and appears even in an artifact written inside the host process with no bus in its path, so no consumer configuration avoids it. **That upstream risk is open, not closed.**

**Customer scenario:** The campaign author is told "indeterminate — this condition cannot be decided from a sampled stream" instead of a confident pass that happens to be wrong.
**Pain removed:** [B]'s own example includes *"did anything reach a terminal state it should not have"* — a question naturally implemented as "no record says it did". On a stream with unreported loss, that implementation returns "passed" from a file that is missing the frame in which it happened. It looks perfectly clean. Tenet 3 exists for exactly this.

**Acceptance criteria:**
- Every condition kind is classified as absence-dependent or not, and the classification is stated in the README.
- An absence-dependent condition reports `indeterminate`, with the reason, rather than `pass`.
- `indeterminate` is visible in the report and is never silently folded into pass, fail, timeout or infrastructure error. *(Note the interaction with CR-EX-5: [B] fixes the **run outcome** vocabulary at four. `indeterminate` is a **verdict** state, not a fifth run outcome — a run containing an indeterminate verdict is reported with its four-state outcome plus the indeterminate verdict named. Keeping these two vocabularies distinct is [ORIGINATED] and deliberate, so that [B]'s acceptance criterion 5 stays exactly satisfied.)*
- The README states plainly, in the "limits" section [B] asks for, what a pass does and does not prove.

**Trace:** UAC-CR-AS-4

### Reporting (REP)

#### CR-REP-1 (P1): A result per run and a summary across the campaign
The system SHALL produce a record for every run and a summary across the campaign, both machine-readable and both readable by a person.

**Provenance: [QUOTED] for the two artifacts; [ORIGINATED] for machine-readability.** [B]: *"Reporting — a result per run, a summary across the campaign, and enough detail on a failure that someone can go and look."*
*[B] requires the two artifacts and requires a report *"someone can act on"*; **it does not require either to be machine-readable.** That clause is **[ORIGINATED]** — decided by this project, because CR-CAP-1's re-judgement, CR-REP-4's diff and the stretch-goal regression mode all consume the report as input, and a human-only format would force each of them to re-derive results from the captures.*

**Customer scenario:** The campaign author reads one summary to know how the campaign went, and one run record to know about any run in it.
**Pain removed:** Twenty scattered capture files are not a report. The summary is what turns a set of runs into the evidence [B] says a campaign exists to produce.

**Acceptance criteria:**
- One record per run, containing its identifier, its parameter value, its outcome, its verdicts, its stop-predicate value, and the path to its capture.
- One campaign summary containing the counts of the four outcomes, the self-test result, and the sweep presentation (CR-PAR-2).
- Both are parseable by a machine and legible to a person without a viewer.

**Trace:** UAC-CR-REP-1

#### CR-REP-2 (P1): Enough detail on a failure that someone can go and look
For a failing run, the system SHALL record enough to locate the causing data in the capture without re-running the simulation.

**Provenance: [QUOTED].** [B]: *"enough detail on a failure that someone can go and look."*

**Customer scenario:** The analyst takes a failing run from the summary and is at the causing records in the capture inside a minute.
**Pain removed:** Without the locating key, "run 14 failed" costs a capture-wide search or a re-run — and a re-run of a run that failed intermittently may not fail again.

**Acceptance criteria:**
- A failure names the capture file, the segment, the `(entity, occupancy)` pairs, and the `sim_time_s` of the deciding samples.
- Those coordinates are sufficient to find the records by hand in the capture.
- The failure detail is present in the machine-readable record, not only in console output.

**Trace:** UAC-CR-REP-2

#### CR-REP-3 (P1): The four outcomes are distinct in the report
The system SHALL present pass, fail, timeout and infrastructure error as four distinct outcomes in the report, at both the per-run and the campaign level.

**Provenance: [QUOTED].** [B] acceptance criterion 5: *"Pass, fail, timeout and infrastructure error are four distinct outcomes in the report."*

**Customer scenario:** The campaign author's summary line distinguishes "three scenarios behaved wrongly" from "three hosts would not start".
**Pain removed:** [B] gives it directly: *"Report the categories separately or the summary is worthless."*

**Acceptance criteria:**
- The four counts appear separately in the summary and sum to the number of runs attempted.
- No aggregate anywhere in the report collapses two of the four into one figure.
- Verdict-level `indeterminate` (CR-AS-4) is reported alongside, and never merged into any of the four.

**Trace:** UAC-CR-REP-3

#### CR-REP-4 (P1): A diff that names the first point of divergence
The system SHALL compare any two runs and identify the **first** point at which they diverge — not merely that they differ.

**Provenance: [QUOTED].** [B] acceptance criterion 6: *"A diff between two runs identifies the first point of divergence, not just that they differ."* [B] paragraph 9: *"change one input and show exactly where the two runs diverged."*
*Two of the acceptance criteria below are **[ORIGINATED]** and flagged in place: the frozen-clock exclusion, and the present-versus-different distinction. [B] requires neither; both come from the consumed format's measured behaviour [C1] [C3].*

**Customer scenario:** The campaign author changes one parameter and sees the simulation frame and entity at which the two runs first parted.
**Pain removed:** [B] calls the diff *"the piece that makes it worth having"*. A boolean answer to "are these different?" is exactly what a campaign author already knows.

**Acceptance criteria:**
- The diff reports the first divergence by segment, `(entity, occupancy)`, `sim_time_s` and field — not by line number.
- It works both for the same-configuration case (where it is CR-DET-1's failure reporter) and for the changed-input case.
- **[ORIGINATED]** It excludes frozen-clock segments and states that it did, since a divergence reported there would be an artifact of alignment rather than of the data (CR-CAP-4).
- **[ORIGINATED]** It distinguishes "present in one run and absent in the other" from "present in both with different values", because on this platform those have different causes [C1].

**Trace:** UAC-CR-REP-4

### Documentation and evidence (DOC)

#### CR-DOC-1 (P1): A README covering configuration, assertions, output format and limits
The system SHALL ship a README stating how to configure a campaign, how to write an assertion, the output format, and the limits.

**Provenance: [QUOTED].** [B] deliverable 2: *"A README.md — how to configure a campaign, how to write an assertion, the output format, and the limits."*

**Customer scenario:** A campaign author who has never seen this repository runs their own campaign from the README alone.
**Pain removed:** Without the "limits" section in particular, a reader assumes a pass proves more than it does — see CR-AS-4.

**Acceptance criteria:**
- All four topics [B] names are present, each as its own section.
- **[ORIGINATED]** The **limits** section states, explicitly: what a pass proves and does not prove given a sampled stream; the determinism gate's content basis and why (ADR-1); the stop predicate (CR-EX-3); and the disk ceiling (CR-CAP-5). *([B] requires a limits section and does not say what belongs in it; this list is this project's.)*
- **[ORIGINATED]** The CLI is documented by the checked-in golden `--help` output, kept true by the build-time comparison test (§"Naming and interface conventions") rather than by prose.

**Trace:** UAC-CR-DOC-1

#### CR-DOC-2 (P1): The evidence pack — a real campaign, a recording, and the determinism notes
The system SHALL ship, committed to the repository: a real campaign with its configuration, its captured runs and its report; a 5-minute recording; and a page of notes on determinism.

**Provenance: [QUOTED].** [B] deliverables 3–5: *"A real campaign — its configuration, its captured runs and its report, committed as an example"*; *"A 5-minute recording: launch a campaign, watch it run, read the report"*; *"A page of notes on determinism — what you had to do to make comparison meaningful, and anything you saw that you could not explain. **The last part is the one to write carefully.**"*

**Customer scenario:** A reviewer confirms every claim in this PRD by running one committed command and watching one recording.
**Pain removed:** A campaign runner whose evidence is a screenshot is a demonstration. [B]'s whole thesis is the difference between a demonstration and evidence.

**Acceptance criteria:**
- The committed campaign is the 20-run campaign of CR-EX-7, re-runnable from its committed configuration.
- The determinism notes state what had to be done to make comparison meaningful — at minimum the content basis, segment exclusion, and the `(entity, occupancy)` key — and carry a section for **unexplained observations**, which is left present and honest even if the honest content is "none".
- **The recording needs a person and a screen recorder.** It is called out here rather than assumed, because the equivalent deliverable was **not delivered** upstream [C2]. Everything it should show is scriptable; the README names the command for each beat.

**Trace:** UAC-CR-DOC-2

## Scope authority

The FR sections above are the **contract** for this PRD. The design document (`docs/design.md`, to be added) realizes these FRs as components, sequences and milestone tasks.

**The design must not introduce surface area beyond this PRD's FR table without a corresponding PRD revision.** A new run outcome, a fourth condition kind, a second parameterisation axis, a network listener, or a new output file requires a PRD revision first.

Conversely, **this PRD must not specify implementation detail beyond FR shape.** Process management, the JSON parser, the geodetic distance library, threading and file layout belong in the design. Where this document names a concrete mechanism — the `(entity, occupancy)` key, the frozen-clock exclusion test, the content basis of the determinism comparison — it is because that mechanism *is* the externally-observable contract or is forced by the consumed format, not because the design has been pre-empted.

**One addition specific to this project:** the design may not introduce a dependency on any EXT-08 source artifact. EXT-17's inputs are `contract/`'s specification and capture files, and the host and recorder binaries as processes. A design that reads an EXT-08 header has crossed the boundary the repo split exists to enforce.

---

## Data model: the campaign as a logical entity

EXT-17 authors two record shapes. The capture's own model is EXT-08's and is specified in `contract/capture-format-v1.md`; it is consumed, never authored, and is not restated here.

**Campaign configuration** — authored by the campaign author.

| Field | Type | Required | Description |
|---|---|---|---|
| `scenario` | string | yes | The scenario to run |
| `axis` | object | yes | The one parameterisation axis (CR-PAR-1): what varies, and the values it takes |
| `runs` | integer | yes | Number of runs; ≥ 20 for the acceptance campaign |
| `stop_predicate` | object | yes | The explicit definition of "finished" (CR-EX-3, OQ-1) |
| `timeout` | duration | yes | Per-run backstop (CR-EX-4) |
| `conditions` | path | yes | The condition file (CR-AS-1) |
| `disk_ceiling` | integer | no | Campaign-level byte ceiling (CR-CAP-5) |

**Per-run record** — authored by EXT-17, one per run.

| Field | Type | Required | Description |
|---|---|---|---|
| `run_id` | string | yes | Zero-padded ordinal; never a timestamp |
| `parameter_value` | any | yes | This run's value on the axis |
| `outcome` | enum | yes | `pass` \| `fail` \| `timeout` \| `infrastructure_error` — exactly one |
| `stop_reason` | object | yes | What the stop predicate evaluated to, or the fault that ended the run |
| `verdicts` | array | yes | One per declared condition, each `met` \| `not_met` \| `indeterminate` |
| `capture_path` | string | yes | Where the capture is |
| `failure_detail` | object | when failing | Segment, `(entity, occupancy)`, `sim_time_s`, deciding values (CR-REP-2) |

**Campaign summary** — authored by EXT-17, one per campaign: the four outcome counts, the self-test result including both the content and the byte comparison, the sweep presentation, and the campaign-level faults encountered.

### State model: one run

- `pending` → `host_starting` (trigger: the campaign loop reaches this run)
- `host_starting` → `ready` (trigger: observed readiness) | → `infrastructure_error` (trigger: host fails to start, or the readiness timeout expires)
- `ready` → `loading` (trigger: `load_scenario` published)
- `loading` → `running` (trigger: scenario reported loaded, `start` published) | → `infrastructure_error` (trigger: the scenario refuses to load)
- `running` → `ending` (trigger: **the stop predicate is satisfied** — CR-EX-3) | → `timeout` (trigger: the run timeout expires) | → `infrastructure_error` (trigger: the host dies)
- `ending` → `judged` (trigger: teardown complete, capture closed and read)
- `judged` → `pass` | `fail` (trigger: all verdicts met / any verdict not met)

**Rules.** Every terminal state is exactly one of the four outcomes [B] names; `timeout` and `infrastructure_error` are terminal and are never re-derived as `fail`. Teardown runs on **every** path out of `running`, including both fault paths, because CR-EX-1's isolation rule has no exceptions. `indeterminate` is a verdict state inside `judged`, never a run state.

## Migration plan

There is nothing to migrate — EXT-17 does not exist. What this section covers is the **only versioned contract in play**: the capture format EXT-17 consumes.

**Current state.** `n8ro-capture/1`, frozen at EXT-08's M7, vendored read-only in `contract/`. The vendored fixture was written by producer 0.5.0 [C3].

**Compatibility rule.** Adding a key to an existing record type is non-breaking and stays `n8ro-capture/1` (§13). A new record type, a renamed or retyped key, or a changed closed vocabulary is `n8ro-capture/2` and a downstream change here.

**Migration steps if `n8ro-capture/2` ships.** (1) EXT-17's reader rejects it by CR-CAP-3 — loudly, on the first capture, which is the intended behaviour and not a defect. (2) Re-pin `contract/`, and diff the specification. (3) Extend the reader to the new version; **old captures stay valid under version 1**, so stored campaigns remain re-judgeable (CR-CAP-1). (4) Re-run the self-test and the reader conformance suite before any campaign is trusted.

**Pin discipline.** `contract/` is a vendored copy and **has already drifted twice** — once upstream at re-pin time, and once found by this project's own reading [C3]. The pin is re-checked at the start of every milestone, and a drifted `contract/` is a defect to fix, not a difference to tolerate. See R4.

## Performance requirements

> **All figures below are inherited or derived, and none is yet measured by this project.** They are targets and budgets, not results. This section will carry measured numbers after M3.

| Quantity | Basis | Budget for v1 |
|---|---|---|
| Capture size per run | ~64 MB per 200-second run, 42 entities [C2]; ~465 B/record, independently derived from the fixture [C3] | Stated in the README; bounded by CR-CAP-5 |
| Campaign disk footprint, 20 runs | The above × 20 | ~1.3 GB — checked pre-flight, never discovered at run 19 |
| Re-judgement of a stored capture | Upstream replays a 64 MB capture in ~1 s [C2]. **Not a commitment here** — it is a different program | Re-judging a 20-run campaign completes without a person waiting on it; number fixed at M6 |
| Run wall-clock time | Dominated by the simulation, not by the harness | The harness's own overhead per run is bounded and reported, so a slow campaign is attributable |
| Determinism self-test | Two extra runs per campaign, by construction | Reported as its own line in the campaign summary, so its cost is visible rather than hidden in the total |

**Optimisation approach:** none, in v1. The campaign is I/O- and simulation-bound, and the first optimisation anyone would reach for — reusing a host across runs — is **forbidden by CR-EX-1** and was measured worthless upstream. Recording that here is the point: the obvious speed-up is the known-wrong move.

## Observability

**In the report** (the primary observability surface, because nobody is watching): the four outcome counts; the self-test result with both comparisons; the fault that ended each non-passing run; the stop-predicate value per run; and the drop and orphan counters read out of each capture's trailer.

**In the log:** one line per run-lifecycle transition, with wall-clock time — which is where wall-clock time is *allowed* to appear, and nowhere in compared output (CR-DET-2). Bring-up waits log what they are waiting for and for how long, so a hung run is diagnosable after the fact.

**Surfaced prominently, never buried** — a capture whose trailer reports non-zero decode-side or delivery-side counters is a capture that may be missing whole message types or may be a sampled record. Either makes a verdict over it weaker than it looks, and CR-EX-5 already routes the structural case to `infrastructure_error`.

**No alerting, no health endpoint, no metrics backend.** This is a workstation tool run by a person who reads its report.

## Cross-service impact

**EXT-08 (upstream).** EXT-17 consumes `n8ro-capture/1` and drives EXT-08's recorder as a process. Rev 1 listed three upstream gaps binding this project; **two have since been closed there and one has not, which is the one that matters:**

- ~~The byte-limited capture requirement is unbuilt.~~ **Built** — EXT-08 producer 0.9.0, with `--capture-max-bytes`, a `stop`-or-`rotate` action and both stated in `header.limits`. The format did not move. R3 re-rated; CR-CAP-5 narrowed to the campaign level and given a criterion that configures the upstream bound.
- ~~The 5-minute demo recording was not delivered there.~~ **Delivered**, as four published takes. It remains a reason to schedule ours deliberately rather than late (CR-DOC-2), but no longer an upstream gap.
- **The "capture is not a guaranteed-complete transcript" risk is open, not closed (R5).** Unchanged, and it is the upstream finding that actually binds this project's assertion semantics — see ADR-6. Do not let the two closures above suggest this one moved with them.

**Two defects go back to EXT-08 or to the brief's owner, and neither is worked around here.** The `EntityStateSample.h` citation in [B]'s own surface table (R7), and the drift in `contract/` (R4).

**N8RO platform (read-only).** EXT-17 runs `n8ro-sim-app.exe` and reads `C:\N8RO`. It writes nothing there. If the chosen parameterisation axis requires authoring scenario variants inside the install tree, that is a blocking finding (R9, OQ-4).

**Deployment coordination:** none. There is no service to deploy.

## Operational readiness

**Runbook — the four faults [B] names, and what a person does about each.** Host fails to start: the report names it `infrastructure_error` with the bring-up log; check the model path and schema file first, since [B]'s upstream sibling records configuration as the most common failure. Scenario refuses to load: same category; check the scenario name as the platform reports it. Run never ends: the timeout fires and the run is `timeout`; if a whole campaign times out, the stop predicate (OQ-1) is wrong, not the scenario. Host dies mid-run: `infrastructure_error`, partial capture retained; if it recurs, that is the plugin-free teardown case escalating and it goes to the mentor with the run records attached (R6).

**Before a campaign:** free space checked against the projection (CR-CAP-5); no host running that the campaign did not start (CR-EX-1); the self-test green (CR-DET-1).

**Capacity:** one workstation, one campaign at a time. Parallelism is out of scope for v1.

**Dependencies and SLAs:** none external. Everything runs locally against a local install.

---

## Dependencies and constraints

| Dependency | Owner | Status | Impact if delayed |
|---|---|---|---|
| N8RO 2.1.328 install + `com.n8ro.dev` SDK component | Arkheon (external) | **Available**; host binary verified present [C3] | Blocks everything |
| `contract/` — the frozen `n8ro-capture/1` spec and fixture | EXT-08 (vendored, read-only) | **Available**, and **drifted** — see R4 | A drifted pin costs reader rework; a specification defect must go back to EXT-08 |
| EXT-08's recorder binary, driven as a process | EXT-08 | Available | Blocks CR-CAP-1's live path. The offline re-judge path is unaffected |
| A ruling on the determinism gate's basis (OQ-2) | The owner of [B] | **Open — escalated** | **Blocks M4, which [B] makes a hard stop.** The highest-priority item in this document |
| Mentor confirmation of the headless invocation (OQ-3) | Mentor | Open — [B] asks for it by name | Does not block: the invocation is known to work [C1]. Confirms it is the intended shape |
| A scenario with a varyable axis that does not require writing into `C:\N8RO` (OQ-4) | Mentor / scenario catalogue | **Available — three of them**, measured at M2 (`docs/m2-automation.md` §5) | No longer blocks M5. R9 closed at rev 3 |
| VS 2026 v18.x, C++17, Release\|x64 | Local | Assumed available | Blocks all build work |

**Constraints:**

- **C1 — [QUOTED].** Standalone C++17 program, Track C. [B] header: *"Track C (open interfaces, no plugin) · Language C++17"*. No plugin, no plugin ABI.
- **C2 — [QUOTED].** [B] header: *"Prerequisite EXT-08"*. EXT-17 depends on EXT-08's output, and on nothing of EXT-08's source.
- **C3 — [QUOTED].** [B] rule 7: *"Never throw."* Failures are return values plus logging.
- **C4 — [QUOTED].** [B] rule 5: *"Stamp records with simulation time, not your own clock — as in EXT-08."*
- **C5 — [QUOTED].** [B] paragraph 19: *"Campaign runs are for the closed configuration."* No externally-timed input feeds a campaign run.
- **C6 — [QUOTED] as a target, [ORIGINATED] as a discipline.** [B] header: *"Effort 2–3 weeks"*. **This document will not report elapsed effort it has not measured.** Every day figure in §"Rollout and milestones" is an estimate, labelled as one, and no milestone will be described as "inside budget" without a recorded burn-down. *(The discipline is adopted from [C2]'s rev-9 audit, which withdrew exactly such claims.)*
- **C7 — [ORIGINATED].** `C:\N8RO` is read-only for this project: read headers, read docs, run binaries, write nothing. Files in this repository carry no Arkheon proprietary header, since that convention applies only to files created inside the install tree.
- **C8 — [ORIGINATED].** No EXT-08 source is read, linked, or cited. The boundary is the point of the repo split.

## Risks and open decisions

| Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|
| **R1 — [B]'s step-4 gate cannot pass under a byte reading, and step 4 is a hard stop.** [B] says *"Do not build further until it passes"*. Measured upstream: byte comparison fails; content comparison passes 50 358 / 50 358 [C1] | **Critical.** Under a byte reading the project stops at milestone 4 permanently, for a reason that has nothing to do with the thing the gate protects | **Certain** on both available hosts, if the byte reading is taken | CR-DET-1 restates the gate on content, **marked as a deviation rather than buried**; ADR-1 records the alternatives rejected; **OQ-2 escalates it for a ruling before M4 opens**. M4's first act is to reproduce the upstream measurement here rather than inherit it on trust |
| **R2 — "Is the run finished?" is unsolved, and it is on the critical path.** [B] flags it: *"harder than it sounds"*. EXT-08 explicitly refused it [C2] | High — it gates CR-DET-1, because two runs must be bounded identically before they can be compared at all | High — nobody has solved it | **OQ-1, named in rev 1 with a decision target of M2 and explicit criteria**, rather than discovered at milestone 2. CR-EX-4's timeout is the always-present backstop, so a wrong predicate costs correctness of comparison, never a hung campaign |
| **R3 — Disk exhaustion; upstream now bounds a file, not a campaign.** EXT-08 producer 0.9.0 ships a per-capture byte bound with a `stop`-or-`rotate` action, stated in `header.limits` [C2]. A **campaign** is still unbounded by anything upstream: ~64 MB per 200 s run [C2], ~465 B/record [C3], ~1.3 GB for 20 runs | Medium — a full disk mid-campaign still ends the run, but the evidence already written survives: an upstream-bounded capture closes with a well-formed trailer rather than a line cut in half | Low — both controls are configuration, and the pre-flight check catches the projection before run 1 | CR-CAP-5, now at two levels: the recorder's per-run bound configured by us, and our own campaign ceiling with a pre-flight free-space check. **Re-rated at rev 2 from High/Medium.** The rev-1 premise — "specified but unbuilt — no rotation, no size ceiling in the header" — is no longer true in any of its three clauses. CR-CAP-5 stays P2 and stays ours, because a per-file bound multiplied by twenty is not a campaign bound |
| **R4 — `contract/` is a pinned copy and drifts every time the upstream producer ships.** Re-pinned at rev 2 to EXT-08 `0fe7cd5`, producer **0.9.0**. It has now gone stale twice in two producer releases, both times for an added key: 0.8.0's `header.sample_form`, then 0.9.0's `header.limits` / `part` / `continues_from` / `continued_in`. **The vendored fixture still reports producer 0.5.0** and carries none of those keys [C3] | Low — the spec is current again, and every drift so far has been an added key that §13 requires a reader to ignore, so a reader built to CR-CAP-2 is unaffected by construction. What a stale pin actually costs is *interpretation*: a reader meets a key its copy of the spec does not describe | **Re-pinned at rev 2; the fixture's own staleness is unresolved and deliberate** | Re-check the pin at the start of every milestone — twice in two releases is the base rate, not bad luck. **Test against a capture written by the pinned producer, not only against the vendored fixture**, or the newest header keys are never exercised: the fixture predates all of them. CR-CAP-2's ignore-unknown-keys criterion is what makes the drift survivable, and it is worth a test that feeds a header carrying a key the reader has never heard of |
| **R5 — Absence is not evidence, and no counter says so. Upstream risk is OPEN, not closed.** Measured loss of 30 samples in one frame, 0.023%, with every counter at zero; frame-shaped; not rate-driven; present even in an in-process writer with no bus in its path [C1] [C2] | High — [B]'s own example condition (*"did anything reach a terminal state it should not have"*) is naturally implemented as an absence test, and would return a confident wrong pass | Confirmed in shape, unattributed in cause | **CR-AS-4 makes it a requirement rather than a caveat**: absence-dependent conditions report `indeterminate`. ADR-6 records why. The README's limits section states what a pass proves |
| **R6 — Host teardown reliability across 20+ unattended runs.** A `0xC0000005` teardown access violation was observed on this platform with a plugin loaded | High for EXT-17, which needs 20+ clean cycles | **Low for the configuration EXT-17 uses.** Measured upstream: twenty consecutive plugin-free load-run-teardown cycles, host and recorder both exit 0 every time, no access violation [C1] [C2] | **The closure is inherited and scoped**, and this document does not over-claim it: it says nothing about the plugin-loaded case that produced the original observation, which EXT-17 does not use. CR-EX-6 makes host death a handled state regardless, so the campaign survives a recurrence rather than depending on the measurement |
| **R7 — [B] cites an API that does not exist.** [B]'s surface table names `include\n8ro-sim\infrastructure\EntityStateSample.h` as "what a run publishes". **Verified absent** from release 2.1.328 [C3]; that directory holds two headers, and neither is it | **Low for EXT-17, high if unstated.** EXT-17 consumes captures and does not need the type at all — but a plan written from [B] alone would budget against a shipped API and rediscover the largest single work item of the upstream project | Certain — it is a defect in the document | **Stated here so it is not rediscovered.** EXT-17 points at `n8ro-capture/1` instead. **The correction goes back to the brief's owner**; the same defect exists in EXT-08's brief and was escalated there [C2]. It is not EXT-17's to fix, and this PRD does not budget for it |
| **R8 — A determinism leak in our own comparison path** | High — it invalidates the gate everything else rests on, and reads as a platform defect | Medium-high — [B] names three sources and each is easy to reintroduce | CR-DET-2 tests each of the three directly, on every change, including under a comma-decimal locale. CR-DET-3 makes a failure attributable, so a leak is diagnosed rather than misattributed |
| **R9 — The parameterisation axis may require writing into a read-only tree.** [B] offers three axes: initial positions and velocities, which entities are present, which scenario | Low — **closed by measurement.** The premise does not hold for any of the three axes | Low | **Closed at M2 by the `tools/spike-axis` probes** (`docs/m2-automation.md` §5): all three of [B]'s axes are reachable over the bus with **no authoring into `C:\N8RO`**. Entity state set between `load_scenario` and `start` survives materialisation and is integrated from; `sendEntityDelete` and `sendEntityCreate` both take effect; a second scenario loads from the catalogue by name. The rev-1 rating of *"Unknown — not yet investigated"* was correct when written and is now false. **Two limits stand and are carried into OQ-4**: the spike measured *feasibility, not fidelity* — a reachable parameter value can still produce a scenario that makes no sense — and an entity deleted before `start` still appears in `entity_add` and in `trailer.counts`, so an axis of "which entities are present" must count presence by samples, not by adds |
| **R10 — The 5-minute recording needs a person.** [B] requires it as a deliverable | Low technically, but it is an undelivered acceptance item | Medium — **the equivalent deliverable was not delivered upstream** [C2] | Scheduled explicitly at M7 rather than assumed, with every beat scripted so the recording is a capture of a working command sequence rather than a performance (CR-DOC-2) |
| **R11 — `contract/` can be imprecise as well as stale, and an imprecision is harder to notice than a drift.** Found at M3: §6.7 says a rotated run's totals are the sum across its parts' `counts`; for `segments` that is false, because a segment cut by a rotation is closed in one part and opened in the next. Measured — a real four-part capture summed to 5 for a 2-segment run | Low — it bites only on rotated captures, which OQ-6's `stop` means this project does not produce; and it is arithmetic, not data loss | Low — the reader implements what §6.7 says, computes what is true beside it, prints both and names the gap | **E-3**, drafted to EXT-08. Not worked around and not propagated. R4's mitigation — re-check the pin every milestone — catches a *stale* copy; only reading the specification against a real file catches an *imprecise* one, which is the argument for the conformance suite's fourth tier existing at all |

### Open questions

| # | Question | Status | Decision target | Rationale (why open / what would resolve it) |
|---|---|---|---|---|
| **OQ-1** | **What predicate defines "the run is finished"?** | **Open — named in rev 1 deliberately** | **M2**, before any campaign loop is built | [B] flags the difficulty itself and does not answer it; EXT-08's PRD explicitly refused the problem as EXT-17's [C2]. It is named here rather than met at milestone 2 as a rabbit hole. **Selection criteria, fixed now:** the predicate must be (a) observable from what the run publishes, (b) free of wall-clock quantities, (c) identical across two runs of one configuration — because CR-DET-1 cannot compare two runs that stopped at different points, and (d) reached by every run of the reference scenario without the CR-EX-4 timeout firing. **A frame budget is the leading candidate** and is not yet a decision: it satisfies (a)–(c) by construction and was what made the upstream determinism measurement possible at all [C1]. Resolved by the M2 spike measuring candidate predicates against the reference scenario |
| **OQ-2** | **Is the determinism gate keyed on content, or on bytes?** | **Needs Input — escalated** | **Before M4 opens.** [B] makes step 4 a hard stop, so this is the highest-priority question in this document | [B] requires a self-test that shows two identical runs *"produce identical captures"* and that they *"match"*, without saying at what level of representation; **the words "byte" and "hash" do not appear in [B] at all** [C3]. A byte reading is defensible and is the strictest available; it also **cannot pass on either available host** — byte comparison fails, content comparison passes 50 358 / 50 358 [C1]. CR-DET-1 adopts the content reading and marks it as this project's decision, not the client's. **Resolved by a ruling from the owner of [B]**, who is asked to confirm the content reading or to accept that the byte reading requires a host change or a waiver. **The implementer must not close this alone** — that is precisely the mistake that cost the upstream project two revisions [C2]. EXT-08 has an escalation making the same request; this is the downstream half of it. **Sent to the owner of [B] on 2026-08-31; awaiting a reply** (EXT-08 `docs/escalations.md` E-1). CR-DET-1 stands unchanged in the meantime and M4 is not blocked by the wait — what a ruling would change is whether the content reading is confirmed or whether the byte reading forces a host change or a waiver |
| **OQ-3** | Is `n8ro-sim-app.exe --sim-config SimEngineHost_SharedMemory --model-path <dir> --schema-file <name>` the intended production invocation? | Open — and [B] asks for exactly this | M2 | **[B] says so itself**: its surface table reads *"the host binary that runs an engine with no GUI. Confirm the invocation with your mentor."* What is inherited from [C1] is that this invocation **works** — it takes no scenario argument, and load and start are separate publishes on `sim/scenario/command` and `sim/engine/command`. What is *not* established is that it is the intended shape. **EXT-17 does not start from zero here and this PRD does not budget as though it does**; the confirmation is worth having and does not block |
| **OQ-4** | Which of [B]'s three parameterisation axes does v1 use? | Open | M5 | [B] offers initial positions and velocities, which entities are present, or which scenario from the catalogue — and settles the count (*"One axis done properly"*) without settling the choice. **Rev 3: the deciding constraint was expected to be R9 and is not.** The M2 spike measured all three axes reachable with no authoring into `C:\N8RO` (R9, now closed), so **the choice at M5 is free on merit**, and "which scenario from the catalogue" is no longer a fallback — it is one of three equal candidates. What the spike deliberately did **not** measure is fidelity: whether a swept range of values produces scenarios that still make sense. That, and what CR-AS-4 can soundly assert over each axis given that a deleted entity still appears in `entity_add`, are now the deciding criteria. Resolved at M5 |
| **OQ-5** | Adopt EXT-08's condition-file schema, or supersede it? | Open | M6 | EXT-08's OQ-6 resolved that EXT-17 *may* adopt or supersede it [C1] [C2] — a permission, not an instruction, and nothing in [B] mentions a condition-file format at all. Adopting it buys a documented shape and a worked example for free, and makes EXT-08's live verdicts directly comparable with EXT-17's re-judgements. Superseding it may be necessary if the campaign needs cross-run conditions, which EXT-08's per-run schema does not express. **Resolved by CR-AS-3's implementation**: adopt unless a required condition cannot be expressed, and record which in the README |
| **OQ-6** | What is the campaign's disk ceiling, what does it do on reaching it, and which per-run bound do we configure upstream? | **Resolved at M3** — `docs/m3-oq6.md` | M3 | **Three answers, all measured.** (1) **8 GiB, over the whole campaign directory**, not over its captures: a 1200-frame run costs 29 788 003 bytes, of which the capture is 24 297 928 and the host logs 5 490 075, so a capture-only projection under-states a campaign by 22.6% — cross-checked against M2's twenty runs at 22.5%. At [B]'s scale a 200 s run is ~99 MB and twenty are ~1.99 GB, against the rev-1 inherited estimate of ~1.3 GB. (2) **Reaching it stops the campaign with a named outcome**, checked before run 1 against free space and after every run against actual usage — both halves verified against real runs, with completed runs still reading back conformant afterwards. (3) **`stop`**, with `--capture-max-bytes` defaulting to `61 000 × --frames`, three times the measured per-frame capture cost, so every run carries a bound and no normal run reaches it. Rev 2's argument survives contact with the measurement: **a campaign that needs the tail of a run has mis-set its stop predicate (OQ-1)** — the predicate is a frame budget, and a capture reaching its bound before frame N means the bound is wrong, not the run. **Rotation was exercised rather than declined on the documentation**: a real four-part capture, stitched and read back conformantly, reproducing an unrotated run's roster lifecycle exactly. It was rejected because one run's two segments become five `(part, segment)` keys, a cut duplicates its boundary `sim_time_s`, and `counts.segments` cannot be summed (R11) — costs **CR-CAP-4's** per-segment scoping would pay on every run, whether or not any run ever rotates. Rev 2 named "one file per run keeps CR-CAP-4's segment handling simple" as an argument; M3 measured what that sentence is worth |

### Rabbit holes

- **"Is the run finished?"** [B] warns it is *"harder than it sounds"*, and it is the one problem both projects agree belongs here. **Containment:** it is OQ-1 with criteria fixed in rev 1 and a decision target of M2, and CR-EX-4's timeout is an unconditional backstop — so a wrong answer degrades comparison quality rather than hanging the campaign. Timebox the spike; take the frame budget if nothing better appears.
- **Chasing the byte-for-byte gate.** The temptation, on being told a byte comparison fails, is to make it pass — by normalising, by filtering, by excluding fields until two files match. That builds a comparison that passes by construction and measures nothing. **Containment:** ADR-1 forbids it. The byte comparison is *reported*, never *engineered to pass*.
- **Re-deriving the content comparison.** It has known traps — `sim_time_s` is not a key, and frozen-clock segments cannot be aligned at all — and both were learned upstream by getting them wrong first [C1]. **Containment:** CR-CAP-4 and CR-DET-1 name both traps as acceptance criteria, and this project verified the exact detection test against the fixture before writing them [C3].
- **Parameterisation axis creep.** Two axes look like twice the evidence and are four times the analysis. **Containment:** [B] settles it — *"One axis done properly beats four done loosely"* — and a second axis is an Out-of-Scope entry requiring a PRD revision.
- **Condition expressiveness.** The third condition kind invites a fourth, then a boolean combinator, then a parser. **Containment:** CR-AS-3 closes the vocabulary; a fourth kind is a PRD revision, which is the point.
- **Reimplementing EXT-08.** The capture path is visible, well-documented, and tempting to absorb. Doing so dissolves the boundary the repo split exists to enforce and doubles the maintenance of a frozen format. **Containment:** ADR-2, constraint C8, and the Scope Authority clause forbidding a design dependency on any EXT-08 source artifact.
- **The parallelism stretch goal.** It multiplies the isolation risk and makes any determinism failure ambiguous between our concurrency and the platform's. **Containment:** Out of scope until the serial campaign and the self-test are both green.

## Alternatives considered

### Option 1: Consume the frozen capture format; drive host and recorder as processes (selected)
EXT-17 starts a fresh host per run, drives the existing recorder as an external process, and reads the resulting `n8ro-capture/1` files with a reader written from the specification alone.

**Pros:** honours the repo boundary exactly; inherits a frozen, self-describing, demonstrably-readable artifact; recording is separate from assertion by construction, which is what [B] step 3 asks for; a stored run is re-judgeable with no host at all.
**Cons accepted:** EXT-17 depends on two binaries it does not own, so process-level faults are its problem to classify — which CR-EX-5 and CR-EX-6 require anyway; and it inherits upstream gaps (R3, R5) it cannot fix.

### Option 2: Build a second recorder inside EXT-17
Subscribe to the bus directly and write our own capture.

**Pros:** one process instead of two; no dependency on an EXT-08 binary.
**Why not chosen:** it re-pays the upstream project's largest single work item — the entity picture, which the SDK does not provide and which [B]'s own surface table wrongly presents as shipped (R7) — inside a 2–3 week budget. It also creates a second capture format, or a second implementation of the frozen one, either of which makes the two projects' outputs incomparable. [B] step 3 says *"Subscribe as in EXT-08"*; consuming EXT-08's frozen output is the cheapest faithful reading of that.

### Option 3: One long-lived host, many scenario loads
Start one host and load, run and unload scenarios in a loop.

**Pros:** much faster per run; no repeated bring-up cost.
**Why not chosen:** **[B] forbids it** — *"not a still-running host"* — and it has been measured worthless independently: an upstream harness that shared one host across twenty cycles got one usable run and nineteen recordings of orphaned samples [C1]. This is the rare case where the obvious optimisation is both prohibited and known to fail, and §"Performance requirements" records it so nobody reaches for it later.

### Option 4: Keep the byte-for-byte gate as written
Take the strictest reading of [B]'s *"identical captures"* and gate on byte equality.

**Pros:** it is the most defensible reading of [B]'s words, needs no ruling, and cannot be accused of weakening a client requirement.
**Why not chosen:** it cannot pass on either available host, and [B] makes step 4 a hard stop — so the honest consequence is that the project ends at milestone 4. It also measures the wrong thing: the failure reports the publication schedule, not the simulation, while the property the gate protects holds exactly [C1]. **This option is not dismissed; it is escalated.** OQ-2 puts the choice in front of the person who owns [B], with the measurement attached, and this PRD implements the content reading in the meantime rather than stopping.

### Option 5: Drive the run lifecycle from the scripting namespaces rather than the bus
[B]'s surface table offers this route explicitly: *"Run lifecycle from a script, if you drive it that way | the scenario-control and simulation scripting namespaces, whose signatures are in data\resources\missions\stubs\"*.

**Pros:** it is a supported, signature-documented surface, and [B] names it — so it needs no justification and no mentor confirmation, unlike the bus route (OQ-3).
**Cons:** unexamined. This project has not read the stubs directory and does not know whether those namespaces are reachable from an out-of-process campaign runner or are in-engine only — which is the same shape of trap that `EntityStateSample.h` turned out to be (R7).
**Why not chosen:** [B]'s own wording makes it conditional — *"if you drive it that way"* — so it is an option, not an instruction. The bus route is chosen because it is **demonstrated working** end to end [C1] rather than merely documented, and because it keeps EXT-17 symmetric with the recorder it drives. **This is a decision made on asymmetric evidence and it is recorded as such:** if the bus route stalls at M2, the scripting namespaces are the first fallback to examine, and examining them is cheap.

### Option 6: Wall-clock run bounding
Run each simulation for a fixed number of seconds.

**Why not acceptable:** two runs stopped after the same duration have not covered the same simulation, so their captures are guaranteed to differ for a reason unrelated to determinism [C1]. It would make CR-DET-1 unpassable under *any* comparison basis, and [B] rejects the shape directly — *"make it explicit rather than a sleep."*

### Option 7: Do nothing / keep watching runs
**Why not acceptable:** it is [B]'s own opening argument. One watched run is a demonstration; a hundred unwatched runs are evidence. Nothing else produces the second.

## Validation and test plan

- **Unit — the capture reader:** every record type round-trips; unknown `format_version` is a named rejection (CR-CAP-3); unknown keys in known records are ignored; a `fields` key the schema does not declare is a named producer defect. Verified by **mutation**: a truncated file, a malformed line, an undeclared field, and a trailer count that disagrees with the records must each fail the reader.
- **Unit — segmentation and occupancy:** a name re-created at a higher occupancy is a distinct entity; frozen-clock segments are detected by the exact maximum-samples-per-`(entity, occupancy, sim_time_s)` test; no code path sorts globally by `sim_time_s`. **Seeded with the numbers this project already measured** [C3]: the fixture must report 2 segments, 6 924 / 21 samples, and a frozen-clock maximum of 1 / 11.
- **Unit — determinism primitives:** each of the three hazards [B] names, tested directly rather than only end to end, and the number-formatting one tested under a comma-decimal locale.
- **Unit — condition evaluation:** all three kinds against synthetic sample sequences, including the exactly-at-threshold boundary and the never-met case; an unrecognised kind is a named parse error before anything starts.
- **Integration — one run:** bring-up without a sleep; recorder attached before the roster burst (`attached_mid_run: false`, zero orphans); stop predicate fires; teardown leaves no process.
- **Integration — the four faults, injected deliberately:** host fails to start; scenario refuses to load; run never ends; host killed mid-run. Each must produce a named `infrastructure_error`, a continued campaign, and no surviving process.
- **Integration — isolation:** run *N*'s capture is structurally independent of run *N−1*'s; a pre-existing foreign host is refused before any run starts.
- **System — the determinism gate:** two same-configuration runs, bounded by the same stop predicate, compared on content and on bytes. **The content comparison is the gate; the byte comparison is reported.** Re-run at every subsequent milestone, not once — [B] requires *"a self-test you run every time"*.
- **System — the 20-run campaign:** the committed example deliverable, re-runnable, exercising the sweep, the four outcomes, and the report.
- **System — re-judgement:** the committed campaign re-judged against a modified condition file with no host started; verdicts identical to the live ones where the conditions are unchanged.
- **Contract check:** the reader parses the vendored fixture and its counts agree with the trailer. **Re-run at the start of every milestone as the `contract/` drift check** (R4).
- **CI:** none configured — this is a workstation project against a local install. The self-test, the fault matrix and the reader conformance suite are each one scripted command, which is what makes running them habitually realistic.

## Rollback strategy

The rollback surface is not a deployment. It is **the trustworthiness of a campaign report**, because that is the only thing anyone consumes.

**Trigger conditions:** the determinism self-test fails and the cause is inside EXT-17 (R8); the reader disagrees with `contract/capture-format-v1.md`; a campaign is found to have run with fewer conditions loaded than declared; `contract/` is found drifted mid-project (R4); or the four-outcome classification is found to have miscategorised a run.

**Rollback steps.** (1) **Stop running campaigns on the suspect build immediately** — a wrong report is worse than none, and reports accumulate silently. (2) **Quarantine reports produced by the suspect build**: move them aside, never delete them. A wrong report is evidence for the investigation. (3) Identify the last build whose self-test and reader conformance suite both passed. (4) Re-run both against that build to confirm the baseline is genuinely good rather than merely older. (5) Re-judge the quarantined campaigns' **stored captures** against the good build — which costs no re-run, and is the entire reason CR-CAP-1 exists.

**Data rollback.** Captures are EXT-08's artifacts and are never edited by EXT-17. Reports are regenerable from stored captures, so "rolling back a report" means re-judging, not editing.

**Partial rollback.** The four pieces are independently disableable: a campaign can run with no conditions (execution and reporting only), and the assertion path runs with no host at all. A fault in one does not force reverting the others.

## Rollout and milestones

> **Every day figure below is an estimate made when this plan was written. None is a measurement, and this document will not report elapsed effort it has not recorded** (constraint C6). [B]'s target is *"Effort 2–3 weeks"*.
>
> Milestone order follows [B]'s eight steps deliberately — run one by hand, automate one, capture, prove determinism, parameterise, assert, report, handle the ugly reality — with one departure, stated because it matters: **[B]'s step 8 work is pulled forward into M2 and M6 rather than left last**, since CR-EX-6's fault handling is what makes the 20-run campaign possible at all, and a campaign that cannot survive a fault cannot be run unattended to discover it needs to.

### M1 — Run one scenario headless, by hand (est. 0.5 day)
[B] step 1. Start the host manually, load, run, tear down. Learn the lifecycle before automating it. Confirm the invocation with the mentor (OQ-3). Re-check the `contract/` pin (R4).
**Validation:** one run completed by hand, its lifecycle written down. OQ-3 asked. The drift found in this PRD [C3] is either confirmed or resolved.

### M2 — Automate one run, and answer "is it finished" (est. 2–3 days)
[B] step 2. Bring-up on observed conditions with no sleep; load; start; **detect the end**; tear down. Two spikes run here: the **end-of-run predicate** against OQ-1's four criteria, and the **parameterisation-axis feasibility** check against R9.
**Validation:** CR-EX-2, CR-EX-3, CR-EX-4. Twenty automated single runs end at the same point by the predicate's own measure. **Gate: OQ-1 is decided and written into the README before M3 opens.** If no predicate satisfies all four criteria, that is an escalation, not an implementation choice.

### M3 — Capture the run, and read it back (est. 1.5 days)
[B] step 3. Drive the recorder; attach it before the host publishes its roster; build the conformant reader from `contract/capture-format-v1.md` alone. Measure real capture size and fix the disk ceiling, the per-run upstream bound and its stop-or-rotate action (OQ-6).
**Validation:** CR-CAP-2 through CR-CAP-5, and **the first half of CR-CAP-1**. The reader parses the vendored fixture, agrees with its trailer, and survives five deliberate mutations plus a sixth that adds keys it has never seen and must ignore. `attached_mid_run: false` and zero orphans on a real run. OQ-6 decided. **Gate: no EXT-08 source has been read, and no EXT-08 identifier appears in EXT-17's source** — enforced by the reader's own build rather than asserted. *CR-CAP-1's second half — a re-judge mode producing verdicts from a stored capture — needs conditions to judge and is validated at M6 with CR-AS-3. M3 does not claim it.*

### M4 — Prove determinism (est. 1.5 days) — **[B]'s hard gate**
[B] step 4: *"Do not build further until it passes."* Reproduce the upstream measurement **here, on this machine**, rather than inheriting it. Build the content comparison and the byte comparison; report both.
**Validation:** CR-DET-1, CR-DET-2, CR-DET-3. **Gate, and it is the real one: OQ-2 must be answered before this milestone opens.** If the ruling is that the byte reading stands, this milestone cannot pass on either available host and the project stops here by [B]'s own instruction — which is exactly why the question is escalated in rev 1 rather than met at milestone 4. *(If this project's own reproduction disagrees with the inherited 50 358 / 50 358, that is a finding for the mentor and for the determinism notes, not a number to quietly adopt.)*

### M5 — One parameterisation axis, and a sweep (est. 1.5 days)
[B] step 5. The axis chosen at M2's spike (OQ-4); declared in configuration; swept across a campaign; presented so the trend is visible.
**Validation:** CR-PAR-1, CR-PAR-2. A sweep in which at least one condition actually changes outcome across the range.

### M6 — Assertions, reporting, and the ugly reality (est. 3 days)
[B] steps 6, 7 and 8. The three condition kinds with a closed vocabulary; verdicts that locate their evidence; the absence-is-not-evidence rule; per-run records; the campaign summary; the diff; the four-fault injection matrix; the 20-run campaign.
**Validation:** CR-AS-1 through CR-AS-4, CR-REP-1 through CR-REP-4, CR-EX-1, CR-EX-5, CR-EX-6, CR-EX-7. All four injected faults produce a continued campaign. Twenty runs unattended, one command. OQ-5 resolved.

### M7 — Evidence and documentation (est. 1 day)
README with all four topics [B] names, including the limits section; the committed 20-run campaign; the determinism notes with their unexplained-observations section; the 5-minute recording.
**Validation:** CR-DOC-1, CR-DOC-2, and every success metric. **The recording needs a person and is scheduled here explicitly, because the equivalent deliverable was not delivered upstream** (R10).

**Estimated total: 11–12 working days**, against [B]'s *"2–3 weeks"*. **This is the plan's estimate, not a measurement.** The end-of-run predicate (M2) is the largest single unknown and the item most likely to breach it — flagged as R2 with containment defined rather than hoped for.

## Review checklist

- [x] All requirements have acceptance criteria
- [x] All P1 FRs have **Customer scenario** + **Pain removed** fields populated
- [x] All P1 FRs have a corresponding `UAC-{FR-ID}` entry in Appendix B
- [x] Naming and interface conventions subsection present at top of FR section, **with the mechanism that keeps each authority true**
- [x] Scope Authority subsection present, including the no-EXT-08-source clause
- [x] Out of Scope section present with structured entries (status / rationale / target / revision)
- [x] Open Questions table has decision target + rationale on every entry; no owner column
- [x] Security implications assessed
- [x] Cross-repo dependencies documented, including three known upstream gaps
- [x] Migration plan reviewed (capture format-version migration)
- [x] Test plan covers all P1 requirements
- [x] Rollback strategy defined
- [x] Key hypotheses are falsifiable, each with a stated consequence if false
- [x] Success metrics have baselines — all "does not exist", stated plainly
- [x] **Every FR, constraint and acceptance criterion carries a provenance marker, and no [QUOTED] marker cites anything but [B]**
- [x] **Every requirement stricter than [B]'s words is marked [ORIGINATED]**, with the decider and the reason
- [x] **No requirement is written that is already known to be unmeetable** — the one such case (R1) is restated, marked as a deviation, and escalated

---

## Appendix A: Traceability

**Forward: every requirement to its source.** "[B]" means the requirement quotes the brief. "[B]+O" means it quotes the brief and adds an originated clause, flagged in the requirement. "O" means fully originated by this project.

| Requirement | Source | Provenance | Goal |
|---|---|---|---|
| CR-EX-1 | [B] rule 1 (isolation) | [B]+O (refuse-to-start clause) | G1 |
| CR-EX-2 | [B] step 2; [B] surface table (invocation) | [B]+O (the specific invocation) | G1 |
| CR-EX-3 | [B] step 2 ("harder than it sounds") | [B]; predicate itself open (OQ-1) | G1, G4 |
| CR-EX-4 | [B] rule 2; acceptance criterion 5 | [B] | G2 |
| CR-EX-5 | [B] rule 3; acceptance criterion 5 | [B]+O (structural-drop clause) | G2 |
| CR-EX-6 | [B] step 8; acceptance criterion 8 | [B] | G1, G2 |
| CR-EX-7 | [B] acceptance criterion 1 | [B] | G1 |
| CR-CAP-1 | [B] step 3; acceptance criterion 7 | [B] | G5 |
| CR-CAP-2 | [B] step 3 ("Subscribe as in EXT-08") | **O** — the format choice is ours (ADR-2) | G5 |
| CR-CAP-3 | — | **O** — from `contract/` §3, §13 | G5 |
| CR-CAP-4 | — | **O** — from `contract/` §5.1, §8.1, §16; verified [C3] | G2, G4 |
| CR-CAP-5 | — | **O** — a campaign is many captures; the upstream bound is per file (R3, rev 2) | G1 |
| CR-DET-1 | [B] step 4; paragraph 16; acceptance criterion 2 | **[B]+O — NAMED DEVIATION** (content basis; ADR-1, OQ-2) | G4 |
| CR-DET-2 | [B] rule 4; paragraph 18 | [B]+O (locale specificity) | G4 |
| CR-DET-3 | [B] paragraph 16 ("tell which") | [B] | G4 |
| CR-PAR-1 | [B] parameterisation; step 5 | [B]; axis open (OQ-4) | G3 |
| CR-PAR-2 | [B] acceptance criterion 3 | [B]+O (varying-result criterion) | G3 |
| CR-AS-1 | [B] acceptance criterion 4 | [B]+O (fail-before-start clause) | G5 |
| CR-AS-2 | [B] acceptance criterion 4; step 6 | [B]+O (verdict cardinality) | G2, G5 |
| CR-AS-3 | [B] assertion paragraph | [B]+O (closure; units) | G5 |
| CR-AS-4 | — | **O** — from measured loss, upstream R7 open (ADR-6) | G2 |
| CR-REP-1 | [B] reporting paragraph | [B] | G2 |
| CR-REP-2 | [B] reporting paragraph | [B] | G2 |
| CR-REP-3 | [B] acceptance criterion 5 | [B] | G2 |
| CR-REP-4 | [B] acceptance criterion 6; paragraph 9 | [B]+O (frozen-clock exclusion) | G3, G4 |
| CR-DOC-1 | [B] deliverable 2 | [B]+O (limits contents) | G5 |
| CR-DOC-2 | [B] deliverables 3–5 | [B] | G4, G5 |

**Backward: [B]'s eight acceptance criteria, and where each is discharged.**

| # | [B] acceptance criterion | Discharged by |
|---|---|---|
| 1 | *"A campaign of at least twenty runs executes unattended, start to finish, with no manual step."* | CR-EX-7, supported by CR-EX-1, CR-EX-2, CR-EX-6 |
| 2 | *"The same-configuration self-test passes: two identical runs produce identical captures, and the tool checks this itself."* | **CR-DET-1 — with a named deviation on what "identical" means. See ADR-1 and OQ-2. This is the only criterion this PRD does not discharge as written.** |
| 3 | *"A parameter sweep shows a result that varies with the parameter, presented so the trend is visible."* | CR-PAR-1, CR-PAR-2 |
| 4 | *"Assertions are declared separately from the code that runs the simulation, and a failure names what was checked and on what data."* | CR-AS-1, CR-AS-2 |
| 5 | *"Pass, fail, timeout and infrastructure error are four distinct outcomes in the report."* | CR-EX-4, CR-EX-5, CR-REP-3 |
| 6 | *"A diff between two runs identifies the first point of divergence, not just that they differ."* | CR-REP-4 |
| 7 | *"Stored runs can be re-judged against new assertions without re-running."* | CR-CAP-1, supported by CR-CAP-2 |
| 8 | *"A host crash mid-campaign is survived: the campaign continues and the report says what happened."* | CR-EX-6, CR-EX-5 |

**Backward: [B]'s five deliverables.** (1) A git repository with the runner — this repository. (2) README — CR-DOC-1. (3) A real campaign committed as an example — CR-DOC-2, CR-EX-7. (4) A 5-minute recording — CR-DOC-2, R10. (5) A page of notes on determinism — CR-DOC-2, CR-DET-3.

**Backward: [B]'s seven rules.** Isolation → CR-EX-1. Timeout as a distinct state → CR-EX-4. Infrastructure never a test result → CR-EX-5. Determinism first → CR-DET-2. Simulation time, not your own clock → C4, CR-DET-2. Store enough to re-judge → CR-CAP-1. Never throw → C3.

**Backward coverage summary.** *(Corrected during the rev-1 conformance audit: an earlier count of 33 omitted the surface table, the diffing paragraph and the determinism section, which made the tally look more complete than the inventory actually was.)*

| [B] element | Items | Covered | Notes |
|---|---:|---:|---|
| Header line (track, language, effort, prerequisite) | 4 | 4 | C1, C2, C6 |
| "What you build" statement | 1 | 1 | Purpose and scope |
| The four pieces | 4 | 4 | CR-EX, CR-PAR, CR-AS, CR-REP groups |
| Run-to-run diffing paragraph (both halves) | 2 | 2 | CR-DET-1 (same config → same results), CR-REP-4 (changed input → divergence point) |
| "Why it is worth doing" | 1 | 1 | Problem statement |
| Surface table rows | 6 | 6 | Client + import libraries → §"Linkage boundary"; `EntityStateSample.h` → **R7, a defect**; headless host → CR-EX-2 + OQ-3; catalogue queries → CR-PAR-1; scripting namespaces → **Alternatives, Option 5, not chosen** |
| Determinism section (p15, p16, p18, p19) | 4 | 4 | ADR-1, CR-DET-1, CR-DET-3, CR-DET-2, C5. **p15's "every machine" is untested — stated in ADR-1** |
| Steps | 8 | 8 | M1–M7 follow the step order |
| Rules | 7 | 7 | CR-EX-1, CR-EX-4, CR-EX-5, CR-DET-2, C4, CR-CAP-1, C3 |
| Acceptance criteria | 8 | 8 | **1 with a named deviation** (criterion 2) |
| Deliverables | 5 | 5 | CR-DOC-1, CR-DOC-2 |
| Stretch goals | 3 | 3 | All deferred with dated Out-of-Scope entries |
| **Total** | **53** | **53** | |

**Fully covered: 52. Covered with a named deviation: 1** (acceptance criterion 2 — see ADR-1, OQ-2). **Dropped: 0.** One element is covered by *declining* it on stated evidence rather than by implementing it — the scripting-namespace route (Option 5) — and one is covered by *reporting it as a defect in [B]* rather than by building to it (`EntityStateSample.h`, R7).

## Appendix B: User acceptance criteria

### UAC-CR-EX-1: Every run gets a fresh host
**GIVEN** a campaign of twenty runs, and a host process left running by a previous crashed campaign
**WHEN** the campaign is launched
**THEN** it refuses to start with a named error naming the foreign process; and **WHEN** re-launched with no foreign host, every run's capture reports `attached_mid_run: false` and zero orphaned samples, and no host process survives any run

### UAC-CR-EX-2: Bring-up without a sleep
**GIVEN** a workstation under variable load
**WHEN** a run starts the host, waits for readiness, loads the scenario and starts it
**THEN** each wait completes on an observed condition with a bounded logged timeout, no fixed delay appears in the path, the recorder is attached before the roster burst, and a host that never becomes ready is reported `infrastructure_error`

### UAC-CR-EX-3: An explicit end-of-run definition
**GIVEN** twenty runs of one configuration
**WHEN** each run ends
**THEN** each ended because the stated stop predicate was satisfied, the predicate's value is in every per-run record, no wall-clock quantity participated in the decision, and no run ended by timeout

### UAC-CR-EX-4: A timeout on every run, as its own outcome
**GIVEN** a scenario configured so that one run cannot reach its stop predicate
**WHEN** the campaign reaches that run
**THEN** the run is torn down at its timeout, its partial capture is retained and marked, it appears as `timeout` and not as `fail`, and the four outcome counts still sum to the runs attempted

### UAC-CR-EX-5: Infrastructure failure is never a test result
**GIVEN** a campaign in which two hosts fail to start and one condition is genuinely not met
**WHEN** the report is read
**THEN** it shows one `fail` and two `infrastructure_error`, never three failures; and a run whose capture reports non-zero `events_not_recorded` is `infrastructure_error` rather than `fail`

### UAC-CR-EX-6: The four ugly realities are survived
**GIVEN** a twenty-run campaign with each of the four faults injected on a different run
**WHEN** the campaign completes
**THEN** all twenty runs were attempted, each faulted run is named with its fault and lifecycle point, no host process survives, and the campaign exit code distinguishes "ran, some failed" from "could not run"

### UAC-CR-EX-7: Twenty runs unattended
**GIVEN** the committed example campaign configuration
**WHEN** one command is issued
**THEN** twenty or more runs execute to a report with no prompt, no dialog, and no manual host start or teardown

### UAC-CR-CAP-1: Recording separate from assertion
**GIVEN** a completed campaign and a condition file containing a condition that did not exist when it ran
**WHEN** re-judgement is invoked over the stored campaign
**THEN** verdicts are produced with no host started and no bus subscription made, and verdicts for unchanged conditions are identical to the live ones

### UAC-CR-CAP-2: A conformant reader written from the specification alone
**GIVEN** the vendored fixture and four deliberately mutated copies
**WHEN** the reader is run over each
**THEN** the fixture parses completely with counts agreeing with its trailer, each mutation produces a distinct named error, unknown keys in known record types are ignored, and the binary links no EXT-08 code and no N8RO SDK

### UAC-CR-CAP-3: An unrecognised format version is rejected
**GIVEN** a capture whose `format_version` reads `n8ro-capture/2`
**WHEN** the reader opens it
**THEN** it rejects the file with a named error naming both expected and found versions, before parsing any further record, and the run is classified `infrastructure_error`

### UAC-CR-CAP-4: Segment- and occupancy-aware reading
**GIVEN** the vendored fixture, which contains two segments and 42 entities re-created at occupancy 2
**WHEN** any statistic, verdict or comparison is computed
**THEN** it is scoped to a named segment; the frozen-clock segment is detected by the exact maximum-samples test and excluded from comparison; a re-created name is treated as a distinct entity; and no code path sorts the file globally by `sim_time_s`

### UAC-CR-CAP-5: The campaign bounds its own disk usage
**GIVEN** a machine with less free space than the campaign's projected footprint
**WHEN** the campaign is launched
**THEN** it refuses to start, naming both the projection and the available space; and a campaign that reaches its ceiling mid-run stops with a named outcome leaving completed runs and the report valid

### UAC-CR-DET-1: The same-configuration self-test
**GIVEN** two runs of one configuration, both bounded by the same stop predicate
**WHEN** the self-test runs at the start of a campaign
**THEN** the content comparison — per `(entity, occupancy)` value sequences aligned on `sim_time_s`, running segments only — reports every compared sample agreeing; the byte comparison is run and reported separately with its expected-to-fail status stated; a capture with non-zero `samples_not_recorded` is excluded rather than diffed; and the result appears in the campaign report; **and** both runs yield the same verdicts and the same run outcome, reported as its own line

### UAC-CR-DET-2: Nothing of ours varies between runs
**GIVEN** the comparison path and a machine configured with a comma-decimal locale
**WHEN** the determinism unit tests run
**THEN** each of the three hazards the brief names — a clock read, a timestamp in compared output, an unordered container iterated — has a dedicated failing-if-reintroduced test, and number formatting is identical under both locales

### UAC-CR-DET-3: A failed self-test is attributable
**GIVEN** two captures deliberately made to diverge
**WHEN** the self-test fails
**THEN** the report names the first differing record and its position, states whether headers and record counts agree, and attributes the divergence to a segment and an `(entity, occupancy)` rather than to a line number

### UAC-CR-PAR-1: One axis, declared in configuration
**GIVEN** a campaign configuration declaring one axis and twenty values
**WHEN** the campaign runs
**THEN** each run used its declared value, the value appears in the per-run record and the report, no rebuild was needed, and two runs sharing a value form a valid input to the self-test

### UAC-CR-PAR-2: A visible trend
**GIVEN** the committed sweep campaign
**WHEN** a reviewer opens the report
**THEN** runs are ordered by parameter value with the result shown against it, at least one condition changes outcome across the range, and the trend is legible without opening another tool

### UAC-CR-AS-1: Conditions declared outside the code
**GIVEN** a condition file containing a duplicate id, and separately one containing an unrecognised kind
**WHEN** a campaign is launched with each
**THEN** each produces a distinct named parse error and a non-zero exit before any host is started, and no campaign ever runs with silently zero conditions loaded

### UAC-CR-AS-2: Verdicts locate their evidence
**GIVEN** a run in which one condition is met and one is never met
**WHEN** the verdicts are read
**THEN** there is exactly one verdict per declared condition, the never-met one is an explicit not-met rather than silence, and each carries condition id, entities with occupancies, segment, deciding `sim_time_s`, and values that can be recomputed by hand from the samples named

### UAC-CR-AS-3: A closed condition vocabulary
**GIVEN** a condition file exercising proximity, area and terminal-state conditions, including one of each that is never met
**WHEN** it is evaluated
**THEN** all three kinds produce verdicts, units are the platform's own with no conversion applied, and a fourth kind added to the file produces a named error before any run starts

### UAC-CR-AS-4: Absence is never evidence
**GIVEN** a condition that can only be decided by the absence of a record
**WHEN** it is evaluated against a capture whose counters all read zero
**THEN** it reports `indeterminate` with its reason, never `pass`; the indeterminate verdict is visible in the report and folded into none of the four run outcomes; and the README's limits section states what a pass proves

### UAC-CR-REP-1: A result per run and a campaign summary
**GIVEN** a completed twenty-run campaign
**WHEN** the report is opened
**THEN** there is one record per run carrying identifier, parameter value, outcome, verdicts, stop-predicate value and capture path; and one summary carrying the four outcome counts, the self-test result and the sweep — both machine-parseable and legible to a person

### UAC-CR-REP-2: Enough detail to go and look
**GIVEN** a failing run in the report
**WHEN** an analyst follows its failure detail
**THEN** the capture file, segment, `(entity, occupancy)` pairs and deciding `sim_time_s` are named, those coordinates locate the causing records by hand, and the detail is in the machine-readable record rather than only in console output

### UAC-CR-REP-3: Four distinct outcomes
**GIVEN** a campaign containing at least one of each outcome
**WHEN** the summary is read
**THEN** four separate counts appear, they sum to the runs attempted, no aggregate collapses two of them, and indeterminate verdicts are reported alongside rather than merged in

### UAC-CR-REP-4: A diff naming the first divergence
**GIVEN** two runs differing in one parameter
**WHEN** they are diffed
**THEN** the output names the first divergence by segment, `(entity, occupancy)`, `sim_time_s` and field; frozen-clock segments are excluded and the exclusion is stated; and "present in one, absent in the other" is distinguished from "present in both with different values"

### UAC-CR-DOC-1: README covering all four topics
**GIVEN** a campaign author who has never seen this repository
**WHEN** they read only the README
**THEN** they can configure a campaign, write an assertion, interpret the output, and state the limits — including what a pass proves, the determinism gate's basis, the stop predicate and the disk ceiling — and the documented CLI matches the binary, enforced by the golden-file test

### UAC-CR-DOC-2: The evidence pack
**GIVEN** the committed repository
**WHEN** a reviewer runs the committed campaign configuration and watches the recording
**THEN** the campaign reproduces, the report matches the committed one in structure, the determinism notes state what was needed to make comparison meaningful and carry an unexplained-observations section, and the 5-minute recording shows launch, run and report

## Appendix C: Architecture decision records

> ADR stubs for the decisions this PRD makes. Expand them into full ADRs in the repository's decision log as implementation proceeds.

### ADR-1: The determinism gate is keyed on content, not on bytes
**Status:** Proposed — **escalated, not settled (OQ-2)**
**Context:** [B] asserts the platform guarantee this whole project rests on — *"The simulation is deterministic by contract: the same scenario, the same inputs and the same ordering produce the same outputs, on every run and every machine"* — and requires a self-test showing that two identical runs *"produce identical captures"* and that they *"match"*, making it a hard stop: *"Do not build further until it passes."* **The measurement does not contradict [B]'s contract claim; it locates it.** The same outputs *are* produced — every sample present in both runs at the same simulation instant carries byte-identical values. What differs is which frames were **published**, which is a property of the publication schedule rather than of the simulation's outputs. Reading the measurement as "the platform is not deterministic" would be wrong, and this ADR does not say it. [B] never says at what level of representation, and the words *byte* and *hash* do not appear in it [C3]. Measured on the shipped headless host with both runs stopped at exactly frame 1200: byte comparison **fails**; content comparison per `(entity, occupancy)` aligned on `sim_time_s` over running segments finds **50 358 of 50 358 samples agreeing, zero differing** [C1]. The runs disagree only about which ~0.2% of frames were published, differently each time.
**Decision:** Key the gate on content, exclude frozen-clock segments, align but do not key on `sim_time_s`, and run the byte comparison alongside as a reported observation that is never engineered to pass. **Mark the change as this project's decision, not the client's, and escalate it for a ruling before M4 opens.**
**Alternatives rejected:**
- *Keep the byte gate as written.* The most defensible reading of [B] and the safest-looking choice — and it ends the project at milestone 4, since it cannot pass on either available host. It also measures the publication schedule rather than the simulation. **Rejected as the implemented default, but not dismissed: it is put to the owner of [B] as OQ-2, with the measurement attached.**
- *Normalise or filter the captures until a byte comparison passes.* Produces a comparison that passes by construction and measures nothing. This is the rabbit hole named in §"Rabbit holes".
- *Drop the self-test.* Discards the property [B] says everything rests on, and the property genuinely holds — which is the whole point.
**Consequences:**
- The gate becomes passable and measures the thing it exists to protect.
- **It is a weaker test than the brief's strictest reading, and this document says so in the requirement, in the traceability appendix, and here** — rather than quietly satisfying a criterion it has redefined. That transparency is the entire lesson inherited from the upstream project, where a project-originated determinism requirement was later read as brief-traced and had to be rescoped twice [C2].
- M4's first act reproduces the measurement locally. A gate justified by a number nobody here has checked is not a gate.
- **[B] claims the guarantee holds *"on every run and every machine"*. This project measures one machine.** Nothing here tests cross-machine reproducibility, and no claim about it is made. If a second machine becomes available, the same self-test run against captures from both is the cheapest possible check — recorded here so the untested half of [B]'s claim stays visible rather than assumed.

**Supersedes:** None

### ADR-2: EXT-17 consumes the frozen capture artifact and reads no EXT-08 source
**Status:** Accepted
**Context:** [B] says *"Capture the run. Subscribe as in EXT-08"* and nothing more about format or boundary. The two projects are separate repositories with no shared source. `n8ro-capture/1` is frozen, self-describing, vendored read-only in `contract/`, and was demonstrated readable from its specification alone by a conformance reader that linked neither the bridge nor the SDK [C1].
**Decision:** EXT-17's input is the capture format specification and capture files. It drives **the host and the recorder** as processes, never as libraries, and reads no EXT-08 header, source file, or class name. **This is a boundary against EXT-08, not against the platform**: EXT-17 does link the N8RO SDK on its control path, because publishing the scenario and engine commands CR-EX-2 requires needs a bus client — which is what [B]'s surface table points at. See §"Naming and interface conventions" for the three-way split.
**Consequences:**
- The boundary the repo split exists to enforce actually holds, and is checkable: no EXT-08 identifier appears in EXT-17's source (M3 gate).
- A defect in the specification is a defect EXT-08 owns and fixes — that is `contract/PROVENANCE.md`'s own rule — rather than something EXT-17 reads around. R4 is being handled that way.
- EXT-17 inherits upstream gaps it cannot fix (R3, R5) and must state them as risks rather than assume them away.
- **It also means EXT-17 does not need `EntityStateSample.h`**, which [B]'s surface table cites and which does not exist (R7). A project that built its own recorder would have hit that wall; consuming the artifact walks around it.

**Supersedes:** None

### ADR-3: Isolation is enforced by the process boundary, not by cleanup
**Status:** Accepted
**Context:** [B] rule 1 requires that nothing crosses between runs — *"not a file, not a cached handle, not a still-running host."* An upstream harness tried the opposite first: one host shared across twenty cycles. Every cycle after the first attached mid-run and recorded nothing but orphaned samples — twenty runs, one usable [C1].
**Decision:** A fresh host process per run, torn down on every exit path including both fault paths. No cleanup routine is trusted to restore a shared host to a pristine state, because none was ever written that could. A run refuses to start if a foreign host is live.
**Alternatives rejected:**
- *One long-lived host with repeated scenario loads.* Much faster, forbidden by [B], and measured worthless. Recorded in §"Performance requirements" so nobody reaches for it as an optimisation later.
- *Shared host with an explicit reset between runs.* Requires trusting a reset nobody can verify, against an engine whose stop path already re-creates its entire roster under the same names.
**Consequences:**
- Isolation is structural rather than remembered, which is what makes H2 falsifiable by a simple per-run structural check.
- Per-run bring-up cost is paid twenty times. Accepted: correctness of an unattended campaign dominates its wall-clock time.
- Process management becomes EXT-17's problem, including terminating only handles it created — which the threat model requires anyway.

**Supersedes:** None

### ADR-4: The run's end is a stated predicate over the published stream
**Status:** Proposed — **the predicate itself is OQ-1, decided at M2**
**Context:** [B] says *"decide what defines the end and make it explicit rather than a sleep"* and warns it is harder than it sounds. EXT-08 explicitly refused the problem as EXT-17's [C2]. Two runs bounded differently cannot be compared, so this decision gates ADR-1's self-test.
**Decision:** The end is a predicate over what the run published, stated in the README and recorded per run. Wall-clock time participates only as the CR-EX-4 timeout backstop, which is a distinct outcome and never the definition. **A frame budget is the leading candidate** — it is observable, clock-free, and identical across two runs by construction, and it is what made the upstream determinism measurement possible [C1] — but the choice is made at M2 against OQ-1's four criteria, not here.
**Alternatives rejected:**
- *A wall-clock run budget.* Two runs stopped after the same duration have not covered the same simulation; their captures then differ for a reason unrelated to determinism [C1]. It would make the gate unpassable under any comparison basis.
- *Quiescence detection — stop when the scenario goes quiet.* Attractive, and it is an **absence test**, which tenet 3 and CR-AS-4 both reject on a stream with unreported loss: a dropped frame reads as quiescence.
**Consequences:**
- The self-test has a well-defined input, which is its precondition.
- If no candidate satisfies all four criteria at M2, that is an escalation rather than an implementation choice — stated in the M2 gate.

**Supersedes:** None

### ADR-5: The condition vocabulary is closed at the three kinds the brief names
**Status:** Proposed — **adopt-or-supersede of EXT-08's file schema is OQ-5**
**Context:** [B] names three kinds of question: proximity between two entities, an entity reaching an area, and an entity reaching a terminal state. EXT-08 shipped a documented three-kind schema and explicitly resolved that EXT-17 may adopt or supersede it — a permission, not an instruction [C1] [C2]. [B] mentions no file format at all.
**Decision:** Close the vocabulary at the three kinds. An unrecognised kind is a named parse error and a non-zero exit before any run starts, never a skipped condition. Adopt EXT-08's file schema unless a required condition cannot be expressed in it, and record which in the README.
**Consequences:**
- The expressiveness rabbit hole is contained by the PRD-revision requirement rather than by discipline.
- Adopting the schema makes EXT-08's live verdicts and EXT-17's re-judgements directly comparable, which is a cheap conformance check on both.
- Superseding stays available at no cost, because the schema is small and documented — which is exactly why EXT-08 kept it small.

**Supersedes:** None

### ADR-6: Absence is not evidence — assertion semantics over a known-lossy stream
**Status:** Accepted
**Context:** A capture is a very high-fidelity sample of the published stream, not a guaranteed-complete transcript. Loss has been measured with **every platform counter reading zero**: 30 samples in a single frame, 0.023%. It is frame-shaped, is not driven by rate, and appears even in an artifact written inside the host process with no bus in its path — so no consumer configuration avoids it. The upstream risk is **open**, not closed [C1] [C2]. [B] says nothing about this, and one of its own example conditions — *"did anything reach a terminal state it should not have"* — is naturally implemented as an absence test.
**Decision:** No assertion may conclude that an event did not occur from its absence in a capture. Absence-dependent conditions report an explicit `indeterminate` verdict state, distinct from pass and fail, with the reason. `indeterminate` is a **verdict** state and never a fifth **run** outcome, so [B]'s four-outcome criterion stays exactly satisfied.
**Alternatives rejected:**
- *Treat absence as falsity.* The natural implementation, and it returns a confident pass from a file that is missing the frame in which the thing happened. The file looks perfectly clean, which is what makes it dangerous.
- *Refuse absence-dependent conditions entirely.* Would exclude one of the three kinds [B] names. Reporting indeterminate keeps the condition expressible and its limits honest.
- *Wait for the upstream risk to close.* It is unattributed and the mechanism sits upstream of any consumer; there is nothing to wait for.
**Consequences:**
- Some questions get "cannot be decided from this data" instead of an answer. That is the correct answer, and the README's limits section says so.
- The report gains a state the campaign author must understand, which is a documentation cost accepted in exchange for not shipping confident wrong passes.
- If a future producer ships a counter that positively bounds completeness, this ADR is revisited and some indeterminate cases become decidable.

**Supersedes:** None

## Quality gate notes

Advisory only — these do not block the PRD.

**Notable gaps**

- **The determinism gate is deviated from and escalated, not satisfied.** [B]'s acceptance criterion 2 is the one criterion this PRD does not discharge as written. That is stated in the requirement, in ADR-1, in the traceability appendix and here — four places, deliberately, because the failure mode being guarded against is exactly a deviation that becomes invisible. *Suggestion: OQ-2 is the first thing to take to the mentor and to the owner of [B]; it gates M4, which [B] makes a hard stop.*
- **Every performance figure is inherited or derived; none is this project's own measurement of this project's runs.** §"Performance requirements" says so per row. *Suggestion: fill them at M3 and revise.*
- **The end-of-run predicate is unchosen (OQ-1), and it gates the self-test.** This is the largest schedule unknown in the plan and the reason M2 is the longest early milestone. It is named rather than estimated away.
- ~~**The parameterisation axis is unchosen (OQ-4) and may be constrained by a read-only install tree (R9).** No axis has been shown feasible yet. *Suggestion: the M2 spike should establish feasibility for at least two of [B]'s three axes so the choice at M5 is not forced.*~~ **Discharged at M2, recorded at rev 3**: the spike established feasibility for **all three**, exceeding what the suggestion asked for. The axis remains unchosen, which is OQ-4's schedule rather than a gap.

**Minor gaps**

- **No customer quote was written.** [B]'s "customers" are a campaign author, an analyst and a mentor; a quote would be invented rather than sourced. The customer-scenario field on each FR carries that weight instead.
- **The CLI is deliberately not enumerated in this PRD.** That is a departure from the template's convention-table pattern, taken because a prose option list in a document nobody executes is exactly what drifted upstream [C2]. The authority is the checked-in golden `--help` output with a build-time comparison. *If a reviewer prefers the list in the PRD, it should be generated into it, not typed.*
- **OQ-2 is filed as Needs Input at rev 1**, which is unusual — most questions earn that status by ageing. It is correct here: the implementer must not close it, and filing it as merely Open would invite exactly that.

**Requirement smell scan**

Re-read of all P1 requirement statements found no vague adjectives, superlatives, loopholes ("where feasible", "if possible"), open-ended lists, or comparatives without baselines. Three items worth noting:

- **CR-EX-3 defers its central value to OQ-1.** Intentional — the predicate depends on an M2 measurement — but the requirement is a shape without a number until then, and that is a real gap rather than a stylistic one.
- ~~**CR-CAP-5's ceiling is unset (OQ-6)**, for the same reason: it depends on a size this project has not yet measured for its own runs.~~ **Set at rev 3** to 8 GiB over the campaign directory, from this project's own measurement of its own runs. The observation stands as written: it was a real gap, and it closed the way it said it would — by measuring, not by choosing a number.
- **CR-AS-4 introduces a verdict state [B] does not name.** The interaction with [B]'s four-outcome criterion is called out inside the requirement itself, because a fifth state appearing anywhere near that criterion is the kind of thing a reviewer should be able to check in one place.

**Backward traceability**

Source items checked: **53** from [B] — see the itemised table in Appendix A. Fully covered: 52. Covered with a named deviation: 1 (acceptance criterion 2). Dropped: 0. *The rev-1 audit corrected this count from 33: the original inventory omitted [B]'s surface table, its diffing paragraph and its determinism section. All were in fact addressed in the text — but a coverage claim whose denominator is wrong is not a coverage claim, and two genuine gaps (the scripting-namespace route and the catalogue-query dependency) were only found by rebuilding the inventory.*

**Provenance audit** — the check this PRD exists to pass.

- Requirements quoting [B]: 20. Requirements fully originated by this project: 4 (CR-CAP-3, CR-CAP-4, CR-CAP-5, CR-AS-4). Requirements quoting [B] with an originated clause flagged inside them: 3 marked `[B]+O` in Appendix A beyond those, plus CR-DET-1's named deviation.
- **No `[QUOTED]` marker in this document cites anything other than [B].** `contract/`, EXT-08's PRD, EXT-08's escalations, this project's own measurements, and the briefing notes supplied with the request are all labelled [C1]–[C4] and are cited only under `[DERIVED]`-from-context or `[ORIGINATED]`.
- **No second binding source was created.** This is the specific failure this document was written to avoid.
