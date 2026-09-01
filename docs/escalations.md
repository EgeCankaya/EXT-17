# Escalations and questions out

One row per question that leaves this project. A question is only ever marked **Answered** when
a reply exists; "asked" and "answered" are separate things because conflating them is how a
project ends up believing it has a ruling it never received.

**Four states, not two.** `drafted` means written and **not delivered** — nobody has been told.
`sent` means delivered and awaiting a reply. `answered` means a reply exists. A finding that is
written down but not delivered is *recorded*, not *raised*, and the difference is the whole reason
this file has a status column.

**`decided` was added on 2026-09-01, and it is the one to read carefully.** It means *a person
entitled to decide it did, on stated evidence, and the original recipient still has not spoken*.
It is stronger than `sent` and **weaker than `answered`**. Reading `decided` as `answered` is
precisely the failure this table exists to prevent, so every row carrying it says which in the
same sentence, and so does every report the tools print.

**As of 2026-09-01 one row is `decided` and one is partly answered.** E-2 (OQ-2) is decided and
has never been answered by [B]'s author. E-1 (OQ-3) has **four of its six parts answered** by the
mentor, and two — (a) and (c) — **were not covered in what was relayed**, so those two stay
`decided` rather than answered. That distinction was preserved deliberately: an answer nobody
gave is not recorded as one, and the rule applies to a mentor exactly as it applies to [B]'s
author.

Sections below are in the order they were last written, not in numerical order; the table is the
index. `docs/findings.md` indexes these alongside every other issue this project has found.

| # | Question | To | Raised | Status |
|---|---|---|---|---|
| E-1 | **OQ-3** - is this the intended production invocation of the headless host? | Mentor | 2026-08-31 (M1) | **PARTLY ANSWERED, 2026-09-01.** The mentor confirmed **(b)** `N8RO_RELEASE` is expected in production, **(d)** the degraded terrain configuration is expected and stays, **(e)** a console control event is the intended shutdown and its non-zero exit is expected, and **(f)** `C:\N8RO\bin` on `PATH` is a known second precondition. Relayed by the DRI. **(a) and (c) were not covered and are NOT recorded as answered** - they remain `decided` on the reading in `m7-oq2-oq3.md` §2. Every confirmed answer matched what was already built, so **nothing changed**; (b) additionally produced **E-6** |
| E-2 | **OQ-2** — is the determinism gate keyed on content or on bytes? | Owner of [B] | 2026-08-31 | **DECIDED by the DRI, 2026-09-01 — content. Still NEVER ANSWERED.** Sent by EXT-08 as its E-1 and re-checked at M4, M5 and M6 with no reply. The DRI authorised deciding it from [B]'s own words; the reading is `docs/m7-oq2-oq3.md` §1 and the deciding sentence is [B]'s statement of what the self-test is *for*. **No code changed** — content was already the default. A ruling from [B]'s author would still be acted on and would still change a default and no code |
| E-3 | **A defect in `contract/`** — §6.7 says a rotated run's totals are the sum of its parts' `counts`; for `segments` that is not true | EXT-08 | 2026-08-31 (M3) | **SENT** — [EXT-08 issue #1](https://github.com/EgeCankaya/EXT-08/issues/1), 2026-08-31. Awaiting a reply. Measured on a real four-part capture. Not worked around: the reader implements what §6.7 says and reports what is true beside it |
| E-5 | **A gap in `contract/`** — `condition-file-schema.md` is a verbatim excerpt of EXT-08's README that stops one heading before *"How distance is computed"* and *"Boundary semantics"*, the two sections every geometric verdict rests on | EXT-08 | 2026-09-01 (M6) | **SENT** — [EXT-08 issue #3](https://github.com/EgeCankaya/EXT-08/issues/3), 2026-09-01. Awaiting a reply. Verified by correspondence against `eedc228` and `main`, since F-19 means it cannot be verified by identity. **Not worked around, and unlike E-3 and E-4 it could not be**: there is no vendored text to implement, so EXT-17 decided the computation itself and states it with its constants |
| E-6 | **A third defect in `contract/`'s source** - EXT-08's `README.md` documents the R8 headless invocation **without** `N8RO_RELEASE`. Following it exactly produces a host that refuses every 42-entity scenario load **while sitting idle rather than failing** | EXT-08 | 2026-09-01 (M7) | **SENT** - [EXT-08 issue #4](https://github.com/EgeCankaya/EXT-08/issues/4), **plus a correction comment**: the issue as first filed cited `PROVENANCE.md` finding 6, which is **not an EXT-08 file** (F-37). The substance was unaffected and is now cited against EXT-08's own README, which is where EXT-17's digest inherited the omission. Recorded as F-17 since M1; the mentor's confirmation on 2026-09-01 that the variable IS expected in production is what moved it from suspected to demonstrated |
| E-4 | **A second imprecision in `contract/`** — §5.1's frozen-clock test is said to detect a reset clock; measured here it also fires on a *duplicated publication of identical values* inside a segment whose clock did not reset | EXT-08 | 2026-08-31 (M4) | **SENT** — [EXT-08 issue #2](https://github.com/EgeCankaya/EXT-08/issues/2), 2026-08-31. Awaiting a reply. Measured on 2 of 42 real captures. Not worked around: the test is implemented exactly as §5.1 states and both shapes are excluded; what M4 added is that the refusal names which shape it found |

---

## E-1 — OQ-3: confirm the headless invocation

**Status: PARTLY ANSWERED, 2026-09-01 - four parts of six.** The mentor was asked and replied;
the answers were relayed by the DRI on 2026-09-01 and are recorded here as **answers**, which is
a stronger word than `decided` and is used deliberately.

| part | question | answer |
|---|---|---|
| **(b)** | Is `N8RO_RELEASE` expected to be set for production runs? | **Yes.** Confirmed |
| **(d)** | Is the degraded terrain configuration expected on this install? | **Yes - leave it as it is.** Confirmed |
| **(e)** | Is a console control event the intended way to end an unattended host, and is a non-zero exit from it expected? | **Yes to both.** Confirmed |
| **(f)** | Is `C:\N8RO\bin` on `PATH` a second, separate precondition? | **Yes.** Confirmed |
| **(a)** | Is the bus-publish route the intended control path? | **NOT COVERED in what was relayed.** Stays `decided`, on [B]'s own first-listed surface |
| **(c)** | Is `SimEngineHost_SharedMemory` right, versus the seven other variants? | **NOT COVERED in what was relayed.** Stays `decided`, on *"campaign runs are for the closed configuration"* and *"determinism first"* |

**(a) and (c) are not recorded as answered, and that is not pedantry.** This project has kept
`drafted` / `sent` / `decided` / `answered` apart since M1 precisely so that a row cannot quietly
acquire an answer nobody gave. The rule applies to a mentor exactly as it applies to [B]'s
author. Both remain decided on the reading in `docs/m7-oq2-oq3.md` s2, and **one short follow-up
would close them**.

**Nothing changed as a result of any answer.** Every confirmed part matched what was already
built and measured across roughly a hundred runs since M1 - which is the useful thing about
having asked: the answers cost nothing to receive, because the shape was already right.

**One answer had a consequence elsewhere.** (b) turns **F-17** from *suspected* to
*demonstrated*: **EXT-08's own `README.md`** documents the R8 headless invocation **without**
`N8RO_RELEASE`, and following it exactly produces a host that refuses every 42-entity load while
sitting idle rather than failing. Now that the variable is confirmed as expected in production,
that omission is a defect worth another project's time — raised as **E-6**.

*E-6 was first filed citing `PROVENANCE.md` finding 6, which is EXT-17's own manifest and not an
EXT-08 file at all. The substance was unaffected; the citation was corrected by a comment on the
issue rather than by a silent edit, and the mistake is recorded as **F-37**.*

**Its previous statuses, for the record:** `decided` by the DRI (2026-09-01, superseded by this
reply); before that **DEFERRED BY DECISION**; and before that **DRAFTED, NOT SENT** across four
milestones since M1. [B]'s surface table is what asked for this in the first place - *"the host
binary that runs an engine with no GUI. **Confirm the invocation with your mentor**"* - and
EXT-08 closed its own copy of the question and passed it here, on the grounds that EXT-17 is the
project that runs the host in production.

### The question

> Is this the intended production invocation of the headless simulation host?
>
> ```
> set N8RO_RELEASE=C:\N8RO
> n8ro-sim-app.exe --sim-config SimEngineHost_SharedMemory ^
>                  --model-path C:\N8RO\data\db ^
>                  --schema-file N8roSimSchema
> ```
>
> It takes **no scenario argument**. Loading is a separate publish of
> `{"command":"load_scenario","scenarioName":"…","modelName":"…"}` on `sim/scenario/command`, and
> starting is `{"command":"start"}` on `sim/engine/command`. Verified working end to end at M1.
>
> Six specifics, in the order they would change what EXT-17 builds. (a)–(d) were drafted at M1;
> (e) and (f) were added at M2, from what automating the run measured.
>
> **(a) Is the bus-publish route the intended control path at all?** [B]'s surface table also
> offers *"the scenario-control and simulation scripting namespaces"*. EXT-17's PRD declines the
> scripting route (Option 5) and builds on the bus. M1 found the two are the same mechanism —
> `scenarioControl.requestLoadScenario` is documented as *"publishes the command"* — but they
> differ in **where** the command is applied: published from outside the pipeline it takes effect
> during message processing, while a script call from inside is queued to the next frame boundary.
> EXT-17 wants the outside-the-pipeline path. Confirm that is right for unattended use.
>
> **(b) Is `N8RO_RELEASE` expected to be set for production runs?** M1's first attempt did not set
> it, and **every scenario load was refused**: with it unset the host resolves its plugin
> directory from the current working directory, skips the plugin scan, never registers
> `componentPhysics` (supplied by the stock `bin\plugins\sim\n8ro-physics.dll`), and refuses any
> scenario whose entities need it. The host does **not** fail — it sits idle indefinitely, which is
> the dangerous shape for an unattended campaign. Setting `N8RO_RELEASE=C:\N8RO` fixed it. Is that
> the intended provisioning, or is there a supported way to run without it?
>
> **(c) Is `SimEngineHost_SharedMemory` the right transport for unattended campaign use?** The
> install ships seven other `SimEngineHost_*` entries, including
> `SimEngineHost_SharedMemory_BestEffort`. EXT-17 runs host and observers as separate processes on
> one workstation, twenty-plus times in a row, and depends on the published stream being as
> complete as the platform can make it — so a best-effort variant looks actively wrong here, but
> that is inference, not confirmation.
>
> **(d) Is the degraded terrain configuration expected on this install?** Every run floods with
> `TerrainElevationServiceClient` / `GeoidGridModel` errors: there is no elevation service running
> and no geoid grid under `C:\N8RO\data\geoid`. EXT-17 is **deliberately not fixing this** — every
> measurement it inherits from EXT-08 was taken in this configuration, and provisioning terrain now
> would invalidate the comparability of all of it. Confirming that this is the expected state of
> the install, rather than a defect, would let that decision stop being a judgement call.
>
> **(e) Is the host expected to append to a shared log inside the install tree, and is there
> really no shutdown command?** Two things measured at M2 that a campaign has to work around.
> `C:\N8RO\logs\n8ro-logger-n8ro-sim-app.log` is one fixed filename that every host process
> **appends** to — two identical runs produced a file of exactly twice one run's size, carrying
> two startup banners — so a twenty-run campaign accumulates all twenty in one file inside a tree
> it treats as read-only. EXT-17 works around it by recording the file's size before each host
> start and copying only the bytes after it, which is correct but is a workaround. Separately,
> `sim/engine/command`'s vocabulary is closed at `start` / `stop` / `pause` / `step`, so there is
> no way to ask the host to exit; EXT-17 sends a `CTRL_BREAK_EVENT` to the process group it
> created, which shuts the host down in an orderly way (the `.running` marker is removed, no
> crash rename follows) but still reports exit `0xC000013A`. **Is a console control event the
> intended way to end an unattended host, and is a non-zero exit code from it expected?** The
> answer changes how CR-EX-5 tells "the host died" from "we stopped it".
>
> **(f) `C:\N8RO\bin` on `PATH` is a second precondition, separate from `N8RO_RELEASE`.** An
> SDK-linked binary run from any other working directory exits 53 with no output — a missing
> `n8ro-sim.dll`. It is folded into (b): confirming the intended provisioning should cover both.

### Why the answer is worth having even though nothing is blocked

CR-EX-2 marks the invocation **[ORIGINATED]** precisely because [B] does not specify it. If the
answer to (a) is "the scripting namespaces", CR-EX-2's acceptance criteria change shape and
Option 5 has to be reopened. If (b) has a supported alternative, the campaign's per-run
environment setup changes.

**M2 has now built on the current assumption, deliberately, with the cost stated rather than
discovered.** The whole control path is one component — `src/control/EngineControl` — and every
wait, publish and observation the campaign makes goes through it. An answer of "the scripting
namespaces" to (a) is therefore one file to rewrite, not a redesign. That was the reason for the
shape, not a coincidence. The rest of (a)–(f) change configuration and documentation rather than
structure. The answer is still cheaper now than after M4 keys a determinism gate to runs produced
this way.

---

## E-3 — `capture-format-v1.md` §6.7: a rotated run's segment count is not the sum of its parts'

**Status: SENT to EXT-08 on 2026-08-31 as [issue #1](https://github.com/EgeCankaya/EXT-08/issues/1). Awaiting a reply.** It goes to **EXT-08**, not to the brief's author:
`contract/` is vendored and read-only here, and `PROVENANCE.md` states the rule in its own words —
*"If one of them is wrong or insufficient, that is a defect in EXT-08's contract and it goes back
there."* This is the first time that rule has had to be used.

**It does not block.** The format is frozen and this is an imprecision in prose rather than in a
file, so nothing about a capture changes whatever the answer is. What changes is whether every
downstream consumer has to re-derive the correction independently, which is exactly what a
specification exists to prevent.

### The question

> §6.7, "Stitching a set", rule 2 reads:
>
> > **`counts` in each `trailer` is that file's own.** The run's totals are the sum across parts.
> > Nothing in any part states the set's total, because no part knows it — part 0's trailer is
> > written long before the run ends.
>
> For `samples`, `entities_added`, `entities_removed` and `verdicts` that is exactly right. For
> **`segments`** it is not, and the same section says why three paragraphs earlier:
>
> > A segment cut by a rotation appears as a `segment_close` with `reason: "size_limit"` at the
> > end of one part and a `segment_open` at the start of the next — **one segment split across two
> > files, not two segments.**
>
> A cut segment is therefore counted in *both* parts' `counts.segments`, and the sum over-counts
> a run's segments by one per cut.
>
> **Measured here.** One 1200-frame run of `Atacama Air Defense`, recorded with
> `--capture-max-bytes 8000000 --on-size-limit rotate`, produced four parts:
>
> | part | `counts.segments` | `end_reason` | `continued_in` |
> |---|---:|---|---|
> | 0 | 1 | `size_limit` | yes |
> | 1 | 1 | `size_limit` | yes |
> | 2 | 1 | `size_limit` | yes |
> | 3 | 2 | `host_lost` | no |
> | **sum** | **5** | | |
>
> The run had **two** segments — the run and its teardown reload — exactly as an unrotated
> recording of the same scenario produces, and as §16 describes. The other four counters summed
> correctly and matched an unrotated run of the same configuration exactly (`entities_added: 89`,
> `entities_removed: 47`).
>
> Two things would settle it, and either is enough:
>
> **(a) Narrow the sentence.** Something like *"The run's totals for `samples`,
> `entities_added`, `entities_removed` and `verdicts` are the sum across parts. `segments` is not
> summable: a segment cut by a rotation is closed in one part and opened in the next, so it appears
> in both parts' counts. Subtract one per cut — a part carrying both `size_limit` and a
> `continued_in`."*
>
> **(b) Or state that the correction is the reader's** and say how, which is the same sentence
> without the first clause.
>
> This project has implemented (a) and does **not** treat it as settled: `n8ro-capture` computes
> `counts.segments` exactly as §6.7 specifies, computes the corrected run count beside it, prints
> both, and says which is which:
>
> ```
>   counts        summed across parts: segments 5 samples 50449 adds 89 removes 47 verdicts 0
>   segments      5 summed across parts, but the RUN has 2: 3 segment(s) were cut by a rotation
> ```
>
> A confirmation, a correction, or "the sum is what we mean and consumers should not care" are all
> useful answers. What is not useful is two projects quietly computing different numbers from one
> frozen specification.

### Why the answer is worth having even though nothing is blocked

EXT-17 chose `--on-size-limit stop` at OQ-6, so no campaign this project runs will produce a
rotated set — which is exactly why the question should be asked now rather than when one does. The
reader supports rotation because a capture rotated by somebody else still has to be readable here,
and a reader that silently disagreed with the specification about how many segments a run had would
be the kind of quiet divergence the repo split and the frozen format exist to prevent.

`PROVENANCE.md` finding 8 quotes §6.7 for EXT-17's benefit and inherits the same sentence, so a
correction upstream should carry into the next re-pin of `contract/` rather than being noted only
here.


---

## E-2 — OQ-2, decided at M7 by the DRI, and never answered by [B]'s author

**Status: DECIDED (DRI, 2026-09-01) — content. STILL NEVER ANSWERED.**

Both sentences are true and neither replaces the other. Re-checked at M4, M5 and M6 against
EXT-08's own record: its `docs/escalations.md` E-1 — the upstream half of the same question — has
read *"raised with the brief's author 2026-08-31; awaiting a reply"* throughout. **No ruling from
[B]'s author exists on either side, and none is claimed.**

What changed on 2026-09-01 is that schedule became the binding constraint and the DRI authorised
deciding it from [B]'s own words. **The reading is `docs/m7-oq2-oq3.md` §1**, and the sentence it
turns on is not the one usually quoted — it is [B]'s statement of what the self-test is *for*:

> *"If it ever fails, you have found either a defect in your harness or something far more
> interesting, and you must be able to tell which."*

A byte gate fails **190 times in 190** here, identically on a clean harness and a broken one, so
it cannot tell you which case you are in — it defeats the purpose [B] states for the very test
criterion 2 is about. The content gate passed 190 of 190 on a clean harness **and has failed on a
real pair for a real reason** (M6, `campaigns/m6-gate-refused/`), which is [B]'s *"defect in your
harness"* case being caught and named. That is the argument.

**No code changed.** `content` was already the default. What moved is the deviation's status —
from *named and unruled* to *named and ruled, by the DRI rather than by [B]'s author* — and what
the reports are therefore allowed to say.

**A ruling from [B]'s author is still worth having and would still be acted on.** If it says
content, a status line changes from `decided` to `answered`. If it says bytes, `--gate-basis
bytes` already exists and already works, the campaign correctly stops at exit 3 on this hardware,
and that consequence belongs to the ruling rather than to this project.

### What M4 built instead of waiting, which is what made deciding cheap

**M4 did not wait, and did not assume the deviation had been granted.** It built the shape that
survives either answer:

- Both comparisons **always run** and are **always reported** — on every self-test, in the
  campaign log, in `self-test.json` and in `campaign.json`.
- `--gate-basis content|bytes` selects which one **decides**. `content` is the default, and it
  is ADR-1's decision and this project's, not the client's.
- **Under `--gate-basis bytes` the gate correctly fails and the campaign correctly stops** —
  exit 3, and not one campaign run attempted. That is the honest implementation of [B]'s
  strictest reading rather than an argument about it, and it has been run against real captures:
  `campaigns/m4-bytes/`, where the same pair of runs scored `content PASS` (50 361 of 50 361
  samples agreeing) and `bytes FAIL`.
- Every report states, in words, what a content pass does and does not discharge. **Since
  2026-09-01 that wording says `DECIDED (DRI)` and, in the same breath, that [B]'s author has not
  replied** — because "content" on its own would hide which of the two it is. `self-test.json`
  carries it machine-readably as `gate.basis_is_a_named_deviation`, `gate.oq2_ruling`
  (`decided`), `gate.oq2_decided_by` and `gate.oq2_answered_by_brief_author` (`false`).

So the outstanding ruling now changes **a default and no code**. If the answer is "bytes", the
project stops at M4 by [B]'s own instruction and the command that demonstrates it already exists
and has been run. If the answer is "content", the default is already right.

**What the ruling is still needed for**, and why the question stays open: whether [B]'s
acceptance criterion 2 is discharged. This project cannot decide that, and M4's milestone record
says so rather than claiming the gate passed as written.

---

## E-4 — `capture-format-v1.md` §5.1: a duplicated publication is not a reset clock, and one test detects both

**Status: SENT to EXT-08 on 2026-08-31 as [issue #2](https://github.com/EgeCankaya/EXT-08/issues/2). Awaiting a reply.** It goes to **EXT-08**, like E-3 and for the same
reason: `contract/` is vendored and read-only here, and `PROVENANCE.md` states the rule in its
own words — *"If one of them is wrong or insufficient, that is a defect in EXT-08's contract and
it goes back there."* This is the second time that rule has been used, and R11 is the risk that
predicted it.

**It does not block, and nothing has been worked around.** EXT-17 implements §5.1's test exactly
as written, classifies both shapes `frozen`, and excludes both from comparison. What EXT-17
added is that its *refusal* names which of the two shapes it found — because a consumer that
cannot tell them apart cannot tell a platform finding from a harness defect, which is the thing
[B] insists on being able to do.

### The question

> §5.1 gives an exact test for a segment whose clock was reset: the maximum number of samples any
> one `(entity, occupancy)` carries at a single `sim_time_s`, and a value above 1 means the clock
> was reset.
>
> The test is exact and EXT-17 implements it unchanged. The **reading** attached to it — that a
> positive result means the clock was reset — is what this question is about, because a second
> phenomenon satisfies the same test and means something different.
>
> **Measured here.** One ordinary 1200-frame run of `Atacama Air Defense` on the headless host,
> producer 0.9.0, with no parameter manipulation of any kind
> (`campaigns/m4-frozen/selftest/runs/000`):
>
> | | |
> |---|---:|
> | `segment 0` by §5.1's test | **frozen** — max samples per key per `sim_time_s` is **2** |
> | distinct `sim_time_s` in that segment | **1 200**, spanning `0` to `59.999999999998728` |
> | instants published more than once for one `(entity, occupancy)` | **13** |
> | …of which the two records carry **byte-identical values** | **13 of 13** |
>
> The thirteen are the first thirteen entities of the start-up roster burst, published at
> `sim_time_s 0` on lines 45–57 and again on lines 87–99, all at `phase: "uninitialized"`. **The
> clock did not reset** — the segment runs the full sixty seconds across twelve hundred distinct
> instants. Part of the roster burst was simply published twice.
>
> **Frequency, over every capture this project holds** — 42 files. **2** have a frozen segment 0,
> and in **both** every duplicate carries identical values. The other is
> `campaigns/m2-axis/p2-update-post`, a spike run that deliberately published an entity update
> after start and so has a candidate cause. Of the **27 ordinary unmanipulated full runs**, **1**
> is affected — about **3.7%**, and therefore about **7% of pairs** of runs.
>
> That frequency is why this is not a curiosity. A consumer that excludes frozen segments — which
> §14 tells it to, in those words — has roughly a one-in-fourteen chance that a
> same-configuration comparison has nothing left to compare, for a reason that is neither a clock
> reset nor a determinism failure.
>
> Two things would settle it, and either is enough:
>
> **(a) Split the reading from the test.** Keep the test exactly as it is, and say that a positive
> result means *the segment cannot be aligned on `sim_time_s`* — of which a reset clock is one
> cause and a duplicated publication of identical values is another. The two are distinguishable
> by whether the repeated records' values agree, and worth distinguishing because they have
> different causes upstream and different implications for a consumer.
>
> **(b) Or state that the distinction is the consumer's**, and say how. That is the same sentence
> without the first clause.
>
> EXT-17 has implemented (a) and does **not** treat it as settled. Its comparison counts, per
> segment and per run, how many instants were published more than once and how many of those
> repeats carried identical values; it excludes the segment either way; and it names the shape in
> its refusal:
>
> ```
>   REFUSED    no_comparable_segment
>              ... clock is frozen in 000 and running in 001. In 000, 13 instant(s) were
>              published more than once for one (entity, occupancy), 13 of them carrying
>              IDENTICAL values — so this is a DUPLICATED PUBLICATION rather than a reset
>              clock, and the segment is excluded anyway because that is what the format's
>              test says to do (§5.1) and working around it is not this project's to do.
> ```
>
> A confirmation, a correction, or "the distinction is not the format's to make" are all useful
> answers. What is not useful is two projects quietly reading one frozen segment two different
> ways.
>
> **A separate observation, offered rather than asked.** Whatever publishes part of the roster
> burst twice sits upstream of the recorder: the duplicate records are byte-identical and both are
> in the file, so the producer recorded faithfully what it was handed. If that is a known host
> behaviour, naming it in §14 beside the frame-skipping measurement would save the next consumer
> the same investigation.

### Why the answer is worth having even though nothing is blocked

§14 tells a consumer building a determinism self-test to *"exclude frozen-clock segments
entirely"*, and EXT-17 does. The measured consequence is that its gate refuses on about 7% of
pairs. That is the correct behaviour, and it is an operational cost. Whether it is a cost the
format expects consumers to carry, or a symptom worth naming upstream, is not EXT-17's to decide.

`PROVENANCE.md` finding 3 and §14's self-test advice both send a reader to §5.1, so a
clarification upstream would carry into the next re-pin of `contract/` rather than living only
here.

**Strengthened at M6, twice.** M5 added a third shape through §5.1's one test (F-22, a
parameterised run's roster burst republished with *differing* values). M6 found that the same
mechanism has a **fourth observable form and a worse consequence**: instead of making one
segment `frozen`, it can leave the two self-test runs carrying different `velocityNed` values at
`sim_time_s` 0 — which fails the determinism gate and stops the **whole campaign** at exit 3,
rather than costing one run. Measured on the first execution of the twenty-run campaign;
`campaigns/m6-gate-refused/` is that execution, kept. See F-29.

---

## E-5 — `contract/condition-file-schema.md` is a faithful excerpt that stops one heading before the part a consumer needs

**Status: SENT to EXT-08 on 2026-09-01 as [issue #3](https://github.com/EgeCankaya/EXT-08/issues/3). Awaiting a reply.** It goes to **EXT-08** for the reason E-3
and E-4 did: `contract/` is vendored and read-only here, and `PROVENANCE.md` states the rule in
its own words — *"If one of them is wrong or insufficient, that is a defect in EXT-08's contract
and it goes back there."* This is the third time that rule has been used.

**It differs from E-3 and E-4 in one way that changed how it had to be handled, and the
difference is worth stating first.** Those two are *imprecisions in frozen, verbatim-vendored
text*: EXT-17 implements what the specification says and names the gap beside it, and nothing is
worked around because there **is** text to implement. E-5 is a **hole**. There is no vendored
sentence to implement and therefore nothing to defer to, so EXT-17 had to **decide** the
computation and say so on the record. That is `src/assert/Geodesy.h`, which states the method and
its constants so any verdict is reproducible with a calculator, and `docs/m6-oq5.md` §5, which
records how the decision was reached.

### What was found

`contract/condition-file-schema.md` carries a header saying it is *"Vendored from EXT-08 at
commit `eedc228`"* and pointing at *"README.md under 'Declaring conditions'"*.

**Checked by correspondence, since F-19 established it cannot be checked by identity** — no file
of that name exists in EXT-08 at any commit. Both of the digest's own references resolve:

| | |
|---|---|
| `eedc228` resolves to a commit | **yes** — *"PRD rev 9: audit against [S1]…"* |
| `README.md` §"Declaring conditions" exists there | **yes** |
| the digest's content is verbatim | **yes** — §"Declaring conditions" and §"Verdict semantics", word for word, including the key table and the three-line verdict example |
| that section is unchanged at `main` (`eb13485`) | **yes**, byte-identical between the two commits |

So the digest is not fabricated, not paraphrased, and not stale. **It is an excerpt, and it stops
one heading early.**

### The question

> Immediately after the two sections that were excerpted, EXT-08's `README.md` continues into two
> more, and neither crossed into `contract/`:
>
> - **"How distance is computed"** — that positions are converted to **ECEF on WGS-84** and a
>   distance is the straight-line Euclidean distance between them in metres, with Haversine
>   rejected for ignoring altitude and Vincenty for not converging near-antipodally, and a
>   pointer to `src/Geodesy.h` for the constants.
> - **"Boundary semantics"** — that the comparison is `<=`, so a point exactly at `within_m` or
>   exactly on a circle's edge is **inside**; that a point on a polygon's edge or vertex is
>   inside; and that polygons are plane figures in latitude/longitude, unsupported across the
>   antimeridian or a pole.
>
> **Would EXT-08 vendor those two sections into `contract/condition-file-schema.md`?**
>
> **Why it matters, and why it is not tidiness.** The digest documents `within_m` as a threshold
> *"in metres"* and stops. From that alone a consumer implementing the same file format could
> reasonably compute a great-circle distance, or a two-dimensional horizontal separation, and
> would then produce verdicts that **disagree with the producer's on the same capture while
> parsing the same file**. Nothing would surface the disagreement: both projects would report
> `met` and `not met` in the same vocabulary against the same condition ids.
>
> That is silent divergence across the project boundary, which is the failure the whole
> `contract/` discipline exists to prevent. EXT-17's own requirement makes it concrete —
> CR-AS-2's third acceptance criterion is *"a verdict's numbers are reproducible: recomputing
> them by hand from the samples it names gives the same values"* — and that is not satisfiable
> from the digest as vendored.
>
> **It is not drift.** Both sections exist at `eedc228`, the commit the digest itself names,
> directly below the last paragraph that crossed. Nothing was added upstream afterwards.
>
> **A second, smaller part of the same question.** `PROVENANCE.md`'s table lists this file beside
> `capture-format-v1.md` and the sample capture, both of which *are* verbatim vendored artifacts
> and both of which a pin check verifies byte for byte. Marking the digest as an excerpt in that
> table — or replacing it with the README sections in full — would make the difference visible to
> the next person who runs a pin check, which is what let this go unnoticed until something tried
> to compute a distance. That is F-19 and this finding meeting in one row.
>
> **A note offered rather than asked.** EXT-17 has independently chosen ECEF-on-WGS-84
> straight-line distance, `<=` at the threshold, and edge-inclusive polygons — the same answers.
> That was not inherited: it was arrived at from what *is* vendored (§15 forbids converting
> units, and `positionGeodetic` carries three components, so a metric discarding altitude
> discards data the format preserves deliberately), and then found to agree while this
> correspondence check was being run. The agreement is fortunate, and it is not a substitute for
> the sections being in `contract/` — the next consumer will not have run this check.

### Why the answer is worth having even though nothing is blocked

EXT-17 is not blocked: the method is decided, stated with its constants, and tested. What is
missing is the property that made the two-repository split workable in the first place — that
everything crossing the boundary does so as a **documented, versioned artifact** rather than as
knowledge somebody happened to acquire.

The practical test is the one `PROVENANCE.md` itself proposes: a third party should be able to
implement the consumer side from `contract/` alone. For the capture format that is demonstrably
true and was demonstrated twice. For the condition file it is currently false, in exactly one
place, and that place is the arithmetic every geometric verdict rests on.
