# The clean-room pair test

**What this is.** EXT-08 and EXT-17 cloned cold from GitHub into a scratch directory, and both
READMEs followed **literally**, from zero to a recorded-and-judged run — **in both orders**, because
the order decides what you can see. Run on 2026-09-01 against EXT-17 `9f6fed0` and EXT-08 `bda3904`.

**Why it is a document rather than a badge.** Everything below is what happened, including the six
things that were wrong. A green result would have been worth less: this pass found **seven
findings, F-46 to F-52**, and two of them block an evaluator. Its predecessor — the tier-0/1
handover pass that produced F-42 to F-45 — established the lesson this one repeats: *the passes
that read the repository cannot see what ships*. This one adds a second: **and a pass that reads
one repository cannot see the seam between two.**

---

## 1. What was run

| | |
|---|---|
| Scratch root | `C:\cleanroom\orderA\` and `C:\cleanroom\orderB\`, two independent trees |
| Source | `git clone https://github.com/EgeCankaya/EXT-{08,17}.git` — the remotes, not the working copies |
| Git config | `core.autocrlf=true` at **system** level, Git for Windows' own default. This is the setting F-42 was about, left as found |
| Toolchain | VS 2026 Insiders 18.x (cl **19.51**) discovered through `vswhere`; VS 2022 17.14 also present |
| Install | `C:\N8RO` 2.1.328. The pair test is about the two repositories, not about the SDK boundary — that is what `.github/workflows/zero-install-tier.yml` covers, on a machine nobody here owns |

**Order A** — EXT-08 first, then EXT-17. This is the order the two projects were built in, and the
order a reader who already has a recorder walks.
**Order B** — EXT-17 first, with **no EXT-08 anywhere on disk**, then going to get it. This is the
order an evaluator handed "the campaign runner" walks, and it is the one that found F-51.

---

## 2. Order A — EXT-08 first

### 2.1 EXT-08, its README's Build section verbatim

```cmd
call C:\N8RO\setup.cmd
call C:\N8RO\dev\setup-dev.cmd
cd /d C:\cleanroom\orderA\EXT-08
msbuild n8ro-bridge.sln /p:Configuration=Release /p:Platform=x64
```

**Build succeeded. 0 warnings, 0 errors, 4.70 s**, output at `build\x64\Release\n8ro-bridge.exe`.
Nothing to report — EXT-08's own closing audit had already walked this and fixed what it found.

### 2.2 EXT-17, its README's Building section verbatim

All five commands the README lists, each in a fresh `cmd`, with nothing from `C:\N8RO\setup.cmd`
set:

| Script | Exit | |
|---|---|---|
| `tools\n8ro-campaign\build.cmd` | 0 | …and printed an error on the way, see **F-46** |
| `tools\n8ro-capture\build.cmd` | 0 | links nothing, `--help` matches its golden |
| `tools\n8ro-compare\build.cmd` | 0 | links nothing, four CR-DET-2 hazard searches clean |
| `tools\n8ro-judge\build.cmd` | 0 | links nothing, cannot reach a host or a bus |
| `tests\build.cmd` | 0 | **469 mandatory checks, 0 failures**, count matched the golden |

**The four fixes from the previous pass all held on a cold clone**, which is the first thing this
pass had to establish and the reason to state it rather than assume it:

- **F-42** — `contract/` checked out with **0 CR bytes** in all five files, and EXT-08's sample
  likewise. The `.gitattributes` pins work against Git for Windows' default.
- **F-43** — all eight build scripts found the toolchain through `vswhere`. No hard-coded path was
  reached on a machine that has two Visual Studios.
- **F-44** — **469**, exactly, and the suite printed `tier 4 was skipped` on its own line. The
  count is now a property of the suite rather than of the machine.
- **F-45** — both spike scripts built and linked.

### 2.3 Zero to a recorded-and-judged run

```cmd
set PATH=C:\N8RO\bin;%PATH%
build\n8ro-campaign\n8ro-campaign.exe run-once ^
    --out-dir campaigns\cleanroom-A ^
    --recorder C:\cleanroom\orderA\EXT-08\build\x64\Release\n8ro-bridge.exe ^
    --scenario "Atacama Air Defense" ^
    --conditions examples\atacama-raid.conditions.json ^
    --frames 1200
```

**It worked, first time.** Host started, scenario loaded, 1200 frames, stop, capture read back
**conformant — 50 444 samples over 2 segment keys**, seven verdicts, outcome `fail`, exit 1.
Exit 1 is the documented meaning: three conditions were violated. The capture the pair produced
is **producer 0.9.0**, carrying `sample_form: published`, `limits` and `part` — which is worth
noting, because at this moment nothing in EXT-17's own suite could read such a file on this
machine's behalf (**F-49**).

Then the identity check, which is what CR-CAP-1 rests on:

```cmd
build\n8ro-judge\n8ro-judge.exe campaign campaigns\cleanroom-A ^
    --conditions examples\atacama-raid.conditions.json --verify verdicts.jsonl
```

> `verify: 7 verdict(s) byte-identical to the live run's verdicts.jsonl`

### 2.4 The gate and the sweep, through the cold-cloned pair

`repeat --campaign examples\atacama-raid-speed.json --conditions … --frames 1200` — two self-test
runs and seven swept values, about 11 minutes.

**Gate: PASS on the content basis.** 50 426 samples compared, **0 differing**, coverage
**99.9287 %** against a 99 % floor; 51 samples present only in run 000 and 36 only in 001, counted
and reported and correctly not treated as differences. The byte comparison ran alongside and
**DIFFER**ed, as designed — first difference at byte 7 384 282 — with headers byte-identical and
`model_path` excluded but identical anyway.

The sweep produced a monotone trend and a verdict table that changes with the axis:

```
value  run   outcome                   adds     keys   samples
11     000   fail                        48       48     50643  ##
27.5   001   fail                        48       48     50501  ##
55     002   fail                        47       47     50472  .
82.5   003   fail                        54       54     49835  ##################
110    004   fail                        61       61     49260  #####################################
165    005   infrastructure_error         -        -     48836  (no bar - this run did not complete)
220    006   pass                        62       62     48842  ########################################
```

**Run 005 is R14 met live on a cold clone**, and it is worth keeping rather than re-rolling:
`no segment classified running, so nothing in this capture can be judged`. That is 1 of 7
parameterised runs — **14.3 %**, against the 11.4 % (4 of 35) measured across two earlier
batches. A third independent batch, consistent with a rate the README already publishes as
supporting *a direction and not a number*. The gate was not weakened and no retry was added.

---

## 3. Order B — EXT-17 first, with no EXT-08 on disk

EXT-17 cloned alone. All five build scripts: **exit 0, 469 checks, 0 failures** — so nothing in
EXT-17's build depends on EXT-08 being present, which is the boundary claim and it holds.

Then the README's first run command, as a reader with only this repository would meet it:

| What was tried | What happened |
|---|---|
| `run-once` with no `--recorder` | `n8ro-campaign: --recorder <path> is required unless --no-recorder`, **exit 2, no run attempted**. Correct |
| `--recorder ..\EXT-08\build\x64\Release\n8ro-bridge.exe` (the path the README implies, pointing at nothing) | `FAIL recorder_start: CreateProcess failed … win32 error 2` → `infrastructure_error`, named stage, exit 1. Correct, and F-2's fix working |
| `--no-recorder --frames 60` | ran to frame 60, `completed (unjudged)`, exit 0. Correct |
| `n8ro-judge check`, `n8ro-capture read` | both exit 0 with `C:\N8RO\bin` off `PATH`. The zero-install claim holds for these two |
| `n8ro-campaign report` with `C:\N8RO\bin` off `PATH` | **exit `0xC0000135`, no output, no diagnostic** — and three documents said it needed no install. **F-52** |

**And then the reader is stuck, which is F-51.** Every message above is clear about *what* is
missing and none of them says *where to get it*. `README.md` has a section titled "The fourth
binary is not in this repository", a four-row table about it, and the words "EXT-08's
`n8ro-bridge.exe`, built from that repository" — **and no repository, no URL, no clone command**.
The only GitHub link in EXT-17 is an issue reference buried in `docs/escalations.md` and the PRD's
revision history. Order A cannot see this: if you build EXT-08 first you already have the thing
you would have gone looking for.

### The recovery half

EXT-08 cloned as the sibling the README's path implies, built to its own README, and the same
`run-once` re-run with `--recorder ..\EXT-08\build\x64\Release\n8ro-bridge.exe`. **It worked.**
Capture conformant, 50 374 samples, seven verdicts, `fail`, exit 1.

### The two orders agreed

Two independent clone trees, two separately compiled `n8ro-bridge.exe` binaries (**different
SHA-256** — MSVC does not produce a reproducible build here), two runs, two captures of different
length (50 444 and 50 374 samples).

**All seven verdicts are identical once the file-line pointers are removed.** Same entities, same
occupancies, same `sim_time_s`, and the same geometry to the digit — `closest_approach_m` 6283.6219,
`closest_to_boundary_m` 2400.5825 and 2419.0104 in both. The `verdicts.jsonl` files differ **only**
in `line`, which points into captures of different length and is *supposed* to differ. That is the
content-versus-bytes distinction the whole determinism gate is built on, showing up unprompted
across two clean rooms.

---

## 4. What was found

Seven findings, `docs/findings.md` F-46 to F-52. **Two block an evaluator, and both are in the
seam rather than in either repository.**

| # | | Where it hid |
|---|---|---|
| **F-51** | `README.md` never says where EXT-08 is | Order A cannot see it, by construction |
| **F-49** | On a fresh clone nothing read a real 0.9.0 capture, while two documents named 0.9.0 as pinned | The suite printed its own skip line every time |
| **F-47** | `contract/` was stale in **two** artifacts, by two routes F-38's rule does not cover | An upstream commit is not an event anything here watches |
| **F-50** | §14's exclusion list is three fields and the byte comparison masks one | Open by decision — unreachable for any capture this project produces |
| **F-52** | `report` documented in three places as needing no install; it needs one | The sentence is correct about two of the three things it names |
| **F-46** | Two committed files carry control-character corruption; one makes the flagship build execute its own prose | Exit code checked, output not read |
| **F-48** | The pass itself | — |

### The one that is worth re-reading in a year

**F-47.** `contract/` went stale in the specification *and* in the sample, and **neither route is
one F-38 covers**:

- EXT-08 resolved E-7, E-8 and E-9 — escalations raised **against** EXT-17, which **EXT-17's own
  author settled** — and wrote the answers into the frozen specification. This project had ruled on
  three questions about the format it vendors and was then reading a copy that predated its own
  rulings. Nothing here recorded that the questions had ever been asked: `docs/escalations.md` ran
  E-1 to E-6 and stopped.
- EXT-08 regenerated its sample capture, because its own audit found it shipped a 0.5.0 file against
  a 0.9.0 build. No escalation, no issue, no version bump — nothing to notice it by except running
  the pin check.

**F-38 said a `fixed` escalation makes `contract/` stale and nothing notices. The rule is
narrower than the problem: an escalation this project ANSWERS does it too**, because the answer
lands upstream, in the file this directory copies. That is now written into `PROVENANCE.md`, and
`escalations.md` records inbound escalations as well as outbound ones.

The discipline that made `contract/` trustworthy — raise it, never work around it — is what made
the directory go stale. Twice.

---

## 5. The fifth pin, and what moved

`contract/` re-pinned from EXT-08 `ca5118c` to **`bda3904`**. The pin check as `PROVENANCE.md`
documents it, before and after:

| Artifact | At the fourth pin | EXT-08 `main` | Now |
|---|---|---|---|
| `capture-format-v1.md` | `a6974af…` | `86ecf8e…` | `86ecf8e…` — identical |
| `condition-file-schema.md` | `9d65ed0…` | `9d65ed0…` | unchanged |
| the sample capture | `a089d23…` | `a848759…` | **both vendored**, see below |

**The 0.5.0 sample was kept and the 0.9.0 one added beside it, rather than replacing it.** Three
of tier 1's checks assert that `sample_form`, `limits` and `part` are reported **absent** rather
than defaulted — §6.3a and §6.6's rule that an absent key means *unknown* — and a file carrying all
three cannot exercise that. Replacing would have traded one kind of coverage for another. The pair
is exactly the compatibility question §13 exists for: one reader, one format version, one file that
predates three keys and one that carries them.

### What moved in the suite

`tests/capture_reader_test.cpp` gains **tier 1b**, fifteen mandatory checks over the 0.9.0 fixture.
Golden **469 → 484**.

```
tier 1b - the vendored 0.9.0 fixture (the fifth pin; tier 1 is 0.5.0)
  ok   the 0.9.0 fixture parses completely and conformantly
  ok   format_version is n8ro-capture/1 - a producer release is NOT a version change
  ok   producer is 0.9.0
  ok   11150 lines read
  ok   our tally agrees with trailer.counts
  ok   10915 samples, 132 adds, 90 removes, 7 verdicts, 2 segments
  ok   sample_form is present and reads "published"
  ok   limits is present and reads as unbounded-with-stop, not as absent
  ok   part is present and reads 0
  ok   an unrotated capture carries neither continues_from nor continued_in
  ok   two segments, both keyed on (part, segment)
  ok   segment 0 is running by the format's exact test
  ok   segment 1 is not running, and is excluded either way
  ok   segment 0's extent comes from its samples, not its boundary records
  ok   the same twelve fields in the same declaration order as the 0.5.0 fixture
```

**Every one passed on the first execution**, which is the useful result: the reader was already
correct about a producer release it had never been given a committed file from. That is the
same outcome the fourth pin had, and for the same reason.

### And one thing that did NOT confirm existing behaviour

**F-50.** §14's host-dependent field list widened from `platform.model_path` alone to three, and
`src/compare/` masks one. Every previous pin's whole story was *nothing here changed*; this one has
an exception and it is stated rather than absorbed. It is **open by decision**, with its bound: it
cannot reach any capture this project produces, because `--on-size-limit` defaults to `stop` and an
unrotated capture omits both keys — which tier 1b now asserts. The reasons for not fixing it in the
same pass are in `maskModelPath`'s comment: excluding `trailer.continued_in` means splitting the
byte walk and restating `firstDifferingOffset`, and ADR-1's position is that this comparison masks
as close to nothing as the format allows, so widening it is a decision made by measuring — and
there is no rotated pair here to measure.

---

## 6. Verification after every change

| Check | Result |
|---|---|
| All **eight** build scripts in the tree | exit 0 |
| `tests\build.cmd` | **490 checks, 0 failures**; 484 mandatory, matches the golden |
| `json_writer_test` / `capture_reader_test` / `determinism_test` / `parameter_test` / `assertion_test` | 72 / 93 / 105 / 126 / 166, 0 failures |
| Repo-wide control-character sweep (outside TAB/LF/CR) | none, in either repository |
| `n8ro-campaign` build output | no longer prints `'8ro-judge…' is not recognized` |
| Pin check, all three artifacts vs EXT-08 `bda3904` | byte-identical |
| `contract/` CR bytes on a fresh clone | 0 |
| `n8ro-judge … --verify` on the pair-test run | 7 verdicts byte-identical |
| Order A vs order B verdicts | identical modulo file-line pointers |

**Two help goldens were regenerated deliberately** — `n8ro-compare` and `n8ro-campaign` — because
the help text changed to say what §14 now says and what `report` actually needs. The build failed
on both before they were updated, which is the mechanism working.

---

## 7. What this test is not

It is the **shipping gate**, not the definition of done, and it is worth saying where its limits are
rather than letting a clean second half stand in for more than it covers.

- It did **not** find any defect in what either program computes. Every finding is packaging,
  documentation, or a vendored copy going stale. The engineering held up; it always has — that was
  EXT-08's closing conclusion too, and this pass reaches it independently from the other side.
- It ran on a machine that **has** `C:\N8RO`. The zero-install claim is covered by the CI job on
  `windows-latest`, which asserts the runner has no install and fails otherwise, and that remains
  the only claim here that is not self-certified.
- **It was run once, in each order.** F-29 says the gate can refuse a whole campaign about 1 pair
  in 14; it did not refuse here, and one pass is not evidence that it will not.
- It cannot find what neither README claims. F-51 was findable only because the README *did* claim
  a fourth binary was needed and then stopped short of saying where — a document that had said
  nothing at all would have failed a reader in the same place with nothing to flag.
