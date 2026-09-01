# M7 — Evidence and documentation

**Date:** 2026-09-01
**Milestone:** M7 (the last). [B]'s deliverables 2–5: the README, the committed campaign, the
recording, and the notes on determinism
**Validation line, as the PRD writes it:** *"CR-DOC-1, CR-DOC-2, and every success metric."*

> **This milestone does not fully close, and that was decided in advance rather than discovered
> here.** Two of the things its validation line asks for cannot be produced by this project
> alone. They are reported as **unmet**, with what is missing and who can supply it, because a
> metric whose named method did not happen is not a metric that passed. `docs/findings.md` §E
> recorded this cost when E-1 was deferred; it is carried forward unchanged.

---

## 1. What M7 delivered

| Deliverable | [B]'s words | Status |
|---|---|---|
| README | *"how to configure a campaign, how to write an assertion, the output format, and the limits"* | **Done.** Four sections, each named for the topic, indexed from the top |
| A real campaign | *"its configuration, its captured runs and its report, committed as an example"* | **Done, with one named deviation** — the captures are not committed (F-34); a manifest of each one's size, SHA-256 and read-back counts is |
| Determinism notes | *"what you had to do to make comparison meaningful, and anything you saw that you could not explain"* | **Done.** `docs/determinism-notes.md`, five unexplained observations |
| The 5-minute recording | *"launch a campaign, watch it run, read the report"* | **DELIVERED 2026-09-01** — [one take, 4:10, published](https://drive.google.com/drive/folders/16cR82ynxrcmrzJofwHKdpReNlPj1C--M?usp=sharing). All three clauses in order, no narration. **4:10 against "5-minute", stated rather than rounded** |

---

## 2. CR-DOC-1, against its own acceptance criteria

- **All four topics present, each as its own section.** Yes, and each is titled in [B]'s own
  words so a reader matching the deliverable against the file does not have to interpret. The
  index at the top of the README maps [B]'s four phrases to the four sections.
- **The limits section states what the PRD requires it to.** What a pass proves and does not
  prove given a sampled stream; the determinism gate's content basis and why (ADR-1); the stop
  predicate; the disk ceiling. It also carries what M5 and M6 measured afterwards — the axis's
  fidelity ceiling, the gate's refusal rate, what a not-met verdict is entitled to claim, and the
  one condition [B]'s own example implies that cannot be expressed here.
- **The CLI is documented by the golden `--help` and the build-time comparison**, not by prose.
  Four tools, four golden files, four builds that fail on a drift.

**The limits section is the longest in the README, deliberately.** [B] names it last of the four
and it is the one a reviewer should read first, so it is linked from the top with that said.

---

## 3. CR-DOC-2, and the two halves that are not the same

### The campaign — done, with a deviation that is named rather than quiet

`campaigns/m6-campaign/` holds the configuration, the report, and a manifest. **What it does not
hold is 527 MB of captures**, and `.gitignore` has excluded captures since this repository's
first commit for that reason. `MANIFEST.md` carries each capture's byte size, SHA-256 and
read-back counts, so a number in `docs/m6-assertions.md` traces to a file whose contents are
pinned even though the file is not in the tree.

**This is F-34 and `docs/decisions-m6-m7.md` B6.** It states what the manifest buys and what it
does not: it does **not** make a re-run reproducible byte for byte — nothing does, and that is
this project's central measurement rather than a limitation of the decision.

**One thing M7 improved beyond M6's scope.** The `.gitignore` change admits *any* campaign's
report, so every prior milestone's evidence is now committed too — M2's twenty runs, M3's
rotation probes, M4's gate and byte-basis runs, M5's two sweep executions. 1.2 MB in total. Every
number in every milestone document now traces to a committed record rather than to a directory on
one machine.

### The recording — delivered, and not substituted for

`docs/recording-script.md` is the runbook: every command in order, what each one puts on screen,
and how long it takes. **There is no narration and nothing is opened in an editor** — the
recording is the terminal, which is possible only because the tools caption themselves: the gate
prints its own basis and the OQ-2 wording, the report prints why its bar is not scaled from zero,
and every verdict that is not satisfied prints its reason. A claim that would have to be spoken to
be present is one the recording cannot evidence. What survives from the narrated version is the
last section — **what not to claim in a caption or a title**, because three claims are easy to
overstate and each has a named limit.

**It was shot on 2026-09-01 and is published**:
[one take, 4 min 10 s](https://drive.google.com/drive/folders/16cR82ynxrcmrzJofwHKdpReNlPj1C--M?usp=sharing),
all eight steps in order, beside a command-by-command companion for a viewer. The campaign it
launches on camera is committed at `campaigns/demo/` — three runs at 200 frames with a host-start
failure injected into run 1, gate passed on content at 8503 of 8503 comparable samples, outcomes
2 fail / 1 infrastructure error / 0 pass — and the report read at step 5 is the committed
twenty-run campaign. **It runs 4:10 against [B]'s "5-minute", which is stated rather than rounded
up**: with no narration the commands take less time than they did when they were narrated, and
every clause of the deliverable — *launch a campaign, watch it run, read the report* — is in it.

**What the delay bought, and it is worth one sentence.** R10 exists because the equivalent
deliverable was not delivered upstream, and the failure it names is not lateness — it is
*substituting a written walkthrough and calling the requirement met*. From M7 until the take
existed, the runbook's first line read `NOT RECORDED` and every document here said **outstanding**
rather than claiming the requirement met on the strength of the script. That is the only reason
this section can now say *delivered* and mean it.

---

## 4. The success metrics, reported honestly

| Metric | Target | Result |
|---|---|---|
| Unattended campaign length | ≥ 20 runs, zero manual steps | **Met.** 20 runs, one command |
| Runs surviving an injected infrastructure failure | campaign continues; the run is `infrastructure_error`, not `fail` | **Met**, all four faults, each continuing to the next run |
| Determinism self-test, content | 100% of compared samples agree | **Met.** 50 361 of 50 361 on the committed campaign's gate; 9 573 667 of 9 573 667 over M2's 190 pairs |
| Determinism self-test, bytes | expected to FAIL, recorded and explained | **Met as specified** — it fails, and is reported rather than used as the gate |
| Comparison-path variability introduced by us | zero | **Met**, by a build search and a behavioural test for each hazard, each verified to reject |
| Gate refusals that are not determinism failures | not zero, and measured | **Met.** ~1 pair in 14 ordinary; 11.4% parameterised; 2 of 20 on the committed campaign |
| **Sweep legibility** | a result that varies with the parameter, presented so the trend is visible — **measured by mentor review of the sweep report** | **MET at 2026-09-01, by its own named method.** Reported UNMET at first; see below, which is kept |
| Re-judgement without re-running | a stored campaign re-judged, no host started | **Met.** 20 of 20 byte-identical, 0 verify failures |
| Diff precision | names the **first** point of divergence | **Met**, on two real runs: segment, `(entity, occupancy)`, `sim_time_s` and field |
| Peak campaign disk footprint | bounded and stated | **Met.** 8 GiB ceiling, checked before run 1 and after every run |

### Sweep legibility — reported UNMET, then met by its own named method

**Resolved 2026-09-01: the mentor reviewed the sweep report and confirmed it reads.** The metric
is met, and it is met *on the terms it was written on*.

**The section below is kept exactly as written, because it is the part that makes the pass mean
something.** It was the state of this metric for as long as the method had not been executed, and
it records that the artifact exceeding its target was explicitly **not** accepted as a
substitute. A metrics table whose rows can be satisfied by re-describing them is worth nothing;
this row was held open until the thing it named actually happened.

---

#### As reported before the review — kept verbatim

**The measurement method this metric names is *mentor review of the sweep report*. No mentor has
reviewed it.** That is the whole of the failure.

The *artifact* the metric is about exists and is better than the target asks: the committed sweep
shows a count varying with the parameter, rising 47 → 65 to a peak at 170–190 m/s and falling
back to 54 at 380 — non-monotone, and drawn so that is visible — plus three conditions whose
**verdicts** flip at three different thresholds, and a run outcome that flips with them.

**It would be easy and wrong to mark this met on that basis.** The metric does not say "a trend
is visible"; it says a mentor confirms one is. Substituting the author's own judgement for the
named method is exactly the move that makes a metrics table worthless, and this table is one of
the things a reviewer is meant to be able to trust.

**It traces to E-1**, deferred by DRI decision on 2026-09-01, and `findings.md` §E predicted this
outcome in those words: *"M7 must state that metric as unmet rather than claim it."*

---

## 5. What is still open at the end of the project

Stated in one place, because the last milestone is where an open item is most likely to quietly
become a closed one. **Updated four times on 2026-09-01: after the mentor's first reply, after
all four upstream issues came back fixed, after the mentor's follow-up closed E-1, and after the
recording was shot.** Two rows survived the first three; **one row survives all four**, and it
needs somebody outside this project.

| # | What | Whose | Status | Blocks anything? |
|---|---|---|---|---|
| ~~**The recording**~~ | [B]'s deliverable 4 | Needed a person | **DELIVERED 2026-09-01.** [One take, 4:10, published](https://drive.google.com/drive/folders/16cR82ynxrcmrzJofwHKdpReNlPj1C--M?usp=sharing), shot to `docs/recording-script.md`. The runbook is kept as the record of what was filmed, including what *not* to claim in a caption | No. It was the last one |
| **OQ-2** | Is the gate keyed on content or on bytes? | The owner of [B] | **`decided` (DRI, 2026-09-01) and `concurred` (mentor, same day, independently) — content. Still never answered**, across five milestones. **The concurrence is a second opinion, not a ruling**: criterion 2 is [B]'s author's to discharge | No. Both readings ship as selectable gates; a ruling changes a default and no code |
| ~~**OQ-3 (a) and (c)**~~ | Is the bus route the intended control path? Is `SimEngineHost_SharedMemory` right versus the seven other variants? | Mentor | **CLOSED, 2026-09-01.** The follow-up ran: **(a) yes**; **(c) *"pick the one you prefer"***, so no variant is prescribed. **E-1 is answered in full** | No, and it never did. Nothing changed |
| **E-3** | §6.7's summing rule is wrong for `segments` | EXT-08 ([#1](https://github.com/EgeCankaya/EXT-08/issues/1)) | **CLOSED — FIXED 2026-09-01**, option (a) taken, vendored back at the fourth pin | No, and it never did |
| **E-4** | §5.1's frozen-clock test detects three phenomena, not one | EXT-08 ([#2](https://github.com/EgeCankaya/EXT-08/issues/2)) | **CLOSED — FIXED 2026-09-01, wider than raised.** EXT-08 folded in the third shape, which the issue never carried, and §14 now says an emptied self-test is a refusal rather than a pass | No — and it is the one with a measured operational cost, now including a campaign-level one (R15). **That cost is unchanged and was not traded away for the fix** |
| **E-5** | The vendored condition digest stops one heading early | EXT-08 ([#3](https://github.com/EgeCankaya/EXT-08/issues/3)) | **CLOSED — FIXED 2026-09-01, and not by the fix that was asked for.** EXT-08 created a real upstream file rather than replying "take those two sections too"; the digest is now vendored by identity, closing **F-19** | No. The arithmetic was decided here and stated with its constants, and is unchanged |
| **E-6** | EXT-08's README documents the R8 invocation without `N8RO_RELEASE` | EXT-08 ([#4](https://github.com/EgeCankaya/EXT-08/issues/4)) | **CLOSED — FIXED 2026-09-01**, the same day it was raised, against the corrected citation (F-37) | No. **New at M7** — the mentor's answer turned F-17 from suspected into demonstrated |

**What closed on 2026-09-01, and it is worth naming separately:** four of OQ-3's six parts moved
from `decided` to **`answered`**; the **sweep-legibility metric moved from UNMET to met by its
own named method**; and **all four EXT-08 issues moved from `sent` to `fixed`** — a fifth state,
stronger than `answered`, because a reply is a sentence and a fix is a diff. **None of it changed
a line of code** — every confirmed answer and every correction matched what was already built and
measured, which is what raising them rather than working around them was for.

**Two rows shrank as a side effect and one grew.** F-19 and F-18 both close at the fourth pin.
The new one is **F-38**: a fixed escalation makes `contract/` stale, and nothing notices — the
first drift this project caused itself.

**Nothing in that table was discovered at M7.** Every row was open before this milestone began,
except E-6, which is a five-milestone-old finding that only became reportable when the mentor
confirmed the thing it depends on.

**And after the fixes and the second relay, two rows remain: the recording, and OQ-2.** Both need
a person who is not on this project — a screen recorder and a human for the first, and the author
of [B] for the second. **Nothing that is this project's to close is open.**

**One row closed in a way worth naming, because it is the one that could most easily have been
faked.** OQ-3 (a) and (c) were reported here as *not answered* while four of six were, rather
than being rounded to "the mentor confirmed the invocation". Holding them cost one follow-up
question, and the follow-up closed both. **The same rigour left OQ-2 open on the same day**: the
mentor concurred with the content basis, and a concurrence from somebody who does not own the
acceptance criterion is not a ruling. The rule cut both ways within a few hours, which is the
only real evidence that it is a rule and not a posture.

## 6. What M7 did *not* do

- **It did not close OQ-2**, and the implementer must not. Re-checked at M7: still unanswered,
  and still unanswered after the mentor concurred with the content reading — **a concurrence
  from somebody who does not own the acceptance criterion is not a ruling.**
- **It did not re-raise E-1 while it was deferred.** M7's job was to carry its cost correctly,
  which meant holding §4's metric at UNMET until its named method actually happened. When the
  mentor did reply, four of six parts became answered and two did **not** — and the two that did
  not were left `decided` rather than rounded up, until a follow-up answered them properly. **E-1
  is now closed on all six.**
- **It did not record the video**, and did not substitute anything for it.
- **It did not re-run any campaign to improve an artifact.** `campaigns/m6-gate-refused/` and
  `campaigns/m5-sweep-first/` are both still there, both still counted.
- **It did not widen ADR-1's one-machine scope**, and no document claims [B]'s *"every machine"*.
- **It did not add a requirement, weaken one, or relax a provenance marker.**
