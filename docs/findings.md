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
| F-15 | M3 | **§6.7 says a rotated run's totals are the sum across parts. For `segments` that is false** — a segment cut by a rotation is closed in one part and opened in the next, so the sum double-counts it. Measured: **5 summed for a 2-segment run** | **`raised`** — E-3, [EXT-08 issue](https://github.com/EgeCankaya/EXT-08/issues/1). Not worked around: the reader computes what §6.7 says, computes what is true beside it, and names the gap | `escalations.md` E-3, `m3-oq6.md`, `m3-capture-reader.md` §6 |
| F-16 | M4 | **§5.1's frozen-clock test is said to detect a reset clock; it detects two phenomena.** A duplicated publication of identical values satisfies the same test, in a segment whose clock did not reset. See F-13 for the measurement | **`raised`** — E-4, [EXT-08 issue](https://github.com/EgeCankaya/EXT-08/issues/2). Not worked around: the test is implemented exactly as written and both shapes excluded; what was added is that the refusal names which shape it found | `escalations.md` E-4, `m4-determinism.md` §5 |
| F-17 | M1 | **`PROVENANCE.md` finding 6 omits `N8RO_RELEASE`** from the headless invocation (see F-5). Following it exactly produces a host that refuses every scenario load while sitting idle | `open` — the contract's to correct. Folded into E-1(b) rather than filed separately, since the mentor's answer settles both | `CLAUDE.md`, `m1-lifecycle.md` §7(a) |
| F-18 | M4 | **The `contract/` pin string is stale and its content is not.** `PROVENANCE.md` says `78fd4ef`; EXT-08 `main` is at `eb13485`, two commits later, neither touching a vendored artifact. `PROVENANCE.md` justifies pinning the branch head *because* it makes "is this current?" one comparison — and that comparison now answers "no" while the truth is "yes" | `open` — a note, not a defect. Nothing a reader does depends on it | `m4-determinism.md` §10 |
| F-19 | M4 | **`contract/condition-file-schema.md` does not exist under that name in EXT-08 at any commit.** It is a digest written for EXT-17, not a verbatim vendored file, though `PROVENANCE.md`'s table lists it beside two files that are — so a pin check cannot verify it and must not imply it did | `open` — reference-only, and M6's concern | `m4-determinism.md` §10 |
| — | rev 2 | **`contract/` has drifted twice in two producer releases**, both times an added key, both times non-breaking under §13. The pattern, not a coincidence | `closed` — re-pinned; R4 rated accordingly and tier 2(f) of the conformance suite tests exactly this | `PROVENANCE.md`, PRD R4 |

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
| E-1 | **OQ-3** — is this the intended production invocation of the headless host? Six parts, (a)–(f) | Mentor | **`drafted`, not sent — re-examined at M5 and still not delivered** | **The DRI.** There is no channel to a mentor from here. Does not block: the invocation is measured working, and the whole control path is one file if the answer changes it. **Five milestones now**, and M5 has added runs that publish entity updates over that same invocation |
| E-2 | **OQ-2** — is the determinism gate keyed on content or on bytes? | Owner of [B] | **`raised`** — sent 2026-08-31 via EXT-08's E-1; re-checked at M4 and again at M5, **no reply** | **The recipient.** Does not block: M4 shipped both readings as selectable gates, so a ruling changes a default and no code |
| E-3 | §6.7's summing rule (F-15) | EXT-08 | **`raised`** — [issue #1](https://github.com/EgeCankaya/EXT-08/issues/1) | Nothing. Does not block |
| E-4 | §5.1's frozen-clock test (F-16) | EXT-08 | **`raised`** — [issue #2](https://github.com/EgeCankaya/EXT-08/issues/2) | Nothing. Does not block, and it is the one with a measured operational cost (~1 pair in 14) |

**E-1 is the one to act on, and it is now the oldest thing in this file.** It has been drafted
since M1 and has grown twice since. It is the only finding here whose delivery is blocked on a
person rather than on a reply. M4 keyed a determinism gate to runs produced by the invocation it
asks about — the stated reason for wanting the answer before M4, in E-1's own words — and **M5
has now added entity updates published into that same invocation before `start`**, which is a
second thing riding on an unconfirmed answer. Nothing is blocked by it and that is precisely why
it keeps not being sent.

---

## How to keep this true

A milestone that finds something adds a row here **and** records it wherever it belongs — this
file is the index, not the record. The reverse check is the useful one: at the close of each
milestone, every finding in that milestone's document should have a row, and every `drafted`
escalation should be re-examined for whether it can be delivered yet.
