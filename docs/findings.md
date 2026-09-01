# Findings — one index for everything this project has found

**Every issue found while building EXT-17, in one table.** It exists because the findings
themselves were always recorded and were spread across five places — the milestone documents,
`docs/escalations.md`, `CLAUDE.md`, the README's limits section and the PRD's risk table — under
section names that differ per milestone. Answering *"what has this project found?"* required
knowing where to look and what it was called there.

**This file is an index, not a second home.** Every row points at where the finding actually
lives, with its numbers. Nothing here is the authoritative record of anything; if a row and its
target disagree, the target wins and the row is wrong.

**Status vocabulary, and the distinction that matters most:**

| | |
|---|---|
| `closed` | Understood, and either fixed here or worked around with the workaround documented |
| `open` | Understood and **not** fixed, because it is not this project's to fix. The row says whose |
| `raised` | Sent to whoever owns it. A reply may or may not exist — `docs/escalations.md` tracks that separately |
| `drafted` | Written up and **not** sent. **This is not the same as raised**, and conflating the two is how a project comes to believe it has told someone something it has not |
| `decided` | **Added 2026-09-01.** A person entitled to decide it did, on stated evidence, and **the original recipient still has not spoken**. Stronger than `raised`, **weaker than `answered`**. Reading it as `answered` is the failure this table exists to prevent, so every row carrying it says which in the same sentence it says what was decided |
| `answered` | A reply exists, from the person the question was for. **E-1 is answered in full as of 2026-09-01** — four parts on the first relay, the remaining two on a follow-up. They were held at "not answered" in between rather than rounded up, which is the whole point of having both words, and closing them cost one question |
| `concurred` | **Added 2026-09-01, and the subtlest one here.** Somebody *other than the recipient*, entitled to an opinion, independently reached the same answer. It sits **beside** `decided`, not above it: it raises confidence and does **not** convert a decision into a ruling, because the recipient still has not spoken. One row carries it — E-2, where the mentor said content and only [B]'s author can discharge [B]'s criterion 2 |
| `fixed` | **Added 2026-09-01, and it is the strongest state here.** The recipient changed the thing, the change is on their `main`, and the correction has been **vendored back**. Stronger than `answered`: a reply is a sentence, a fix is a diff. Four rows carry it — E-3, E-4, E-5 and E-6, all against EXT-08 — and they are the first questions this project asked that came back as edits |

---

## A. Defects in this project's own code

Found by exercising the thing rather than by reading it, which is the argument for having
exercised it. All fixed.

| # | Found | What | Status | Recorded in |
|---|---|---|---|---|
| F-1 | M3 | A **relative `--out-dir`** was passed through to the recorder, which resolved it against the run directory it had just been placed in, found nothing, and refused — into a file nobody was reading. Every child runs in its own working directory | `closed` — resolved with `GetFullPathName` before anything reaches a child | `m3-capture-reader.md` §5(a), `CLAUDE.md` |
| F-2 | M3 | **A run that recorded nothing was reported `completed`.** It started no recorder, ran to frame 1200, and was classified as a completed run that M6 would later have judged — judging nothing | `closed` — now `infrastructure_error` with a named stage | `m3-capture-reader.md` §5(b) |
| F-3 | M3 | **A capture covering a third of its run was indistinguishable from one covering all of it.** The `stop` probe recorded to `sim_time_s` 19.5 of 60.0, conformantly, and nothing outside the file said so | `closed` — `n8ro-campaign` reads back every capture it produces; `capture.covers_whole_run` in the run record | `m3-capture-reader.md` §5(c), `m3-oq6.md` |
| F-4 | M3 | **`directorySizeBytes` returned 0** from a mangled path separator, so the mid-campaign disk ceiling silently never fired — four runs and 30.8 MB against a 10 MB ceiling | `closed` — found by testing the ceiling rather than by reading the code | `m3-capture-reader.md` §5 (closing note) |

| F-23 | M5 | **A campaign file silently accepted a key written twice, and an unknown key, first-wins.** Found by a test written against the axis parser, not by reading it: `"kind"` twice resolved to the first and the second line did nothing. Inherited from the JSON parser's §13-correct behaviour, applied to a file where §13 does not apply | `closed` — a campaign file is ours and a person writes it, so a duplicate key and an unknown key are both **refused by name**; a key beginning with `_` is a comment. The capture reader's §13 behaviour is untouched | `m5-sweep.md` §4, README, `tests/parameter_test.cpp` |

| F-24 | M5 | **A run with no RUNNING segment reported its result as `0`, and the sweep table plotted it.** Found by running the committed sweep, not by testing it: two of seven runs hit R14's frozen segment 0, so nothing was measured in them - and `0` was printed in the result column, drawn as a bar, **and used as the bar scale's minimum**, which made every other bar in the table wrong as well | `closed` - a run with no running segment now prints `-`, is excluded from the bar and from its scale, and the table says how many points the sweep is short. `run.json` and `campaign.json` write `null` rather than `0`. A missing measurement is not a measurement of zero (tenet 3, turned on our own report) | `m5-sweep.md` §5, README limits |

| F-25 | M5 | **`tools/spike-axis/build.cmd` had not linked since M3.** M3 gave `RunOnce` a read-back of the capture it had just produced, which added a dependency on the capture reader; the M2 spike's build script was never re-run and silently stopped building two milestones ago. Nothing depended on it, which is exactly why nothing noticed | `closed` - sources added, and it builds. The wider finding is the one worth keeping: **a build script outside the main path rots invisibly**, and M5 found this only because it wrote a sibling script and ran both | `m5-sweep.md` §2, `tools/spike-axis/build.cmd` |

| F-26 | M6 | **The assertion suite's own capture builder formatted numbers through the current locale.** It runs every check twice, the second time under `German_Germany.1252`, and the builder used the C library's fixed-point formatter — so the second pass wrote `"sim_time_s":0,05` into what is supposed to be JSON. Every sample was then rejected as malformed, the segment classified `indeterminate`, and the pass would have "agreed" with the first about a pile of verdicts that were never computed | `closed` — the builder uses the product's own locale-free `fixed()`. **The finding is that a locale-safety test was itself locale-unsafe**, and it went unnoticed because M5's parameter suite never formats a number so the same double pass was genuinely clean there | `m6-assertions.md` §5, `tests/assertion_test.cpp` |

| F-27 | M6 | **A host that dies mid-run is not noticed until the run timeout expires.** The wait blocks on engine-state publications; when the host goes away they simply stop arriving, and there is deliberately no second timed quantity watching for silence — CR-EX-4 makes the run timeout the only clock in a run | `open` **by decision** — an unattended campaign meeting [B]'s fourth ugly reality survives it and pays `--run-timeout-ms` in wall clock for each occurrence. A heartbeat-silence detector would be a second clock in a run; the cheaper answer is to size `--run-timeout-ms` against the frame budget, and the README says so | `m6-assertions.md` §6, README limits, `src/run/RunOnce.cpp` |

| F-28 | M6 | **The continuity bound assumes the platform cannot exceed its own measured acceleration clamp inside one frame.** That is an inference from F-21, which measured 20 m/s² on one entity profile in one scenario, and it is the weakest link in the soundness argument for a `not_met` proximity or area verdict | `open` — stated in `src/assert/Judge.h` and in the README rather than hidden. Its practical weight is small: measured across the committed sweep the term contributes 0.10 m to bounds of 1.20–22.10 m, and the tightest margin clears its bound 64× | `m6-oq5.md` §4, `decisions-m6-m7.md` A3 |

| F-29 | M6 | **The axis update races the roster burst, and the loser is the whole campaign.** F-22's mechanism has a fourth observable form: instead of making one segment `frozen`, it can leave the two self-test runs carrying **different `velocityNed` at `sim_time_s` 0** — 23 samples, on the first execution of the twenty-run campaign. The pre-`start` update lands before the burst in one run and after it in the other, so one capture holds the scenario's authored velocity and the other holds ours | `open` — **the gate is correct to fail and was not weakened**; this is the first time the *content* gate has failed on a real pair for a real reason, and `campaigns/m6-gate-refused/` is that execution, kept. The consequence is worse than the frozen-segment shape: exit 3 and **zero runs attempted**, not one run lost. Strengthens E-4 | `m6-assertions.md` §4, `campaigns/m6-gate-refused/`, README limits, E-4 |

| F-30 | M6 | **The platform round-trips an injected `velocityNed` through a conversion that leaves `-1.0103336092965664e-14` where the scenario's authored value is exactly `0`.** Ours is computed exactly — `direction × value`, no normalisation — so the artifact is introduced between `sendEntityUpdate` and the capture. It is what makes F-29's race *visible*: with a bit-identical vector the race would produce identical values and no difference at all | `open` — the platform's, not ours, and not worked around. Recorded because it is the mechanism behind F-29 and because a consumer comparing an injected value against an authored one will meet it | `m6-assertions.md` §4 |

| F-31 | M6 | **Every difference in an array-valued field printed two EMPTY values.** M4's diff rendered a value as `isNumber() ? raw() : text()`, and `text()` is empty for an array — so `positionGeodetic`, `velocityNed` and `orientationYprRad`, three of the four fields a divergence is most likely to be in, named the field correctly and then showed nothing. CR-DET-3 and CR-REP-4 both require the deciding **values** | `closed` — a verbatim renderer, and the synthetic capture in `determinism_test` now carries an array field so the suite can see it. **It survived three milestones because it takes a real content-gate failure on a real pair to reach that code**, and until F-29 the content gate had never failed on one — M4's failing-gate evidence came from forcing `--gate-basis bytes`, which reports a byte offset and never gets there | `m6-assertions.md` §4, `tests/determinism_test.cpp` |

| F-35 | M6 | **F-24's fix broke again, the same table, one milestone later.** M5 stopped a run with no running segment printing its result as `0`; the guard it added asked `outcome == Completed`. M6 renamed the outcome a judged run gets — `pass` or `fail` — and the guard stopped matching, so the twenty-run campaign's sweep table printed **`-` and no bar on every single row**, including the eighteen runs that measured perfectly well | `closed` — the predicate now asks whether the run *executed* (`pass`, `fail` or `completed`), which is the question it always meant. **Found by reading the twenty-run campaign's output, exactly as F-24 was**, and the lesson is the same one twice: a guard keyed on an enum value is a guard that breaks silently when the enum grows. `report` now re-renders a stored campaign so a table can be checked without a 25-minute re-run | `m6-assertions.md` §4, `campaigns/m6-campaign/report.txt` |

| F-40 | M7 | **The committed campaign's REPORT was never committed, and `.gitignore` said in a comment that it was.** [B]'s third deliverable is *"a real campaign — its configuration, its captured runs and **its report**, committed as an example"*, and the whitelist re-included `*.md`, `*.json` and `*.log` at campaign level — while the report the campaign actually writes is `report.txt`. So did `changed-input-diff.txt`, which is the **only** artifact of [B]'s *"change one input and show exactly where the two runs diverged"* and, unlike the report, is **not regenerable** without the 480 MB of captures that are deliberately excluded. Four documents cited the three files by name, and `docs/recording-script.md` instructed `type campaigns\m6-campaign\report.txt` **on camera** — a command that failed on a clean clone. The same pattern one level down hid all four fault campaigns and the whole of `m3-oq6/`: the patterns matched `campaigns/*/` and the nested campaigns live at `campaigns/m6-faults/<fault>/` | `closed` — `*.txt` whitelisted, the depth-2 rules added, and the two rules that were got wrong written into `.gitignore` beside them. **The depth-2 transcript is whitelisted as `campaign.log` BY NAME, never `*.log`**: at that depth the glob also matches a run directory, and `*.log` there would have committed about 200 MB of host logs silently. Found by auditing the delivered tree against [B] rather than by any check — `git status` is clean whether a deliverable is committed or ignored, which is exactly why this survived seven milestones | `.gitignore`, `README.md` §"The evidence" |
| F-41 | M7 | **A check count quoted in the README rotted, and it is the third counting-drift finding here.** `README.md` read *"78 + 96 + 126 + 166 = 466 checks"* after the determinism suite had grown from 96 to **105** — the four OQ-2 guard checks among them — so the total was 475 and the document said 466. The arithmetic was self-consistent, which is what made it invisible: nothing in it was wrong except a number nobody re-derived | `closed` — `tests\build.cmd` now **sums what the suites actually printed** and fails the build against `tests\checks.golden.txt`, the same mechanism and the same discipline as the four golden `--help` files. This is F-24 and F-35's lesson a third time and the first time the cure is structural rather than a corrected number: a figure no execution produces is a figure that drifts | `README.md` §"Building", `tests/build.cmd`, `tests/checks.golden.txt` |
| F-42 | handover | **A fresh clone rewrote the vendored `contract/` files, and only a clone could see it.** There was no `.gitattributes`, so what a clone contained depended on the cloner's git configuration; `core.autocrlf=true` is Git for Windows' default. Measured on a cold clone of `master`: **7180 CR bytes** injected into `capture-atacama-air-defense-sample.n8rocap.jsonl` and **78** into `example.conditions.json`, against 0 in the working directory. Invisible from the working directory because those files were written LF by the vendoring and never checked out again — the working copy stayed correct while every clone did not | `closed` — **narrower than it looks, and the file says so rather than overstating it**: `CaptureReader.cpp` strips a stray CR (format §2) and `JsonParse.cpp` treats one as whitespace, so both files still parsed and all checks still passed on a fresh clone. What it broke is what `contract/` is FOR — E-5 is vendored **by identity**, and a checkout entitled to rewrite 7180 of its bytes means the identity held on one machine and nowhere else. `contract/**` and `*.n8rocap.jsonl` are now `-text`; the goldens, verdicts and committed campaign evidence are `text eol=crlf`. Zero index churn: all 300 tracked files were already stored LF. Verified by re-cloning and comparing SHA-256, and regression-guarded by the CI job | `.gitattributes`, `.github/workflows/zero-install-tier.yml` |
| F-43 | handover | **All eight build scripts hard-coded one machine's Visual Studio *Insiders* path**, so every one of them failed on any machine but the development machine — at the FIRST command `README.md` asks a reader to run. Nothing caught it because nothing had ever built this repository anywhere else. Raised against this project by EXT-08's closing audit, which guessed five files; it was eight | `closed` — `tools\find-vcvars.cmd` discovers the toolchain through `vswhere`, in two tiers: `sdk` requires 18.x to match what `C:\N8RO` 2.1.328 was built with, `free` accepts 17.x or 18.x for the four targets that link nothing. **It deliberately does not call `C:\N8RO\dev\setup-dev.cmd`, which already does this discovery** — that script requires the install by construction, and inheriting it would make `tests\build.cmd` depend on `C:\N8RO` and destroy the zero-install claim outright. `/Fd` was added to all twelve compile lines in the same pass; without it every build dropped a `vc140.pdb` into the caller's working directory, and there was one in the repository root | `tools/find-vcvars.cmd`, `README.md` §"Building" |
| F-44 | handover | **The check-count golden was a property of the machine, and it had been since the day it was added.** `capture_reader_test`'s tier 4 reads the real producer-0.9.0 captures under `campaigns\m2-oq1\runs`, which are untracked and 569 MB — so it runs where they are and is skipped everywhere else, contributing **6** further checks. The suite therefore ran **475** on the development machine and **469** on a clean runner, **zero failures both times**, and the golden failed the build on the second | `closed` — **this is F-41's own cure exhibiting F-41's defect**: that golden exists to stop a total drifting silently, and a golden that only holds in one place is the same thing it was built to prevent. The suite now prints its optional count on its own line and `tests\build.cmd` subtracts it, so the pinned number is one every machine reproduces; the optional checks still run wherever the captures are and a failure in one still fails the suite, only the COUNT is held apart. Golden 475 → 469. **Found by the CI job on its first execution**, which is the argument for the CI job | `tests/build.cmd`, `tests/capture_reader_test.cpp`, `tests/checks.golden.txt` |
| F-45 | handover | **Both spike build scripts had been failing to link since M6, and nothing noticed.** `tools\spike-axis` and `tools\spike-oq4` both compile `src\run\RunOnce.cpp`; M6 gave it verdicts to write, neither was rebuilt afterwards, and both died with **five unresolved `ext17::assertion::` externals** for two milestones. Found by building every script in the tree rather than the four `README.md` lists | `closed` — `src\assert\{Geodesy,Conditions,Judge}.cpp` added to both, which is what `n8ro-campaign` already links. **The second occurrence of one pattern, and that is the part worth keeping**: `spike-axis`'s own header already recorded going stale the same way at M3, when `RunOnce` gained the capture read-back, and being fixed at M5 — the comment describing that fix was sitting three lines above the break that replaced it. A spike is evidence rather than product, but a build command committed to the repository is one a reader is invited to run, and nothing here executes these two. F-38's shape, one level down | `tools/spike-axis/build.cmd`, `tools/spike-oq4/build.cmd` |

**F-42 to F-45 were all found in one pass, after M7 closed, and none of them by reading the repository.**
Three came from asking what a *fresh clone on a fresh machine* actually does — cloning cold, then building
every script in the tree rather than the four the README lists. The fourth came from the CI job on its first
execution. Every one had been true for at least a milestone, and every one was invisible from a working
directory that had the SDK, the untracked captures and the right Visual Studio already in place. That is the
same lesson as EXT-08's own closing audit, which found four packaging defects the same way and none of them
from the working tree: **the passes that read the repository cannot see what ships.**


**M4 found none of these in its own new code.** That is a weaker claim than it looks: M4's code is
what M4's 75 tests were written against, and three of the four above were found by running probes
rather than by testing. The equivalent probe work at M4 was the byte-gate and overload runs, and
they found platform behaviour (F-13) rather than harness defects.

**M5's OQ-4 decision spike found none either, and the same qualification applies twice over** —
it ran before M5 had code of its own to defect in. What it found is two rows of platform
behaviour, F-21 and F-22, both from exercising a *range* rather than a value. **F-23 and F-24 are M5's
own.** F-23 was found by a test rather than by a probe — the first row in this section of which
that is true, and what a testable configuration surface buys. **F-24 was not**: it took running
the committed sweep on real runs to find that a missing result was being printed as a zero one,
which is the same lesson M3's §5 closing note drew and the reason the sweep is run rather than
only unit-tested.

---

## B. The platform and the environment — ours to work around, not to fix

| # | Found | What | Status | Recorded in |
|---|---|---|---|---|
| F-5 | M1 | **`N8RO_RELEASE=C:\N8RO` is required by the headless host**, not only by `n8ro-sim-local`. Without it the host skips its plugin scan, never registers `componentPhysics`, and **refuses every 42-entity scenario load while sitting idle rather than failing** — the dangerous shape for an unattended campaign | `closed` (set for every child) / `open` upstream — `PROVENANCE.md` finding 6 omits it | `CLAUDE.md`, README preconditions, `m1-lifecycle.md` §7(a), and E-1(b) |
| F-6 | M1 | **N8RO binaries write into their working directory** — `n8ro-sim-local` a per-entity JSONL dump, `n8ro-sim-app` a `data/db/` and `logs/`. It happened in this repo's root before the rule was known | `closed` — every binary runs from a scratch directory | `CLAUDE.md`, `m1-lifecycle.md` §7(b) |
| F-7 | M1 | **`PROVENANCE.md` finding 7's deadline is the scenario load, not the host start.** The recorder must be attached before the roster burst, which is published at load | `closed` — recorder starts first, `attached_mid_run: false` verified on every run | `m1-lifecycle.md` §7(c) |
| F-8 | M2 | **`C:\N8RO\bin` on `PATH` is a second, separate precondition.** An SDK-linked binary launched from elsewhere exits 53 with no output at all — reads like a crash, is a missing DLL. Setting `N8RO_RELEASE` does not cover it | `closed` (set for children, needed by us) | `CLAUDE.md`, README preconditions, `m2-automation.md` §3(a), E-1(f) |
| F-9 | M2 | **The host appends to one fixed log inside the read-only install tree.** Twenty runs accumulate in one file; two identical runs produced exactly twice one run's bytes. Redirected stderr is *not* a reliable record — one run wrote 5 777 navigation errors to the log and three lines to `host.err` | `closed` (byte offset recorded before each start, only the new bytes copied) / `open` upstream — it is a workaround | `CLAUDE.md`, `m2-automation.md` §3(b), E-1(e) |
| F-10 | M2 | **There is no shutdown command**; `sim/engine/command` is closed at `start`/`stop`/`pause`/`step`. A `CTRL_BREAK_EVENT` shuts the host down cleanly but Windows still reports `0xC000013A`, so **a non-zero host exit is the normal case here and is not evidence of a crash** | `closed` (control event preferred, termination-by-handle the fallback) | `CLAUDE.md`, README, `m2-automation.md` §3(c), E-1(e) |
| F-11 | M2 | **A run's captured sample count varies between identical runs; its stopping frame does not.** Twenty runs, **seventeen distinct sample counts**, up to five whole frames missing, 0.38% spread — with all nine drop and bus counters reading zero. Meanwhile all twenty agree exactly on the roster lifecycle | `open` — a property of the platform, not a defect. It is the basis of the stop predicate (OQ-1) and of M4's gate | README limits, `m2-oq1.md`, `m4-determinism.md` §3 |
| F-12 | M4 | **The recorder's `--queue-size` is not `header.subscription.queue_size`.** The first bounds the handler-to-writer queue and is what makes `samples_not_recorded` non-zero; the second is the bus subscription's and read `1024` under both the default and `--queue-size 4`. So a like-for-like check on `header.subscription` cannot see a queue-size difference | `closed` — documented; the drop counter catches it instead, and is checked first | `CLAUDE.md`, `m4-determinism.md` §6 |
| F-13 | M4 | **Segment 0 can classify `frozen` without the clock having reset.** Part of the start-up roster burst is published twice with byte-identical values — 13 instants in one run — inside a segment with 1 200 distinct `sim_time_s` spanning 0 to 60. Measured **2 of 42 captures**; ~3.7% of ordinary runs, so **~1 pair in 14**, which means a campaign stops at its own gate that often for a reason that is not a determinism failure | `open` — the exclusion is correct and stays; **deliberately no retry**. Raised as E-4 | `m4-determinism.md` §5, README limits, `CLAUDE.md`, E-4 |
| F-14 | inherited, re-derived | **The install ships no geoid grid and no elevation service**, so every run floods with terrain errors | `open` **by decision** — deliberately not fixed. Every inherited measurement was taken in this configuration and provisioning terrain would invalidate the comparability of all of it | `CLAUDE.md`, README limits, E-1(d) |
| F-21 | M5 | **An injected entity speed is honoured exactly up to 400 m/s and clamped above it**, the platform walking the entity down at exactly 20 m/s² — 1 m/s per 0.05 s frame. Measured: at 440 m/s a run spends 2.0 s off parameter, at 900 m/s **25.0 s, 42% of the run**. A sweep crossing the ceiling plots a result against a number that stopped being true | `open` **by decision** — the envelope is `(0, 400]` m/s on this profile, the committed sweep stays inside it, and the tool does **not** enforce it: the ceiling belongs to a scenario's entity profiles, not to the campaign runner. **R13** | `m5-oq4.md` §3, README limits, PRD R13 |
| F-22 | M5 | **A parameterisation update applied before `start` can land between two publications of the roster burst**, making segment 0 `frozen` with **differing** repeated values — 31 duplicated instants, 12 identical (untouched Blue entities) and **19 differing** (updated raiders). A **third** phenomenon through §5.1's one test, and the only one this project causes. Measured **4 of 35** parameterised runs (11.4%) across two independent batches, against R12's 1 of 27 ordinary ones (3.7%) | `open` — elevated on a sample too small to attribute, and recorded as elevated rather than established; **the exclusion is not relaxed and no retry is added**. A mitigation exists and was deliberately not taken (apply after `start`; then the axis is not an *initial* condition). Strengthens E-4 rather than adding an escalation. **R14** | `m5-oq4.md` §6 and §7, README limits, PRD R12 and R14, E-4 |

---

## C. Defects and imprecisions in `contract/` — EXT-08's to fix, never worked around

`contract/` is read-only here. `PROVENANCE.md` states the rule in its own words: *"If one of them
is wrong or insufficient, that is a defect in EXT-08's contract and it goes back there."*

| # | Found | What | Status | Recorded in |
|---|---|---|---|---|
| F-15 | M3 | **§6.7 says a rotated run's totals are the sum across parts. For `segments` that is false** — a segment cut by a rotation is closed in one part and opened in the next, so the sum double-counts it. Measured: **5 summed for a 2-segment run** | **`fixed` upstream 2026-09-01** — E-3, [issue #1](https://github.com/EgeCankaya/EXT-08/issues/1) closed. EXT-08 narrowed §6.7 rule 2 and §11 to the four counters that sum and stated how to correct `segments`; vendored back at the **fourth pin**. Never worked around: the reader computed what §6.7 said, computed what was true beside it, and named the gap — and now agrees with the specification instead of reporting beside it. **No code changed** | `escalations.md` E-3, `m3-oq6.md`, `m3-capture-reader.md` §6 |
| F-16 | M4 | **§5.1's frozen-clock test is said to detect a reset clock; it detects two phenomena.** A duplicated publication of identical values satisfies the same test, in a segment whose clock did not reset. See F-13 for the measurement | **`fixed` upstream 2026-09-01, and wider than it was raised** — E-4, [issue #2](https://github.com/EgeCankaya/EXT-08/issues/2) closed. §5.1 now states what a positive result establishes and lists **three** shapes — EXT-08 folded in F-22, which was measured after the issue was filed and never added to it. §14 gained the consequence: an emptied self-test is a refusal, not a pass, and retrying is named as the wrong fix. **R12 and R14 are upstream text now.** Never worked around; **no code changed** | `escalations.md` E-4, `m4-determinism.md` §5, `contract/PROVENANCE.md` finding 2 |
| F-17 | M1 | **`PROVENANCE.md` finding 6 omits `N8RO_RELEASE`** from the headless invocation (see F-5). Following it exactly produces a host that refuses every scenario load **while sitting idle rather than failing** | **`fixed` upstream 2026-09-01, on the same day it was raised** - E-6, [issue #4](https://github.com/EgeCankaya/EXT-08/issues/4) closed. Carried as an internal finding from M1 to M7 because until then this project could only say *we had to set it*, which is a report about one machine. **The mentor's confirmation on 2026-09-01 that the variable IS expected in production turned it from suspected into demonstrated**, and that is what made it worth another project's time. EXT-08's R8 block now carries both preconditions and both failure modes; `contract/PROVENANCE.md` finding 6 - **ours**, and the file the issue wrongly named (F-37) - carried the same omission and is corrected at the fourth pin | `CLAUDE.md`, `m1-lifecycle.md` §7(a), `escalations.md` E-6, `contract/PROVENANCE.md` finding 6 |
| F-18 | M4 | **The `contract/` pin string is stale and its content is not.** `PROVENANCE.md` says `78fd4ef`; EXT-08 `main` is at `eb13485`, two commits later, neither touching a vendored artifact. `PROVENANCE.md` justifies pinning the branch head *because* it makes "is this current?" one comparison — and that comparison now answers "no" while the truth is "yes" | **`closed` at the fourth pin, 2026-09-01.** Re-pinned to `ca5118c`, which is EXT-08's `main`, so the one comparison answers "yes" again. It stayed open for three milestones as a note rather than a defect, and the fourth pin is the first time the *content* went stale too — see **F-38** | `m4-determinism.md` §10, `contract/PROVENANCE.md` |
| F-19 | M4 | **`contract/condition-file-schema.md` does not exist under that name in EXT-08 at any commit.** It is a digest written for EXT-17, not a verbatim vendored file, though `PROVENANCE.md`'s table lists it beside two files that are — so a pin check cannot verify it and must not imply it did | **`closed` at M6 by correspondence, and closed OUTRIGHT at the fourth pin.** M6's answer was that it *can* be checked by **correspondence** — the digest's own pin (`eedc228`) resolves, its cited README section exists there, its content is verbatim, and that section is byte-identical at `main` — and what that check found instead is F-32. **Since 2026-09-01 the correspondence check is no longer needed**: E-5's fix created `docs/condition-file-schema.md` upstream, this directory's copy is byte-identical to it, and all three vendored artifacts are now checkable by identity | `m4-determinism.md` §10, `m6-oq5.md` §5, `contract/PROVENANCE.md` "The fourth pin" |
| F-32 | M6 | **The digest is verbatim and it stops one heading early.** EXT-08's `README.md` continues immediately into *"How distance is computed"* (ECEF on WGS-84, straight-line Euclidean, Haversine and Vincenty rejected) and *"Boundary semantics"* (`<=`, edge-inclusive polygons, no antimeridian support). Neither crossed into `contract/`, and both are required by CR-AS-2's *"a verdict's numbers are reproducible"*. From the digest alone `within_m` is a distance with **no stated metric**, so two projects could parse the same file and disagree about the same capture, invisibly | **`fixed` upstream 2026-09-01, and NOT by the fix that was asked for** — E-5, [issue #3](https://github.com/EgeCankaya/EXT-08/issues/3) closed. The ask was "vendor those two sections too"; EXT-08 answered that this fixes one consumer and leaves the next to make the same excerpt, and created `docs/condition-file-schema.md` with all four sections plus a test that fails if it drifts from its README. **Never worked around, and unlike E-3 and E-4 it could not have been**: there was no vendored text to implement, so EXT-17 decided the computation itself and states it with its constants — unchanged, and now agreeing with a vendored sentence rather than with an inference. Closes **F-19** outright | `escalations.md` E-5, `m6-oq5.md` §5, `src/assert/Geodesy.h`, `contract/PROVENANCE.md` |
| F-39 | M7 | **`n8ro-campaign --help` printed `100%%`.** A `printf`-style escape left in a string literal that is emitted with `std::puts`, which does not interpret `%`. It had been in the golden `--help` file since M4 and was copied forward by every regeneration, because the golden is generated from the binary's own output — so the build-time comparison confirmed the two agreed and could never notice that both were wrong | `closed` — one character. Found while regenerating the golden for an unrelated wording change. `n8ro-compare` has the same `%%` in its source and is **correct**, because it uses `std::printf`; that is why this survived a reading. **The lesson is about the check, not the typo**: a golden file generated from the thing it checks catches drift and cannot catch a defect that was there at generation | `tools/n8ro-campaign/main.cpp`, `tools/n8ro-campaign/help.golden.txt` |
| F-38 | M7 | **A fixed escalation makes `contract/` stale, and nothing notices.** All four `contract/` findings were fixed upstream on 2026-09-01, and for about an hour this repository held a vendored copy that its own escalations file described as wrong while the upstream file was right. The three earlier drifts were all *the producer growing* and were caught by re-reading `PROVENANCE.md`; this one is **caused by this project's own success** and has no trigger at all — an issue closing on another repository is not an event anything here watches | `closed` for these four, by re-pinning at `ca5118c` and re-running the suite. **Open as a shape**: the discipline is now written into `PROVENANCE.md` — a `fixed` row in `escalations.md` is not finished until the pin moves and the suite re-runs — but it is a written rule and not a check, and the next one will be caught by somebody remembering | `contract/PROVENANCE.md` "The fourth pin", `escalations.md` |
| — | rev 2 | **`contract/` has drifted twice in two producer releases**, both times an added key, both times non-breaking under §13. The pattern, not a coincidence | `closed` — re-pinned; R4 rated accordingly and tier 2(f) of the conformance suite tests exactly this. **Three times now**, and the third had a different cause — F-38 | `PROVENANCE.md`, PRD R4 |

---

## D. Defects in the client brief [B]

| # | Found | What | Status | Recorded in |
|---|---|---|---|---|
| F-20 | PRD rev 1 | **`EntityStateSample.h` does not exist in release 2.1.328.** [B]'s own surface table cites it as the answer to "What a run publishes". `C:\N8RO\include\n8ro-sim\infrastructure\` holds exactly two headers and a tree-wide search returns nothing | `open` — reported as a defect in [B] rather than built to. EXT-17 is not blocked: consuming the capture artifact walks around it | PRD R7, ADR-2 |

---

## E. Questions out with people — and what is actually blocking each

**This is the section the index existed to make visible.** Two of the four escalations cannot be
delivered by anyone but the DRI, and saying so is the point: a finding that is written down but
not delivered is *recorded*, not *raised*.

| # | Question | To | Delivery | Blocked on |
|---|---|---|---|---|
| E-1 | **OQ-3** - is this the intended production invocation? Six parts, (a)-(f) | Mentor | **CLOSED - all six `answered` 2026-09-01**, across two relays: (b), (d), (e), (f) first, then (a) *yes* and (c) *"pick the one you prefer"* | **Nothing, across all six.** Every answer matched what was already built, so nothing changed - the argument for asking early, not for having waited five milestones. (c)'s *"pick the one you prefer"* is an answer and not a non-answer: **no variant is prescribed**, so `SimEngineHost_SharedMemory` stays and is now explicitly our choice rather than an inference. (b) produced **E-6**, now fixed |
| E-2 | **OQ-2** — is the determinism gate keyed on content or on bytes? | Owner of [B] | **`decided` — DRI, 2026-09-01, content — and `concurred`, mentor, same day, independently. Still never `answered` by [B]'s author** | **The recipient, still, and the concurrence does not change that.** Sent 2026-08-31 via EXT-08's E-1; re-checked at M4, M5 and M6; no reply. Decided from [B]'s own words because schedule became binding; the mentor, asked separately, reached the same answer. **Criterion 2 is [B]'s author's to discharge**, so this is a second opinion and not a ruling. **No behaviour changed** — content was already the default; the reports gained the concurrence and two paired tests that stop it standing in for a ruling |
| E-3 | §6.7's summing rule (F-15) | EXT-08 | **`fixed` 2026-09-01** — [issue #1](https://github.com/EgeCankaya/EXT-08/issues/1) closed, vendored at the fourth pin | Nothing. Never blocked, and nothing changed here when it was fixed |
| E-4 | §5.1's frozen-clock test (F-16) | EXT-08 | **`fixed` 2026-09-01, wider than raised** — [issue #2](https://github.com/EgeCankaya/EXT-08/issues/2) closed, vendored at the fourth pin | Nothing. Never blocked, and it is the one with a measured operational cost — **~1 pair in 14, and worse than that since M6**: F-29 found a fourth shape of the same mechanism that fails the gate and stops the *whole campaign* rather than costing one run. **That cost is unchanged and was not traded away for the fix** — §14 now states it upstream instead of this project carrying it alone |
| E-5 | The vendored condition-file digest stops one heading early (F-32) | EXT-08 | **`fixed` 2026-09-01, the same day it was sent** — [issue #3](https://github.com/EgeCankaya/EXT-08/issues/3) closed, vendored at the fourth pin | Nothing. Never blocked: the geodesy is decided here and stated with its constants. It was the first `contract/` finding that could **not** be handled by implementing what the text says, because there was no text — and it is now the one that produced an upstream *file* rather than an upstream sentence |

### The mentor replied: four of E-1's six parts are ANSWERED, and two are not

**2026-09-01, relayed by the DRI.** Confirmed: **(b)** `N8RO_RELEASE` is expected in production;
**(d)** the degraded terrain configuration is expected and stays; **(e)** a console control event
is the intended shutdown *and* its non-zero exit is expected; **(f)** `C:\N8RO\bin` on `PATH` is a
known second precondition. **Every one confirms what was already built and measured — nothing
changed.**

**(a) and (c) were not covered in the first relay, and were NOT recorded as answered.** They were
held at `decided` on the reading in `docs/m7-oq2-oq3.md` §2 — the rule this table has applied to
[B]'s author for five milestones, applied to a mentor: **a row does not quietly acquire an answer
nobody gave.** One short follow-up closed both, later the same day: **(a) yes**, and **(c) "pick
the one you prefer"**. The rigour cost one question. Rounding up would have cost a permanent claim
that somebody confirmed something they were never asked.

**(c)'s answer is easy to misfile as a non-answer, and it is not one.** *"Pick the one you
prefer"* settles that **no variant is prescribed**. So `SimEngineHost_SharedMemory` stays, on the
reasoning in `m7-oq2-oq3.md` §2.2 — a best-effort transport is one permitted to drop, and [B]
says *"campaign runs are for the closed configuration"* and *"determinism first"* — and it stays
as **this project's own choice, now explicitly delegated**, rather than as an inference about
somebody's unstated intent. What the answer removes is the risk that we chose against an intent
that existed and had not been written down.

**One answer had a consequence.** (b) turned **F-17** from suspected into demonstrated —
`PROVENANCE.md` finding 6 documents this invocation *without* the variable, and following it
exactly gives a host that refuses every 42-entity load while sitting idle rather than failing.
Raised as **E-6**, [EXT-08 issue #4](https://github.com/EgeCankaya/EXT-08/issues/4). It waited
from M1 to M7 on purpose: until the mentor confirmed the variable is expected, this project could
only say *"we had to set it"*, which is a report about one machine rather than a statement about
the contract.

**It was fixed the same day it was raised**, and the citation it was raised under was the wrong
one (F-37) — EXT-08 fixed its own `README.md`, which is where the omission actually was, and this
project corrected `contract/PROVENANCE.md` finding 6 itself at the fourth pin, since that file is
ours.

**And the sweep-legibility metric is now MET, by its own named method.** The mentor reviewed the
sweep report and confirmed it reads. It was reported **unmet** at PRD revs 7 and 8, on the
grounds that the artifact exceeding its target was not a substitute for the method the metric
named — so it is met now on the terms it was written on, rather than on relaxed ones. **F-36's
first half closes; its second half, the recording, does not.**

---

### E-1 and E-2 were DECIDED by the DRI — and neither had been ANSWERED

**Superseding the deferral below, on 2026-09-01.** Schedule became the binding constraint, and
the DRI authorised deciding both from [B]'s own words rather than waiting for recipients who had
not replied. **The reading that decided them is `docs/m7-oq2-oq3.md`.**

**What `decided` means here, said once so nothing downstream has to re-derive it:** a person
entitled to decide it did, on stated evidence, and **the original recipient still has not
spoken**. It is not `answered`. Every place carrying the decision — the escalation rows, the PRD,
the README, `self-test.json`, and every report the tools print — says both halves in the same
sentence, because *"the gate is content"* on its own would hide which of the two it is.

**Neither decision changed any code.** OQ-2's default was already `content`; OQ-3's invocation is
the one measured across roughly a hundred runs since M1. What changed is what this project is
entitled to *say*, and that two open questions stopped being open.

**One thing did NOT get decided, and is not claimed to be.** [B] asks its reader to *"confirm the
invocation with your mentor"*. That is an instruction to ask a person, no reading of [B] can
discharge it, and it did not happen. It is now a **named limit in the README** — a change of
where it is recorded, not a claim that it was done. The same shape applies to the
sweep-legibility metric: **[B]'s own acceptance criterion 3 is met on measured evidence, and the
PRD's internal mentor-review verification method was not executed.** The metric row says exactly
that, rather than being rewritten to a method it could pass.

**What reversing costs, if a reply ever arrives:** `m7-oq2-oq3.md` §5. Briefly — a content ruling
changes one status word; a byte ruling changes a default and no code, and blocks the project at
[B]'s step 4, which would be that ruling's consequence rather than a defect here; a mentor
correcting the invocation costs a few hours of re-measurement and one file.

---

### The deferral this superseded, and what it cost — kept, because the prediction was exact

**Decided 2026-09-01 by the DRI:** complete M6 and M7, then return to the mentor items. The
mentor is not readily accessible and schedule is the binding constraint. **This is a decision,
and it is recorded as one** — the failure mode this table exists to prevent is an item that was
never decided about, not an item that was.

What it costs, stated rather than discovered later:

- **PRD's OQ-3 decision target was M2.** By M7 it will be five milestones past it. That is a
  fact about the schedule, not a new risk.
- **M7 cannot fully close.** Its validation line is *"CR-DOC-1, CR-DOC-2, and every success
  metric"*, and one success metric (`prd.md`, "Sweep legibility") names **mentor review of the
  sweep report** as its method. **M7 must state that metric as unmet rather than claim it**, and
  CR-DOC-2's unexplained-observations section is where OQ-2 and OQ-3 both belong.
- **The exposure is re-measurement, not rework.** Every number this project holds — M2's twenty
  runs, M4's 190 pairs, M5's thirty-five parameterised runs — was taken through the invocation
  E-1 asks about. A different answer would not make them wrong; it would make them measurements
  of a path the client did not intend, and re-running the campaigns is a few hours of machine
  time. The code change stays one file, which is why `EngineControl` was shaped that way.
- **Two things now ride on the unconfirmed answer**, where at M1 there were none: M4 keyed the
  determinism gate to runs produced this way, and M5 added entity updates published into it
  before `start`.

**Revisit at:** the close of M7, together with OQ-2, the sweep report and the twenty-run
campaign — which is a better package to put in front of a reviewer than any single milestone
was, and is the one upside of having waited.

**M7 has now closed, and the package is ready.** What was predicted here happened exactly — and
then, hours later on the same day, the mentor replied and superseded two of these bullets. **They
are kept as written**, because a prediction that came true and was then overtaken is worth more
on the record than one quietly edited to match the outcome. The section above carries the current
state.

What was predicted:

- **The cost landed where this section said it would.** M7's validation line asks for *"every
  success metric"*, and **sweep legibility is reported UNMET** because its named method is mentor
  review and no mentor has reviewed it. The artifact exceeds the target — a non-monotone count
  peaking at 170–190 m/s, three conditions whose verdicts flip at three thresholds — and that is
  not what the metric measures. **F-36**, PRD rev 7, `m7-evidence.md` §4.
- **The 5-minute recording is also outstanding** for the same underlying reason, and is scripted
  rather than substituted for. ~~**This one still stands.**~~ **Superseded 2026-09-01: shot and
  published** — one take, 4:10, to the runbook. It stood for as long as it was true, which is the
  only thing this bullet was ever for.

**Superseded the same day:** the mentor reviewed the sweep report, so the first bullet's metric is
**met by its own named method**, and four of E-1's six parts became **answered**. The recording is
the only one of the two that survives.
- **The package is now a whole project rather than a milestone**: a twenty-run judged campaign,
  a determinism gate that has both passed and refused on real pairs, five unexplained
  observations written up carefully, and three escalations sent upstream. That was the stated
  upside of waiting, and it is the shape it was expected to take.

**Two things ride on the answer that did not at M1, and now a third.** M4 keyed the determinism
gate to runs produced this way; M5 added entity updates published into it before `start`; and M6
judged twenty of those runs and reported eight of them as passing. The exposure is still
re-measurement rather than rework, and `src/control/EngineControl` is still one file.

---

## F. Requirements this project found under-covered in its own work

Not defects in anyone's code — gaps between what a requirement asks for and what had been built,
found by re-reading the requirement against the thing rather than against the plan.

| # | Found | What | Status | Recorded in |
|---|---|---|---|---|
| F-33 | M6 | **CR-REP-4's changed-input half had no implementation and no test.** [B] asks for two diffing questions — *"run the same configuration twice and show that the results are identical; change one input and show exactly where the two runs diverged"* — and only the first was built. `n8ro-compare` would compare any two captures, but it framed every answer as a **gate**, which is wrong in both directions for the second question: a divergence between two configurations is not a failure, and agreement is not a pass | `closed` — `--changed-input`. Same machinery, different framing: no gate line, no pass/fail word, and **agreement is the outcome flagged loudly**, because it means the changed input did not take effect. Found by auditing the brief against the built thing at M6, not by a test failing | `m6-assertions.md` §1, `tests/determinism_test.cpp` |
| F-37 | M7 | **This project cited the wrong file in an escalation it had already sent.** E-6 was filed against EXT-08 naming *`PROVENANCE.md` finding 6* - and `PROVENANCE.md` **is not an EXT-08 file**. It does not exist there under that name at any commit; it is EXT-17's own manifest of what crossed the boundary, and its "finding 6" is this project's summary of what it learned upstream. Asking EXT-08 to edit it is asking the wrong party | `closed` - a **correction comment** was posted on [issue #4](https://github.com/EgeCankaya/EXT-08/issues/4) rather than the issue being quietly edited, because the original was sent and read, and *"we told them X"* versus *"we told them something adjacent to X"* is the distinction these records exist to keep. **The substance stands and is now better cited**: EXT-08's own `README.md` documents the R8 headless invocation without `N8RO_RELEASE`, which is where EXT-17's digest inherited the omission from. **This is F-19's shape a second time** - a file in `contract/` that reads as vendored and is a digest - and two instances is a pattern rather than a coincidence | `escalations.md` E-6, [issue #4 comment](https://github.com/EgeCankaya/EXT-08/issues/4) |
| F-36 | M7 | **Two of M7's own validation items could not be produced by this project, and a project's last milestone is where that is most likely to be quietly rounded up.** Its validation line is *"CR-DOC-1, CR-DOC-2, and every success metric"*. The **5-minute recording** needs a person, and the **sweep-legibility** metric names *mentor review of the sweep report* as its measurement method | **`closed`, both halves, and neither by relaxing its terms.** The metric was reported UNMET at PRD revs 7 and 8 - the artifact exceeding its target was explicitly NOT accepted as a substitute for the named method - and the **mentor reviewed the sweep report on 2026-09-01 and confirmed it reads**, so it is now **met on the terms it was written on** rather than on relaxed ones. The recording was **delivered on 2026-09-01** — [one take, 4:10, published](https://drive.google.com/drive/folders/16cR82ynxrcmrzJofwHKdpReNlPj1C--M?usp=sharing), shot to the runbook, with the campaign it launches on camera committed at `campaigns/demo/`. **Both halves are now closed on their own terms**: the metric by its own named method, the recording by being made rather than by being described. It ran **4:10 against "5-minute"**, and that is stated rather than rounded up | `m7-evidence.md` §4, PRD revs 7-9 and 12, `recording-script.md` |
| F-34 | M6 | **A committed campaign's captures cannot be committed.** [B]'s third deliverable is *"a real campaign — its configuration, its captured runs and its report, committed as an example"*, and twenty runs of Atacama Air Defense are about **480 MB** of JSON Lines. `.gitignore` excluded captures from the repository's first commit, for that reason | `open` **by decision** — the configuration, the whole report and a **MANIFEST** of each capture's size, SHA-256 and read-back counts are committed; the raw captures are not. A re-run does not reproduce them byte-for-byte in any case, which is this project's central measurement rather than a limitation of the decision. Stated in the manifest rather than left for a reviewer to notice | `campaigns/m6-campaign/MANIFEST.md`, `decisions-m6-m7.md` B6, `.gitignore` |

---

## How to keep this true

A milestone that finds something adds a row here **and** records it wherever it belongs — this
file is the index, not the record. The reverse check is the useful one: at the close of each
milestone, every finding in that milestone's document should have a row, and every `drafted`
escalation should be re-examined for whether it can be delivered yet.
