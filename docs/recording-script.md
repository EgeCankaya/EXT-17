# The 5-minute recording — the script, and exactly which shell to run each command in

**Status: NOT RECORDED.** The recording is a deliverable [B] names — *"a 5-minute recording:
launch a campaign, watch it run, read the report"* — and it needs a person. This document is
everything except the person: which terminal, which command, in what order, what the screen will
show, and what to say over it.

**Why the status is stated rather than implied.** The PRD scheduled this at M7 *explicitly*
because the equivalent deliverable was not delivered by the upstream project (R10). The failure
R10 names is not lateness; it is **substituting a written walkthrough and calling the requirement
met**. So this file does not claim to be the recording. It is the script the recording follows,
which means the remaining work is the recording alone.

---

## 0. Before you press record

### 0.1 Use `cmd`, not PowerShell — both windows

**Every command in this script is written for `cmd.exe`.** That is a deliberate choice and it is
worth one sentence on camera if anyone asks:

| | |
|---|---|
| The build scripts are `.cmd` | `tools\*\build.cmd` are batch files. In `cmd` you run them by name; in PowerShell you must wrap every one as `cmd /c "tools\n8ro-judge\build.cmd"` |
| The line-continuation character differs | This project's README and help text use `^`, which is **cmd**. PowerShell uses a backtick `` ` ``. A command copied from the README into PowerShell breaks at the first line break |
| Environment variables differ | `set N8RO_RELEASE=C:\N8RO` in cmd; `$env:N8RO_RELEASE="C:\N8RO"` in PowerShell |
| Running an exe with a quoted path differs | cmd: `"build\n8ro-judge\n8ro-judge.exe" --help`. PowerShell needs the call operator: `& "build\n8ro-judge\n8ro-judge.exe" --help` |

**One `cmd` gotcha to know about even though this script avoids it:** `%errorlevel%` inside a
single-line `&`-chain is expanded when the line is *parsed*, not when it runs, so it reports the
**previous** command's code. If you want to show an exit code on camera, put it on its own line —
every command below does.

> If you would rather record in PowerShell, every command still works; wrap each `.cmd` in
> `cmd /c "..."`, swap `^` for `` ` ``, prefix each `.exe` with `&`, and use `$env:` for the two
> variables. The script does not do this because one shell on screen is one fewer thing to
> explain.

### 0.2 Open two `cmd` windows, both at the repo root

```cmd
cd /d C:\Projects\EXT-17
```

- **Terminal A — "the campaign."** Runs the long thing. It stays busy for about 90 seconds while
  you talk over it.
- **Terminal B — "the reading."** Everything else. Fast commands, all of them under a second.

### 0.3 Set the two environment variables in Terminal A only

```cmd
set N8RO_RELEASE=C:\N8RO
set PATH=C:\N8RO\bin;%PATH%
```

**Both are measured preconditions, not preferences**, and both are worth naming on camera because
each fails in a way that looks like something else:

- Without `N8RO_RELEASE`, the host resolves its plugin directory from the working directory,
  skips its plugin scan, never registers `componentPhysics`, and **refuses every 42-entity
  scenario load while sitting idle rather than failing** (F-5). It looks like a hang.
- Without `C:\N8RO\bin` on `PATH`, an SDK-linked binary exits `-1073741515` (`0xC0000135`,
  STATUS_DLL_NOT_FOUND) having printed nothing at all (F-8). It looks like a crash.

**Terminal B deliberately does NOT get them**, and that is a demonstration rather than an
oversight — see beat 5.

### 0.4 Which binary needs what — verified, not assumed

| binary | needs `C:\N8RO\bin` on `PATH`? | needs `N8RO_RELEASE`? |
|---|---|---|
| `n8ro-campaign` | **Yes** — even `--help` exits `-1073741515` without it | Yes, to hand to the host it starts |
| `n8ro-judge` | **No** | No |
| `n8ro-compare` | **No** | No |
| `n8ro-capture` | **No** | No |

The bottom three link nothing — not EXT-08, not the N8RO SDK — and that is checked on every
build. Beat 5 shows it rather than says it.

### 0.5 A scratch directory, because N8RO binaries write into their working directory

The campaign handles this itself (every child gets its own directory), so running from the repo
root is safe **for these commands**. It is not safe in general: `n8ro-sim-local.exe` writes a
per-entity JSONL dump into its working directory and `n8ro-sim-app.exe` creates `data\db\` and
`logs\` there. It did that in this repo's root once, before the rule was known (F-6).

### 0.6 Clear the demo directory so the launch is from nothing

In **Terminal A**:

```cmd
rmdir /s /q campaigns\demo 2>nul
```

### 0.7 Have these open in an editor, not on screen yet

`README.md` at the top, and at *Limits*. `docs/determinism-notes.md` at §5.

---

## 1. `0:00 – 0:30` — What this is

**Nothing typed. Show the top of `README.md`.**

> "EXT-17 runs many simulation runs without a human, varies one input across them, decides
> whether each one passed, and reports across the campaign. It is the downstream half of a pair —
> EXT-08 records a run into a durable capture; this one runs the campaign.
>
> One line before anything else, because it is the honest headline: **the determinism gate passes
> on content, and that is this project's decision rather than the client's.** I'll come back to
> what that costs."

---

## 2. `0:30 – 1:10` — The condition file, and the check that costs ten seconds instead of twenty runs

**Terminal B.**

```cmd
build\n8ro-judge\n8ro-judge.exe check --conditions examples\atacama-raid.conditions.json
```

```
examples\atacama-raid.conditions.json: 7 condition(s), all valid
  raid-leader-reaches-airfield     proximity       expect met
  raid-leader-crosses-corridor     area            expect met
  raid-leader-enters-depot-ring    area            expect met
  raid-leader-destroyed            terminal_state  expect not_met
  command-centre-destroyed         terminal_state  expect not_met
  airfield-operational             terminal_state  expect met
  raid-leader-degraded             terminal_state  expect not_met
```

> "Conditions live in their own file, never in the source. This validates it **before any host
> starts** — a duplicate id, an unrecognised kind, an unknown key, a key written twice are each a
> distinct named error. A typo costs ten seconds, not twenty runs."

**Then break it on camera**, because a refusal is more convincing than a pass:

```cmd
build\n8ro-judge\n8ro-judge.exe check --conditions docs\demo\typo.conditions.json
```

```
n8ro-judge: condition file refused - unknown_key [raid-leader-reaches-airfield]:
  "within_meters" is not a key of a proximity condition. A condition file is ours and a person
  wrote it, so an unrecognised key is refused rather than ignored - it is how "within_meters"
  for "within_m" would otherwise become a threshold that silently did not apply. A key
  beginning with '_' is a comment
```

> "One character. The capture format's own rule says ignore an unrecognised key, and that rule is
> right — for a capture, where a producer adds keys and an old reader has to survive them. A
> person writes *this* file, so it gets the opposite rule. Without it that's a threshold which
> silently didn't apply, and twenty confident passes."

---

## 3. `1:10 – 1:40` — Launch it. One command.

**Terminal A.** This is the *"launch a campaign"* beat.

```cmd
build\n8ro-campaign\n8ro-campaign.exe repeat ^
  --out-dir campaigns\demo ^
  --count 3 ^
  --frames 200 ^
  --recorder C:\Projects\EXT-08\build\x64\Release\n8ro-bridge.exe ^
  --conditions examples\atacama-raid.conditions.json ^
  --inject-fault host_start_failure ^
  --inject-at-run 1
```

> "One command. No keystroke per run, no prompt, no manual host start or teardown — that's the
> brief's first acceptance criterion.
>
> Two things I've done to make this fit in five minutes, and I want to be straight about both.
> **The runs are 200 frames instead of 1200**, so a whole campaign finishes while we talk — about
> ninety seconds. And I've asked it to **deliberately break the host on run 1**, so you can see
> what an unattended campaign does at three in the morning when something goes wrong.
>
> The real committed campaign is twenty runs at 1200 frames and takes twenty-five minutes. We'll
> read *its* report, not this one's."

**Leave it running.** It will not need attention again until beat 6.

---

## 4. `1:40 – 2:40` — The gate, while it runs

**Nothing typed.** Point at Terminal A as the self-test scrolls past.

```
  gate basis            content   (ADR-1: the content basis is THIS PROJECT'S decision, not the client's)
  OQ-2                  UNANSWERED. ...
  content comparison    PASS
  byte comparison       DIFFER
  GATE                  PASS   on the content basis
                        This does NOT discharge [B]'s acceptance criterion 2 as written.
```

> "Before any campaign run, it proves determinism: the same configuration twice, captured both
> times, compared. The brief makes that a hard stop — *do not build further until it passes* — so
> the campaign runs it itself rather than trusting anyone to remember. If it fails, no campaign
> run is attempted at all.
>
> Both comparisons always run and both are always reported. Two identical runs here agree on
> every sample present in both — nine and a half million samples over a hundred and ninety pairs,
> zero differing — and are **never byte-identical**, because about 0.2% of frames go unpublished,
> differently every run, with every platform counter reading zero.
>
> The brief asks for 'identical captures'. Whether that means content or bytes is an open
> question with its author and it is **unanswered**. So both readings are built as selectable
> gates: under `--gate-basis bytes` the gate correctly fails and the campaign correctly stops. A
> ruling changes a default and no code. Every report says so — I'm not going to tell you this
> project met acceptance criterion 2, because it hasn't."

---

## 5. `2:40 – 3:20` — Re-judging, and the boundary you can see

**Terminal B** — the window with **no N8RO on its `PATH`**. That is the point of this beat.

```cmd
build\n8ro-judge\n8ro-judge.exe campaign campaigns\m6-campaign ^
  --conditions examples\atacama-raid.conditions.json ^
  --verify verdicts.jsonl ^
  --quiet
```

```
re-judging campaigns\m6-campaign
  7 condition(s) from examples\atacama-raid.conditions.json
  no host is started and no bus subscription is made - this binary links nothing that could

  000  fail   satisfied 3  violated 3  indeterminate 1   (met 1, not met 5)
    verify: 7 verdict(s) byte-identical to the live run's verdicts.jsonl
  ...
  20 run(s) judged
```

> "A week after the campaign ran you think of a new question. This answers it against the stored
> captures in seconds, with **no host started and no bus subscription made** — and this binary
> couldn't make one. Look at the terminal: N8RO isn't on this window's PATH at all. The campaign
> runner won't even print its own help without it. This runs fine, because it links nothing.
>
> And `--verify` is checking something specific: that re-judging produces verdicts **byte-
> identical** to the live run's. That holds by construction — there's one evaluator and one input,
> a stored capture, and the live campaign judges through the same code over the same file. But a
> structural argument nobody checked is how a structural argument stops being true, so it's
> checked too. Twenty of twenty."

---

## 6. `3:20 – 4:10` — Read the report

**Terminal A first** — the demo campaign has finished by now. Scroll to its last lines.

```
[campaign] run          run 000 -> fail
[campaign] run          run 001 -> infrastructure_error
[campaign] run          run 002 -> fail
[campaign] campaign     3 run(s) attempted
[campaign] campaign       pass                  0
[campaign] campaign       fail                  2
[campaign] campaign       timeout               0
[campaign] campaign       infrastructure_error  1
[campaign] campaign       the four sum to 3, and no aggregate above merges two of them
```

> "There's the injected fault. The host was pointed at a path that doesn't exist, run 1 is an
> **infrastructure error** — not a failing scenario — and the campaign **carried on to run 2**.
> That's the brief's rule: never let an infrastructure failure count as a test result.
>
> And the two failures are real. At 200 frames the raid doesn't have time to reach the airfield,
> so the conditions asking whether it did are violated. That's a result, not a defect."

**Now Terminal B** — the committed twenty-run campaign, which is the report worth reading:

```cmd
build\n8ro-campaign\n8ro-campaign.exe report ^
  --out-dir campaigns\m6-campaign ^
  --campaign examples\atacama-raid-speed-20.json
```

> **Wait — this one needs `PATH`.** `n8ro-campaign` links the SDK, so Terminal B cannot run it.
> Either run this in **Terminal A** (which has `PATH` set and is now idle), or `type
> campaigns\m6-campaign\report.txt` in Terminal B. **Prefer `type` on camera** — it is the
> committed artifact rather than a re-render, and it avoids explaining a DLL error mid-recording.

```cmd
type campaigns\m6-campaign\report.txt
```

Point at the count table:

> "Twenty runs, ordered by parameter value — not by spelling, which would put 110 before 27.5.
> The bar is scaled between the minimum and maximum, not from zero, and the header says so: a
> column running 47 to 65 drawn from zero is twenty identical bars.
>
> And the shape is the interesting part. Engagement rises to a peak around 170–190 m/s and then
> **falls**. A raid fast enough to overfly is engaged less, not more. A sweep that reported
> 'higher is more' would be reporting a line that isn't there."

Then the verdict table:

> "This is what the sweep is for. One column per condition **whose outcome changes** — the ones
> that don't are listed below as constant, because a column that never changes isn't a trend. You
> can see three different thresholds: the corridor is entered from 100, the depot ring from 115,
> the airfield only from 170. And the run outcome flips with them — fail up to 130, pass from 170.
>
> Two runs are infrastructure errors, at 150 and 210. Their segment zero came out frozen, so
> nothing in those captures can be judged. It's a known platform behaviour, it happens about one
> run in nine, and there is deliberately **no retry** — a harness that re-rolls until it likes
> the answer has no gate."

---

## 7. `4:10 – 4:40` — The verdict that says it cannot say

**Terminal B.**

```cmd
type campaigns\m6-campaign\runs\000\verdicts.jsonl
```

Find `raid-leader-degraded` — or show it from the report's per-verdict lines:

```
raid-leader-degraded   INDETERMINATE   health=nominal equals=degraded
  no sample of RedUAV_N_01 carried health = "degraded" ... A field's rate of change is not
  bounded by anything in the format, so the value could have been taken and left between two
  samples. This form is never decidable in the negative.
```

> "This one is the point of the whole project.
>
> A capture is a very high-fidelity **sample** of what the run published — not a transcript — and
> loss has been measured with every counter reading zero. So *'no record says it happened'* is not
> the same claim as *'it didn't happen'*.
>
> A naive referee reports this NOT MET, confidently, from a file that may simply be missing the
> frame. This one says it cannot decide, and why. **Indeterminate is a verdict state and never a
> fifth run outcome** — the brief fixes those at four, and keeping the two vocabularies apart is
> what keeps that criterion exactly satisfied.
>
> Every verdict also carries what decided it: the entities with their occupancies, the line in
> the capture, the deciding simulation time, the measured value and the threshold. Enough to open
> the file and check it by hand."

---

## 8. `4:40 – 5:00` — The limits, which is where to stop

**Open `README.md` at *Limits*.** Nothing typed.

> "The last thing, and it's the part I'd want a reviewer to read first.
>
> A pass here means every declared condition was satisfied — **not that the run was correct**. It
> is a statement about the questions somebody thought to ask, over the data that reached the file.
>
> The gate refuses about one pair in fourteen for a reason that is not a determinism failure, and
> there's deliberately no retry. There's one condition the brief's own example implies that
> **cannot be expressed here**, and it's named. The arithmetic every geometric verdict rests on is
> this project's decision, because the vendored contract stopped one heading short of stating it —
> that's raised upstream, and the decision doesn't wait on the answer.
>
> All of that is in the README, in the limits section, not in a footnote. That's the difference
> between a demonstration and evidence, which is what the brief opens with."

---

## What to have ready but not show

- `campaigns\m6-gate-refused\` — the execution where the gate correctly refused and **zero runs
  were attempted**. Have it if anyone asks *"what happens when it fails?"*, because the answer is
  not hypothetical.
- `docs\determinism-notes.md` §5 — the five things that could not be explained.
- `campaigns\m6-faults\` — all four ugly realities, injected and survived.
- `campaigns\m6-campaign\changed-input-diff.txt` — the run-to-run diff, if there is time for the
  brief's *"change one input and show exactly where the two runs diverged"*.

## What not to say

- **Do not say the self-test discharges the brief's acceptance criterion 2.** It does not, under
  the content reading, and OQ-2 is unanswered.
- **Do not describe the twenty-run campaign as always succeeding.** It met R15 on its first
  attempt: the gate refused and no run was attempted.
- **Do not call an `indeterminate` verdict a failure of the tool.** It is the correct answer.
- **Do not call the sweep "reviewed".** The sweep-legibility metric is unmet until a mentor
  reviews it.

## If something goes wrong on camera

| symptom | cause | fix |
|---|---|---|
| A binary prints nothing and exits `-1073741515` | `C:\N8RO\bin` not on `PATH` (F-8) | You are in Terminal B. Use Terminal A, or `type` the committed artifact |
| The campaign sits at "scenario loaded" and never starts | `N8RO_RELEASE` not set (F-5) | `set N8RO_RELEASE=C:\N8RO` and relaunch |
| The campaign exits 3 having run nothing | The determinism gate refused — possibly R15 | **This is correct behaviour.** Say so, show the refusal naming its shape, and relaunch. Do not edit anything |
| A command breaks at the first `^` | You are in PowerShell | Switch to `cmd`, or replace `^` with a backtick |
| `n8ro-campaign` refuses to start a run | A host it did not create is already live (CR-EX-1) | Close the stray `n8ro-sim-app.exe` and relaunch |

## Timings, measured rather than estimated

| | |
|---|---:|
| one 200-frame run | **14.6 s** |
| the demo campaign in beat 3 — 2 self-test runs + 3 campaign runs, one injected | **88.7 s** |
| one 1200-frame run | ~70 s |
| the committed twenty-run campaign | ~25 min |
| `n8ro-judge campaign --verify` over 20 stored runs | a few seconds |
