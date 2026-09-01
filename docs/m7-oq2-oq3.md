# OQ-2 and OQ-3 — decided by the DRI, on a reading of [B]

**Status: DECIDED at M7, 2026-09-01 — and then overtaken the same day, in different ways.**

> **UPDATE, later on 2026-09-01, and it supersedes the banner below.** Both recipients were
> reached after this document was written.
>
> - **OQ-3 is ANSWERED IN FULL and E-1 is CLOSED.** The mentor confirmed all six parts across
>   two relays — see §2.0 and §2.2. **Every answer matched the reading below**, so nothing this
>   project ships changed. [B]'s *"confirm the invocation with your mentor"* is **discharged**.
> - **OQ-2 is still NOT answered.** The mentor concurred with the content reading, independently
>   — see §1.0 — but the question is addressed to the owner of [B], criterion 2 is theirs to
>   discharge, and they have still never replied. A concurrence is not a ruling. **This one
>   stays open.**
>
> The banner below is kept exactly as written, because it is the state these decisions were
> taken in and because the distinction it draws is the reason both outcomes could be recorded
> honestly rather than rounded to "we asked and it was fine".

> **Read this line before any other in this document, as it stood when the decisions were made.** Neither question was *answered*. The
> owner of [B] never replied to OQ-2, and no mentor ever reviewed OQ-3. What happened is that the
> **DRI, on 2026-09-01, authorised the implementer to decide both from the brief's own words**,
> because schedule became the binding constraint and neither recipient was reachable in time.
>
> This project has kept `drafted` / `sent` / `answered` apart since M1 precisely so that a
> sentence like the one above can be written without ambiguity. **A fourth state is now in use:
> `decided`.** It means *"a person entitled to decide it did, on stated evidence, and the
> original recipient still has not spoken."* It is stronger than `open` and weaker than
> `answered`, and conflating it with either is the failure this file exists to prevent.

**Decision, in two parts**

> 1. **OQ-2 — the determinism gate is keyed on CONTENT.** [B]'s own words decide it, and the
>    deciding sentence is not the one everybody quotes. **No code changes**: `content` was
>    already the default. What changes is that the deviation stops being *unruled* and becomes
>    *ruled by the DRI rather than by [B]'s author*, and every report now says which.
> 2. **OQ-3 — the measured invocation is adopted as this project's stated production
>    invocation**, and its six sub-questions are answered from [B] where [B] speaks and from
>    measurement where it does not. **No code changes.** [B] asks its reader to *"confirm the
>    invocation with your mentor"*; that confirmation **did not happen**, and that is recorded as
>    a standing limit rather than closed over.

---

## 1. OQ-2 — content or bytes

### 1.0 UPDATE, 2026-09-01 — the mentor concurs, and the question stays open anyway

**The mentor was asked separately and said content.** They had not seen the reading below; they
reached the same answer independently. That is worth recording and it is worth being precise
about what it is.

**It is a concurrence, not a ruling.** OQ-2 does not ask "what is the better basis" — that was
decided below, and the mentor agreeing raises confidence in it. OQ-2 asks whether **[B]'s
acceptance criterion 2 is discharged**, and [B]'s acceptance criteria belong to [B]'s author, who
has still never replied. A second opinion from somebody entitled to hold one does not transfer
that authority.

So three words now attach to this question and none of them substitutes for another:

| | who | what it means |
|---|---|---|
| `decided` | the DRI, 2026-09-01 | a person entitled to decide it did, on the reading below |
| `concurred` | the mentor, 2026-09-01 | somebody else entitled to an opinion reached the same answer, independently |
| `answered` | **nobody** | [B]'s author has not replied, and criterion 2 is theirs to discharge |

**Nothing about the gate changed** — content was already the default and still is. What changed
is what the reports say: `self-test.json` gained `gate.oq2_concurred_by` as a **separate key**
beside `gate.oq2_decided_by` and `gate.oq2_answered_by_brief_author` (still `false`), so that a
consumer cannot compute the last from the others. Two paired checks in
`tests/determinism_test.cpp` assert that the concurrence may only ever appear in a report that
also says [B]'s author has not replied — the failure being guarded is not the wording, it is
somebody later reading "the mentor said content" and deleting the caveat.

**The section below is unchanged.** It is the reading that decided the question, and it decided
it before anyone concurred.

### 1.1 What [B] actually says, in full

Five places bear on it. Four support content; one is the sentence the byte reading rests on.

| # | [B]'s words | Reading |
|---|---|---|
| 1 | *"the same scenario, the same inputs and the same ordering produce **the same outputs**"* | **Outputs**, not bytes. A capture is a *recording of* the outputs, not the outputs |
| 2 | *"run the same configuration twice, capture both, and **show they match**"* | **Match**, not "are identical files" |
| 3 | acceptance criterion 2: *"two identical runs produce **identical captures**"* | **The one phrase that supports bytes**, and it is the strongest thing the byte reading has |
| 4 | *"Anything of yours that varies between runs — a timestamp in **the compared output**, an unordered container iterated, a value read from a clock"* | *"The compared output"* is a thing **we build**. A byte diff has no output of ours in it to contaminate; a content comparison does. This sentence only makes sense if we author a comparison |
| 5 | *"Determinism first. Nothing in **your comparison path** may vary between identical runs"* | Again: **our** comparison path. [B] expects one to exist |

**And the two words that are not there.** *"Byte"* and *"hash"* do not appear anywhere in [B] —
not in the prose, not in the acceptance criteria, not in the surface table. The byte reading is
an inference from the single word *"identical"* in criterion 2, against four passages that read
the other way.

### 1.2 The sentence that decides it

> *"**If it ever fails, you have found either a defect in your harness or something far more
> interesting, and you must be able to tell which.**"*

This is [B] stating the self-test's **purpose**, and it is the passage that settles the question,
because it is a functional requirement rather than a description.

**Under the byte reading the self-test cannot do the job [B] assigns it here.** Measured on this
platform: two runs of one configuration are byte-identical **0 times in 190**. The byte gate
therefore fails **always** — on a clean harness, on a broken harness, and on a genuinely
interesting platform finding, identically. A test with one outcome distinguishes nothing. It
cannot tell you which of [B]'s two cases you are in, because it says the same thing in both.

**Under the content reading the self-test does exactly what that sentence asks.** It passed 190
of 190 pairs on a clean harness, and it has **failed on a real pair for a real reason** — M6's
first attempt at the twenty-run campaign, where 23 samples at `sim_time_s` 0 carried different
`velocityNed` values because our own parameter update raced the roster burst. That is precisely
[B]'s *"a defect in your harness"* case, found by the gate, named by the gate, and told apart
from the platform's own behaviour. `campaigns/m6-gate-refused/` is that run, kept.

**So the two readings are not equally defensible.** One satisfies criterion 2's wording and
defeats the stated purpose of the thing criterion 2 is about. The other satisfies the purpose and
requires reading *"identical captures"* as *"captures that match"* — which is [B]'s own word for
it two paragraphs earlier.

### 1.3 What was considered and rejected

- **Wait for the ruling.** The correct choice until 2026-09-01, and it was made and held across
  four milestones. It stopped being available when schedule became binding. It is not reversed by
  this decision: the question stays recorded as never answered, and a reply would still be acted
  on.
- **Adopt the byte reading and declare the project blocked at M4.** Faithful to criterion 2's
  most literal wording, and it makes [B]'s step 4 — *"Do not build further until it passes"* —
  unsatisfiable on this hardware forever, for a reason that has nothing to do with the simulation
  being deterministic. [B] does not read like a document asking for that.
- **Weaken the byte comparison until it passes.** Never available. ADR-1 forbids it, and the
  report says so on every run: *"Nothing here is normalised, filtered or masked beyond the one
  field §14 names — a comparison made to pass by construction would measure nothing."*

### 1.4 What changes, and what does not

**No code changes.** `content` was already the default; `--gate-basis bytes` still exists and
still correctly fails; both comparisons still always run and are always reported.

**What changes is what the documents and the reports are allowed to say:**

| | before | after |
|---|---|---|
| OQ-2's status | `Needs Input — escalated`, unanswered | **`Decided` (DRI, 2026-09-01)**, still never answered by [B]'s author |
| The deviation | a **named and unruled** deviation from criterion 2 | a **named and ruled** deviation — ruled by the DRI, on the reading above |
| Acceptance criterion 2 | *"this project does not claim it"* | **met under the adopted reading**, which is stated wherever it is claimed |
| The report line | `OQ-2 UNANSWERED` | `OQ-2 DECIDED (DRI) — content. [B]'s author has still not ruled` |

**What is still not claimed, and must never be**: that [B]'s author agrees. They have not
replied. A ruling from them remains the only thing that would make this an *answer*, and it would
still change a default and no code.

---

## 2. OQ-3 / E-1 — the headless invocation

### 2.0 UPDATE, 2026-09-01 — the mentor replied, and four of the six are now ANSWERED

**This section was written before the mentor was reachable.** It stands as the reading that
decided the six parts; this table records which of them stopped being decisions.

| part | decided below as | the mentor's answer |
|---|---|---|
| **(b)** `N8RO_RELEASE` | required | **ANSWERED — yes, expected in production** |
| **(d)** degraded terrain | leave it | **ANSWERED — yes, leave it as it is** |
| **(e)** console control event; non-zero exit expected | both adopted | **ANSWERED — yes to both** |
| **(f)** `C:\N8RO\bin` on `PATH` | a second, separate precondition | **ANSWERED — yes** |
| **(a)** bus-publish route | yes, on [B]'s own ordering | **ANSWERED on the follow-up — yes** |
| **(c)** `SimEngineHost_SharedMemory` | yes, on *"closed configuration"* + *"determinism first"* | **ANSWERED on the follow-up — *"pick the one you prefer"***. No variant is prescribed |

**Every answer matched the decision, and nothing changed as a result** — which is the useful
thing about having asked: four readings of [B] were confirmed correct by the person [B] told us
to ask.

**(a) and (c) were not recorded as answered when the first relay did not cover them**, because
the rule this project has applied to [B]'s author for five milestones applies to a mentor too.
**A follow-up closed both later the same day, so E-1 is now closed in full** — six of six.

**(c)'s answer settles less than it looks and more than it looks.** *"Pick the one you prefer"*
does not say `SharedMemory` is right; it says **nothing is prescribed**. The decision below
therefore stands unchanged and stands as **ours**, explicitly delegated rather than inferred —
and what the answer removes is the possibility that we chose against an intent that existed and
had simply not been written down. That was the actual risk, and it is now gone.

**One answer had a consequence beyond confirming a reading.** (b) turned F-17 from *suspected*
into *demonstrated* and produced **E-6**
([EXT-08 issue #4](https://github.com/EgeCankaya/EXT-08/issues/4)): `PROVENANCE.md` finding 6
documents this invocation **without** `N8RO_RELEASE`, and following it exactly gives a host that
refuses every 42-entity load while sitting idle rather than failing.

### 2.1 The one thing that could not be decided from [B]

[B]'s surface table says, of `bin\n8ro-sim-app.exe`: *"the host binary that runs an engine with
no GUI. **Confirm the invocation with your mentor.**"*

That is an instruction to ask a person. **No reading of [B] can discharge it**, and this decision
does not pretend to. What it does is settle every *substantive* sub-question from [B] or from
measurement, so that what remains outstanding is one process step rather than six technical
unknowns.

### 2.2 The six parts, decided

**(a) Is the bus-publish route the intended control path?** → **Yes, on [B]'s own ordering.**
[B]'s surface table lists *"The client — control, subscribe, query"* **first**, as the primary
surface. The scripting namespaces appear last and are qualified: *"Run lifecycle from a script,
**if you drive it that way**."* That conditional is permissive, not prescriptive. EXT-17 drives
it the other way, which is the way [B] lists first.

**(b) Is `N8RO_RELEASE` expected to be set?** → **It is required, and not by preference.** [B]
describes this binary as *"the host binary that runs an engine with no GUI"*. Measured at M1:
without `N8RO_RELEASE` it does not run an engine — it skips its plugin scan, never registers
`componentPhysics`, and refuses every 42-entity scenario load while **sitting idle rather than
failing**. Setting it is therefore a precondition of [B]'s own description being true.
`PROVENANCE.md` finding 6 omits it; that stays recorded as F-17, upstream's to correct.

**(c) `SimEngineHost_SharedMemory` or a best-effort variant?** → **SharedMemory.** [B] settles
this in one sentence: *"**Campaign runs are for the closed configuration.**"* A best-effort
transport is one permitted to drop, and [B]'s rule 4 is *"Determinism first."* Choosing a variant
that may drop, for a campaign whose entire purpose rests on the published stream being as
complete as the platform can make it, would contradict both.

**(d) Is the degraded terrain configuration expected?** → **Left exactly as it is, and that is
now a decision rather than a judgement call.** [B] says nothing about terrain. It says a great
deal about determinism, and about the self-test needing to distinguish a harness defect from
something interesting. **Provisioning terrain now would invalidate every measurement this project
has inherited and taken**, all of which were made in this configuration — turning a comparable
body of evidence into two incomparable halves at the last milestone. The errors are noise in the
host log, they are recorded in the README's limits, and nothing computed here depends on
elevation.

**(e) Console control event, and a non-zero host exit?** → **Both adopted, and [B] requires the
first in substance.** [B]'s rule 1: *"Every run is isolated. No state carried from one run into
the next — not a file, not a cached handle, **not a still-running host**."* [B] does not say how
to end the host, and `sim/engine/command`'s vocabulary is closed at `start`/`stop`/`pause`/`step`
with no shutdown, so *something* outside that vocabulary must end it. A `CTRL_BREAK_EVENT` to the
process group this campaign created ends it in an orderly way — the `.running` marker is removed
and no crash rename follows — where `TerminateProcess` does not. **The non-zero exit
(`0xC000013A`) is normal here and is not evidence of a crash**, and CR-EX-5 already tells "we
stopped it" from "it died" by asking the process itself rather than by reading its exit code.

**(f) `C:\N8RO\bin` on `PATH`.** → **A second, separate precondition, and required.** Same
reasoning as (b). Verified again at M7 and tabled in `docs/recording-script.md` §0.4: without it
`n8ro-campaign` exits `-1073741515` (`0xC0000135`, DLL not found) having printed nothing —
`n8ro-judge`, `n8ro-compare` and `n8ro-capture` are unaffected, because they link nothing.

### 2.3 What changes, and what does not

**No code changes.** The invocation is unchanged; it is the one measured across roughly a hundred
runs since M1.

What changes is that the six technical questions stop being open, the invocation is stated as
**this project's production invocation** rather than as a candidate, and **one thing stays open
and is named**: [B] asked for a mentor's confirmation and it did not happen. That belongs in the
README's limits, not in a queue.

---

## 3. The sweep-legibility success metric

**Decided: [B]'s acceptance criterion 3 is met on measured evidence. The PRD's own
*mentor-review* verification method was not executed, and that is recorded rather than rewritten.**

The distinction matters and it is the whole of this section.

- **[B]'s acceptance criterion 3** is *"A parameter sweep shows a result that varies with the
  parameter, presented so the trend is visible."* **It names no reviewer and no method.** It is
  met, demonstrably: across the committed twenty-run sweep, engagement rises 47 → 65 to a peak at
  170–190 m/s and falls to 54 at 380; three conditions flip at three different thresholds; the
  run outcome flips with them. `campaigns/m6-campaign/report.txt`.
- **The PRD's success-metric row** attached a verification method — *mentor review of the sweep
  report* — and **that method is this project's own invention**, not the client's. It was not
  executed.

**The row is therefore not rewritten to a method it can pass.** Doing that — quietly swapping a
verification method for one you already satisfy — is the move that makes a metrics table
worthless, and this project has said so in print. What the row now records is both facts: the
client criterion is **met on evidence**, and the internal verification method was **not
executed**, by DRI decision, with the reason.

**A mentor review remains available and would still add something.** It is no longer *required*
by anything [B] says.

---

## 4. What this decision does not touch

- **E-3, E-4 and E-5** are with EXT-08 as issues #1, #2 and #3. They are `raised`, none blocks,
  and none of them is the DRI's to decide — they are defects in another project's artifact.
- **ADR-1's one-machine scope.** Still one machine. [B] claims the platform's guarantee holds
  *"on every run and every machine"*; nothing here tests the second half and no document claims
  it.
- **R13's fidelity ceiling**, still not enforced in code, still the campaign author's job.
- **R12, R14 and R15.** The gate is not weakened and no retry is added. This decision changes
  which comparison *decides*; it changes nothing about what either comparison *does*.
- **R16.** The continuity bound still rests on a clamp measured on one entity profile. Unchanged,
  and still the assumption to attack first.

## 5. How to reverse this

Cheaply, and that is by design.

| If | Then |
|---|---|
| [B]'s author rules **content** | Nothing to do but update a status line from `decided` to `answered`. The evidence in §1 becomes redundant, which is the best outcome |
| [B]'s author rules **bytes** | `--gate-basis bytes` already exists and already works. The campaign then correctly stops at exit 3 on this hardware, which is `campaigns/m4-bytes/`, already demonstrated. **A default changes; no code does.** The project would be blocked at [B]'s step 4 and that would be the ruling's consequence rather than a defect |
| A mentor corrects the invocation | `src/control/EngineControl.cpp` is one file. Every campaign would need re-running — a few hours of machine time — because every number this project holds was measured through this invocation. That is re-measurement, not rework, and the exposure has been stated since M1 |
| A mentor reviews the sweep | The success-metric row's method becomes executed, and the row moves from "not executed" to "met" |
