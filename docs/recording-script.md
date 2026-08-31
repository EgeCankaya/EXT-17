# The 5-minute recording — script, and its status

**Status: NOT RECORDED.** The recording is a deliverable [B] names explicitly — *"a 5-minute
recording: launch a campaign, watch it run, read the report"* — and it needs a person. This
document is everything except the person: the commands in order, what to say over each, and what
the screen will show.

**Why it is scripted rather than improvised, and why the status is stated rather than implied.**
The PRD scheduled this at M7 *explicitly* because the equivalent deliverable was not delivered by
the upstream project (R10). The failure mode R10 names is not "the recording is late"; it is
**substituting a written walkthrough and calling the requirement met**. So this file does not
claim to be the recording. It is the script the recording follows, which means the remaining work
is the recording and not the preparation.

**Total: 5:00.** Timings are the measured durations of the commands, not estimates — a
1200-frame run takes about 70 s here, so the campaign is started early and returned to.

---

## Before recording

```cmd
set N8RO_RELEASE=C:\N8RO
set PATH=C:\N8RO\bin;%PATH%
```

Both are **measured preconditions, not preferences** — without the first the host refuses every
42-entity scenario load while sitting idle rather than failing; without the second an SDK-linked
binary exits 53 having produced no output at all. Worth one sentence on camera, because both
failures look like something else.

Have two shells open. Run from a scratch directory: N8RO binaries write into their working
directory.

---

## 0:00 – 0:30 — What this is

> "EXT-17 runs many simulation runs without a human, varies one input across them, decides
> whether each one passed, and reports across the campaign. It is the downstream half of a pair —
> EXT-08 records a run into a durable capture; this runs the campaign."

Show `README.md`'s first screen. Point at one line and move on:

> "The determinism gate passes on **content**, and that is this project's decision rather than
> the client's. I'll come back to why."

## 0:30 – 1:00 — The conditions, and the check that costs ten seconds instead of twenty runs

```cmd
build\n8ro-judge\n8ro-judge.exe check --conditions examples\atacama-raid.conditions.json
```

```
examples\atacama-raid.conditions.json: 7 condition(s), all valid
  raid-leader-reaches-airfield     proximity       expect met
  ...
  raid-leader-degraded             terminal_state  expect not_met
```

> "Conditions live in their own file, never in the source. This validates it **before any host
> starts** — a duplicate id, an unrecognised kind, an unknown key are each a distinct named
> error. A typo costs ten seconds, not twenty runs."

Then break it on camera, because a refusal is more convincing than a pass:

```cmd
build\n8ro-judge\n8ro-judge.exe check --conditions docs\demo\typo.conditions.json
```

```
n8ro-judge: condition file refused - unknown_key [raid-leader-reaches-airfield]:
  "within_meters" is not a key of a proximity condition. A condition file is ours and a person
  wrote it, so an unrecognised key is refused rather than ignored ...
```

> "One character. It would have been a threshold that silently did not apply, and twenty
> confident passes."

## 1:00 – 1:30 — Launch it, one command

```cmd
build\n8ro-campaign\n8ro-campaign.exe repeat ^
  --out-dir campaigns\demo ^
  --campaign examples\atacama-raid-speed-20.json ^
  --conditions examples\atacama-raid.conditions.json ^
  --recorder <path-to-n8ro-bridge.exe>
```

> "One command, twenty runs, no keystroke per run. That's the brief's first acceptance
> criterion."

**Leave it running.** Say what it is doing while the first lines scroll:

> "Before any campaign run, it proves determinism: the same configuration twice, captured both
> times, compared. The brief makes that a hard stop — *do not build further until it passes* — so
> the campaign runs it itself rather than trusting anyone to remember."

## 1:30 – 2:30 — The gate, while it runs

Point at the self-test output as it appears:

```
  gate basis            content   (ADR-1: the content basis is THIS PROJECT'S decision, not the client's)
  OQ-2                  UNANSWERED. ...
  content comparison    PASS
  byte comparison       DIFFER
  GATE                  PASS   on the content basis
                        This does NOT discharge [B]'s acceptance criterion 2 as written.
```

> "Both comparisons always run and both are always reported. Two identical runs here agree on
> every sample present in both — 9.5 million samples over 190 pairs, zero differing — and are
> **never** byte-identical, because about 0.2% of frames go unpublished, differently every run,
> with every platform counter reading zero.
>
> The brief asks for 'identical captures'. Whether that means content or bytes is an open
> question with its author, and it is **unanswered**. So both readings are built as selectable
> gates: under `--gate-basis bytes` the gate correctly fails and the campaign correctly stops. A
> ruling changes a default and no code. Every report says so — I am not going to tell you this
> project met acceptance criterion 2, because it hasn't."

## 2:30 – 3:30 — What a run produces, on a run that has finished

```cmd
type campaigns\demo\runs\000\run.json
```

Scroll to `judgement`, then:

```cmd
type campaigns\demo\runs\000\verdicts.jsonl
```

> "One verdict per declared condition. Each names the entities **with their occupancies** —
> identity here is `(name, occupancy)`, never the name, because the engine re-creates entities
> under names it has used — the segment, the deciding `sim_time_s`, and the values that decided
> it. That's enough to open the capture and find the two records by hand."

Then the one that matters most:

```
raid-leader-degraded   INDETERMINATE   health=nominal equals=degraded
  no sample of RedUAV_N_01 carried health = "degraded" ... A field's rate of change is not
  bounded by anything in the format, so the value could have been taken and left between two
  samples. This form is never decidable in the negative.
```

> "That one is the point of the whole project. A capture is a very high-fidelity **sample** of
> what the run published, not a transcript — and loss has been measured with every counter at
> zero. So 'no record says it happened' is not the same claim as 'it did not happen'.
>
> A naive referee reports that condition NOT MET, confidently, from a file that may be missing
> the frame. This one says it cannot decide, and why. `indeterminate` is a **verdict** state and
> never a fifth run outcome — the brief fixes those at four."

## 3:30 – 4:15 — The campaign report

```cmd
type campaigns\demo\campaign.log
```

Scroll to the two tables.

> "The sweep, ordered by parameter value — not by spelling, which would put 110 before 27.5. The
> bar is scaled between the minimum and maximum, not from zero, and the header says so: a column
> running 47 to 65 drawn from zero is twenty identical bars."

Then the verdict table:

> "And this is what the sweep is for. One column per condition **whose outcome changes** — the
> ones that don't are listed below as constant, because a column that never changes is not a
> trend. You can see where the raid first reaches the corridor, and where it first reaches the
> airfield. Two different thresholds, two different conditions."

Then the four counts:

> "Pass, fail, timeout, infrastructure error — four distinct outcomes, summing to the runs
> attempted, and no aggregate anywhere collapses two of them. Indeterminate verdicts are counted
> **beside** them and never folded in."

## 4:15 – 4:45 — Re-judging, without re-running

> "A week later you think of a new question."

```cmd
build\n8ro-judge\n8ro-judge.exe campaign campaigns\demo ^
  --conditions examples\atacama-raid.conditions.json --verify verdicts.jsonl --quiet
```

```
  no host is started and no bus subscription is made - this binary links nothing that could
  000  fail   satisfied 3  violated 3  indeterminate 1
    verify: 7 verdict(s) byte-identical to the live run's verdicts.jsonl
```

> "Seconds, not runs. And `--verify` is checking something specific: that re-judging produces
> **byte-identical** verdicts to the live run. That holds by construction — there is one
> evaluator and one input, a stored capture, and the live campaign judges through the same code
> over the same file. But a structural argument nobody checked is how a structural argument stops
> being true, so it is checked too."

## 4:45 – 5:00 — The limits, which is where to stop

Open `README.md` at *Limits*.

> "The last thing, and it is the part I'd want a reviewer to read first. A pass here means every
> declared condition was satisfied — not that the run was correct. It is a statement about the
> questions somebody thought to ask, over the data that reached the file.
>
> The gate refuses about one pair in fourteen for a reason that is **not** a determinism failure,
> and there is deliberately no retry. There is one condition the brief's own example implies that
> cannot be expressed here, and it is named. And the arithmetic every geometric verdict rests on
> is this project's decision, because the vendored contract stopped one heading short of stating
> it — that's raised upstream, and the decision doesn't wait on the answer.
>
> All of that is in the README, not in a footnote."

---

## What to have ready but not show

- `campaigns/m6-gate-refused/` — the execution where the gate correctly refused and **zero runs
  were attempted**. Worth having to hand if anyone asks *"what happens when it fails?"*, because
  the answer is not hypothetical.
- `docs/determinism-notes.md` §5 — the things that could not be explained.
- The four fault injections, if anyone asks how a host crash is survived.

## What not to say

- Do not say the determinism self-test discharges the brief's acceptance criterion 2. It does not,
  under the content reading, and OQ-2 is unanswered.
- Do not describe the twenty-run campaign as always succeeding. It met R15 on its first attempt.
- Do not call an `indeterminate` verdict a failure of the tool. It is the correct answer.
