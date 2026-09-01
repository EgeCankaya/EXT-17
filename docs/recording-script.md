# The 5-minute recording — the commands, in order

> **Status: RECORDED and PUBLISHED, 2026-09-01.** One take, **4 min 10 s**, shot to this runbook:
> **[Take 1, on Google Drive](https://drive.google.com/drive/folders/16cR82ynxrcmrzJofwHKdpReNlPj1C--M?usp=sharing)**,
> beside a command-by-command companion for a viewer. All eight steps below are on camera in
> order. The campaign it launches is the three-run, 200-frame demo with a host-start failure
> injected into run 1 — its records are committed at `campaigns/demo/` — and the report read at
> step 5 is the committed twenty-run campaign.
>
> **It is 4:10 against [B]'s *"5-minute recording"***, and that is stated rather than rounded up.
> Every clause of the deliverable is in it — *launch a campaign, watch it run, read the report* —
> and with no narration the commands simply take less time than they did when they were narrated.

The recording is a deliverable [B] names — *"a 5-minute recording: launch a campaign, watch it
run, read the report"* — and it needed a person. This document is everything except the person:
which shell, which command, in what order, and what each one puts on screen. **It is kept as the
record of what was filmed and how to reshoot any step**, not as a substitute for the film.

**Why the status is stated this precisely.** The PRD scheduled this at M7 *explicitly* because the
equivalent deliverable was not delivered by the upstream project (R10). The failure R10 names is
not lateness; it is **substituting a written walkthrough and calling the requirement met** — so
from M7 until the take existed, this file said `NOT RECORDED` in its first line. It now says the
opposite because the film exists and is published, and for no other reason.

**There is no narration, and nothing is opened in an editor. The recording is the terminal.**
That is a deliberate constraint rather than a shortcut, and it holds because **the tools caption
themselves**: the self-test block prints the gate basis and the OQ-2 wording, the report prints
why its bar is not scaled from zero and what `OK` / `XX` / `??` mean, and every verdict that is
not satisfied prints its own reason. A claim that would have to be spoken to be present is a
claim the recording cannot evidence — so if it is not on screen, it is not in the recording.

## What the brief asks for, and which step does it

| [B]'s words | step |
|---|---|
| *launch a campaign* | **3** — one command |
| *watch it run* | **4** — nothing typed for ~90 s |
| *read the report* | **5** |

Steps 1, 2, 6, 7 and 8 are beyond those three. They fill the remaining time and are the parts to
cut first if you run long — cut from the bottom, never from 3–5.

---

## 0. Before you press record

### 0.1 Use `cmd`, not PowerShell

**Every command here is written for `cmd.exe`:**

| | |
|---|---|
| The build scripts are `.cmd` | `tools\*\build.cmd` are batch files. In `cmd` you run them by name; in PowerShell each needs `cmd /c "tools\n8ro-judge\build.cmd"` |
| The line-continuation character differs | This project's README and help text use `^`, which is **cmd**. PowerShell uses a backtick `` ` ``, so a command copied from the README into PowerShell breaks at the first line break |
| Environment variables differ | `set N8RO_RELEASE=C:\N8RO` in cmd; `$env:N8RO_RELEASE="C:\N8RO"` in PowerShell |
| Running an exe with a quoted path differs | cmd: `"build\n8ro-judge\n8ro-judge.exe" --help`. PowerShell needs the call operator `&` |

`%errorlevel%` inside a single-line `&`-chain expands when the line is *parsed*, not when it runs,
so it reports the **previous** command's code. Every `echo %errorlevel%` below is on its own line
for that reason. Those echoes matter here: with no narration, the exit code is how the recording
says what a command decided.

### 0.2 One `cmd` window at the repo root, with both variables set

```cmd
cd /d C:\Projects\EXT-17
set N8RO_RELEASE=C:\N8RO
set PATH=C:\N8RO\bin;%PATH%
```

**Both are measured preconditions, not preferences**, and each fails as something else:

- Without `N8RO_RELEASE`, the host resolves its plugin directory from the working directory,
  skips its plugin scan, never registers `componentPhysics`, and **refuses every 42-entity
  scenario load while sitting idle rather than failing** (F-5). It looks like a hang.
- Without `C:\N8RO\bin` on `PATH`, an SDK-linked binary exits `-1073741515` (`0xC0000135`,
  STATUS_DLL_NOT_FOUND) having printed nothing at all (F-8). It looks like a crash.

One window is enough, because only `n8ro-campaign` needs either variable.

### 0.3 Which binary needs what — verified, not assumed

| binary | needs `C:\N8RO\bin` on `PATH`? | needs `N8RO_RELEASE`? |
|---|---|---|
| `n8ro-campaign` | **Yes** — even `--help` exits `-1073741515` without it | Yes, to hand to the host it starts |
| `n8ro-judge` | **No** | No |
| `n8ro-compare` | **No** | No |
| `n8ro-capture` | **No** | No |

The bottom three link nothing — not EXT-08, not the N8RO SDK — and that is checked on every build.
If you want the recording to *show* that rather than assert it, run steps 1, 2, 6, 7 and 8 in a
second `cmd` window that never had `set PATH=C:\N8RO\bin;%PATH%` run in it. They work there;
`n8ro-campaign` will not print its own help.

### 0.4 Clear the demo directory, so the launch is from nothing

```cmd
rmdir /s /q campaigns\demo 2>nul
```

### 0.5 Off camera, once, before the take

```cmd
tools\n8ro-campaign\build.cmd
tools\n8ro-judge\build.cmd
tools\n8ro-compare\build.cmd
build\n8ro-campaign\n8ro-campaign.exe --help
```

The last line is the cheapest check that `PATH` is right in *this* window: it prints help, or it
prints nothing and exits `-1073741515`. Then re-run 0.4 and start recording.

**N8RO binaries write into their working directory** — `n8ro-sim-local.exe` drops a per-entity
JSONL dump, `n8ro-sim-app.exe` creates `data\db\` and `logs\` (F-6). Running the commands in this
file from the repo root is safe because the campaign gives every child its own directory; running
an N8RO binary directly from here is not.

---

## 1. `0:00 – 0:25` — Validate the condition file

```cmd
build\n8ro-judge\n8ro-judge.exe check --conditions examples\atacama-raid.conditions.json
echo %errorlevel%
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
0
```

## 2. `0:25 – 0:45` — The same check, on a file with one character wrong

```cmd
build\n8ro-judge\n8ro-judge.exe check --conditions docs\demo\typo.conditions.json
echo %errorlevel%
```

```
n8ro-judge: condition file refused - unknown_key [raid-leader-reaches-airfield]:
  "within_meters" is not a key of a proximity condition. A condition file is ours and a
  person wrote it, so an unrecognised key is refused rather than ignored - it is how
  "within_meters" for "within_m" would otherwise become a threshold that silently did not
  apply. A key beginning with '_' is a comment
2
```

This is the check `n8ro-campaign` runs before any host starts, so a typo costs ten seconds instead
of twenty runs. The message says the rest itself.

---

## 3. `0:45 – 1:00` — Launch the campaign. One command.

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

No keystroke per run, no prompt, no manual host start or teardown — that is the brief's first
acceptance criterion, and one command is the whole demonstration of it.

**Two deliberate deviations from the committed campaign, both visible in the output rather than
hidden:** the runs are **200 frames instead of 1200** so the campaign finishes inside the
recording, and **run 1's host is deliberately broken** so an unattended campaign's behaviour under
failure is on camera. Both are written into that run's `run.json` and into the campaign summary, so
a run under injection can never be mistaken for a clean one. The report read in step 5 is the
*real* one — twenty runs at 1200 frames, ~25 minutes, committed.

## 4. `1:00 – 2:30` — Watch it run. Nothing typed.

The self-test runs first and prints the gate; then the three campaign runs.

```
  gate basis            content   (ADR-1: the content basis is THIS PROJECT'S decision, not the client's)
  OQ-2                  UNANSWERED. ...
  content comparison    PASS
  byte comparison       DIFFER
  GATE                  PASS   on the content basis
                        This does NOT discharge [B]'s acceptance criterion 2 as written.
```

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

Everything worth saying here is printed: both comparisons ran and both are reported, the gate names
its basis and says what it does not discharge, run 1 is an **infrastructure error** rather than a
failing scenario, and the campaign **carried on to run 2**. The two failures are real — at 200
frames the raid cannot reach the airfield, so the conditions asking whether it did are violated.

Elapsed here is **88.7 s** measured. If the gate refuses instead, see the trouble table: that is
correct behaviour and not a retake.

---

## 5. `2:30 – 3:30` — Read the report

The committed twenty-run sweep, re-rendered from its stored run records:

```cmd
build\n8ro-campaign\n8ro-campaign.exe report ^
  --out-dir campaigns\m6-campaign ^
  --campaign examples\atacama-raid-speed-20.json
```

Nothing is run, no host is started and no capture is read — it prints through the same printer the
live campaign used, so a re-render cannot disagree with what the campaign printed. `--campaign`
supplies the sweep's order, which the axis declares and which is not re-derivable from the run
records: it is why `110` sorts after `27.5` rather than before it.

Let it sit on screen. It is 79 lines and it carries its own captions:

```
[campaign] sweep          value  run   outcome                   adds     keys   samples
[campaign] sweep          11     000   fail                        48       48     50534  ##
...
[campaign] sweep          150    010   infrastructure_error         -        -     49007  (no bar - this run did not complete)
[campaign] sweep          170    011   pass                        65       65     48927  ########################################
...
[campaign] sweep          the bar is `adds` scaled between 47 and 65 - NOT from zero, ...
```

```
[campaign] sweep          value  run   outcome               C1    C2    C3    C4    C5    C6
[campaign] sweep          100    007   fail                  XX    OK    XX    OK    OK    OK
[campaign] sweep          115    008   fail                  XX    OK    OK    OK    OK    OK
[campaign] sweep          170    011   pass                  OK    OK    OK    OK    OK    OK
...
[campaign] campaign     20 run(s) attempted
[campaign] campaign       pass                  8
[campaign] campaign       fail                  10
[campaign] campaign       timeout               0
[campaign] campaign       infrastructure_error  2
```

The three thresholds (corridor from 100, depot ring from 115, airfield only from 170), the peak in
`adds` around 170–190 followed by a **fall**, the four outcomes that sum and are never merged, and
the condition that is constant across the sweep and is therefore listed below the table rather than
given a column — all of it is legible without a word spoken.

## 6. `3:30 – 4:05` — Re-judge the stored runs

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

  000  fail                  satisfied 3  violated 3  indeterminate 1   (met 1, not met 5)
    verify: 7 verdict(s) byte-identical to the live run's verdicts.jsonl
  ...
  010  infrastructure_error  satisfied 0  violated 0  indeterminate 7   (met 0, not met 0)
    no segment classified running, so nothing in this capture can be judged: ...
  ...
  20 run(s) judged
    pass                  8
    fail                  10
    timeout               0
    infrastructure_error  2
    (the four outcomes sum to 20, and no aggregate merges two of them)
    indeterminate VERDICTS 32 - a verdict state, never a fifth run outcome
```

Stored runs re-judged in seconds against a condition file, with **no host started and no bus
subscription made**. `--verify` byte-compares each re-judgement against the verdicts the live run
wrote: twenty of twenty identical. The exit code here is **1**, because indeterminate verdicts
exist — the `verify:` lines are what to read, not the exit code, so do not `echo %errorlevel%` on
this one.

## 7. `4:05 – 4:35` — One run's verdicts, including the one that says it cannot say

```cmd
build\n8ro-judge\n8ro-judge.exe capture ^
  campaigns\m6-campaign\runs\000\capture-atacama-air-defense-000.n8rocap.jsonl ^
  --conditions examples\atacama-raid.conditions.json
```

```
  capture  fail                  satisfied 3  violated 3  indeterminate 1   (met 1, not met 5)
    raid-leader-reaches-airfield       NOT MET <- VIOLATED   t=59.99999999999873 closest_approach_m=8693.1695 within_m=3000 [RedUAV_N_01@1 line 50580, BlueBase_Airfield@1 line 50551]
      closest approach 8693.1695 m at sim_time_s 59.99999999999873, against a threshold of 3000 m.
      The margin of 5693.17 m exceeds the 1.20 m they could have closed inside the largest
      unobserved window (0.1000 s at a relative 11.0 m/s), so they did not reach it
    raid-leader-destroyed              NOT MET (as asserted) removed_with=scenario_unload removal_reason=destroyed [RedUAV_N_01@1 line 50621]
    airfield-operational               met                   t=0.05 phase=operational equals=operational [BlueBase_Airfield@1 line 69]
    raid-leader-degraded               INDETERMINATE         health=nominal equals=degraded
      no sample of RedUAV_N_01 carried health = "degraded" - the last value seen was "nominal".
      A field's rate of change is not bounded by anything in the format, so the value could have
      been taken and left between two samples. This form is never decidable in the negative
```

Every verdict names what decided it: the entities with their occupancies, the line in the capture,
the deciding simulation time, the measured value, the threshold, and — for a not-met geometric
verdict — the bound the conclusion rests on. The last verdict is the point of the whole project and
it argues for itself on screen: a capture is a very high-fidelity **sample**, so *"no record says
it happened"* is not *"it did not happen"*, and the verdict says it cannot decide instead of
reporting a confident NOT MET.

## 8. `4:35 – 5:00` — Change one input, and show exactly where the two runs diverged

```cmd
build\n8ro-compare\n8ro-compare.exe ^
  campaigns\m6-campaign\runs\000\capture-atacama-air-defense-000.n8rocap.jsonl ^
  campaigns\m6-campaign\runs\019\capture-atacama-air-defense-019.n8rocap.jsonl ^
  --changed-input
```

```
    FIRST DIFFERENCE    segment (part 0, segment 0)  entity BlueSAM_ShortRange_wpn_600_0@1
                          sim_time_s 0.7000000000000001
                          field "positionGeodetic": [-23.496501124623734, ...]   against   [-23.496492816664038, ...]
                          capture-atacama-air-defense-000.n8rocap.jsonl line 586, capture-...-019.n8rocap.jsonl line 562

  DIVERGED              at the point named above - segment, (entity, occupancy), sim_time_s
                        and field. That is the brief's "exactly where", ...
                        30066 of 44345 compared sample(s) differ. The count is context; the FIRST
                        one is the finding, because everything after it is downstream of it.
```

`--changed-input` is a framing rather than a mode: two runs at **different** inputs, where a
divergence is the answer and agreement would be the finding worth chasing. The byte comparison and
result equality are deliberately not run here, and the output says why.

---

## If something goes wrong on camera

| symptom | cause | fix |
|---|---|---|
| A binary prints nothing and exits `-1073741515` | `C:\N8RO\bin` not on `PATH` (F-8) | Only `n8ro-campaign` needs it. `set PATH=C:\N8RO\bin;%PATH%` and re-run |
| The campaign sits at "scenario loaded" and never starts | `N8RO_RELEASE` not set (F-5) | `set N8RO_RELEASE=C:\N8RO` and relaunch |
| The campaign exits 3 having run nothing | The determinism gate refused — possibly R15 | **This is correct behaviour.** Let the refusal stand on screen naming which shape it found, then relaunch. Do not edit anything, and do not cut it out |
| A command breaks at the first `^` | You are in PowerShell | Switch to `cmd`, or replace `^` with a backtick |
| `n8ro-campaign` refuses to start a run | A host it did not create is already live (CR-EX-1) | Close the stray `n8ro-sim-app.exe` and relaunch |
| Step 6 or 7 exits `1` | An indeterminate verdict, or a violated condition | Not an error, and not a retake. The verdict lines are the result |

## Timings, measured rather than estimated

| | |
|---|---:|
| one 200-frame run | **14.6 s** |
| the demo campaign in step 3 — 2 self-test runs + 3 campaign runs, one injected | **88.7 s** |
| one 1200-frame run | ~70 s |
| the committed twenty-run campaign | ~25 min |
| `n8ro-judge campaign --verify` over 20 stored runs | a few seconds |
| every other command in this file | under a second |

## If you caption, title or describe the recording anywhere

There is no narration, so the tools' own text carries every claim — which makes a caption the one
place a claim could enter that the terminal does not support. Three that must not:

- **The brief's author did not rule on the gate's basis.** They never replied. It is **decided**
  — by the DRI on 2026-09-01, from the brief's own words — and *decided* is not *answered*.
  Criterion 2 is discharged **under the content reading**, and that qualifier goes in the same
  breath every time.
- **The mentor did not rule on it either.** They **concurred**, separately and independently,
  which is worth saying — but never alone: "still not answered by the brief's author" goes in the
  same breath. The reports do exactly that, and two tests enforce it.
- **An `indeterminate` verdict is not a failure of the tool**, and the twenty-run campaign is not
  a campaign that always succeeds — it met R15 on its first attempt, where the gate refused and no
  run was attempted (`campaigns\m6-gate-refused\`).

**Four lines in this file have gone stale within hours of being written, twice on 2026-09-01** —
first the sweep being unreviewed and criterion 2 unruled, then the invocation being confirmed in
only four of six parts. All were true when written and all were overtaken the same day. Re-read
this section before the camera goes on.
