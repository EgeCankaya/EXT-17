# OQ-5 — adopt EXT-08's condition-file schema, or supersede it?

**Status: DECIDED at M6.** **Adopt the declaration shape verbatim; supersede three rules around
it; record two limits.** The PRD's own resolution rule is *"adopt unless a required condition
cannot be expressed, and record which in the README"* — so this was decided by writing M6's
conditions in the vendored shape and evaluating them against the committed sweep, not by reading
the schema and forming a view.

**Decision, in four parts**

> 1. **The declaration shape is adopted verbatim.** Every condition M6 requires — all three
>    kinds, both never-met cases, and a condition that genuinely flips across the committed
>    sweep — is expressible in `contract/condition-file-schema.md`'s shape with no key added and
>    no key repurposed. Files written for EXT-08's referee parse here.
> 2. **Three rules around it are superseded, each for a reason this project has measured.**
>    The unknown-key rule is inverted (F-23). The verdict becomes three-valued (CR-AS-4). A
>    teardown removal is excluded from `terminal_state` (measured: 267 of 385 removals across
>    the sweep are `scenario_unload` at `sim_time_s` 0).
> 3. **Two limits are recorded in the README rather than designed around.** There is no entity
>    pattern, so M5's cleanest binary flip — *"no CIWS gun engaged"* — is **not expressible**;
>    two other conditions flip across the same sweep and are, so nothing required is lost.
>    And `terminal_state`'s `field`+`equals` form can never soundly report `not_met`.
> 4. **The digest is faithful but truncated, and that is a defect in `contract/`.** It carries
>    the declaration and stops immediately before the sections that say how a distance is
>    computed and how a boundary is decided — the two things CR-AS-2's reproducibility criterion
>    needs. Raised as **E-5**. Not worked around: EXT-17 decides the computation itself, on the
>    record, and the decision is stated here rather than inherited silently.

> Every number in this document was measured here, over the seven committed captures of
> `campaigns/m5-sweep/`, by evaluating the conditions below against them.

---

## 1. Why this needed trying rather than reading

OQ-5 has been open since PRD rev 1 with a good summary of the trade — adopting buys a documented
shape and makes EXT-08's live verdicts comparable with EXT-17's re-judgements; superseding may be
necessary if the campaign needs cross-run conditions. Both halves are arguments. Neither is a
test, and the PRD says so in the resolution rule it wrote for itself: **adopt unless a required
condition cannot be expressed.** That is a statement about what happens when you try.

So M6 tried. Seven conditions were written in the vendored shape — the five from the vendored
example, plus the two M6 additionally requires — and every one was evaluated against all seven
committed captures. What that found is not what reading the schema would have suggested: **the
shape holds completely, and what fails is the material around it.**

**One thing OQ-5 cannot be answered as worded.** It asks whether to adopt *"EXT-08's
condition-file schema"*. What is vendored is not that schema; it is a digest written for EXT-17
(**F-19** — no file of that name exists in EXT-08 at any commit). The answerable question is
whether to adopt *the shape recorded in `contract/condition-file-schema.md`*, and that is the
question answered here. The distinction is not pedantic — §5 is entirely about material that is
in the schema and not in the digest.

---

## 2. The attempt: M6's conditions, written in the vendored shape

M6 requires conditions satisfying four separate criteria: CR-AS-3's three kinds with at least one
never-met condition of each; CR-PAR-2's third criterion, a condition that **changes outcome**
across the committed sweep; CR-AS-4's `indeterminate`, demonstrated rather than described; and
CR-AS-2's reproducible numbers. Seven conditions cover all four:

| | id | kind | vendored shape | requirement it carries |
|---|---|---|---|---|
| C1 | `raider-reaches-airfield` | `proximity` | `entities:[RedUAV_N_01, BlueBase_Airfield]`, `within_m: 3000` | **CR-PAR-2's flip** |
| C2 | `raider-enters-base-circle` | `area` / circle | `centre:[-23.49849,-68.25173,7.5]`, `radius_m: 3000` | CR-AS-3 circle |
| C3 | `raider-crosses-corridor` | `area` / polygon | four `[lat,lon]` vertices | **a second flip, at a different value** |
| C4 | `raid-leader-destroyed` | `terminal_state` | `entity: RedUAV_N_01`, `removal_reason: destroyed` | terminal-state never-met |
| C5 | `airfield-operational` | `terminal_state` | `field: phase`, `equals: operational` | the `field`+`equals` form, met |
| C6 | `command-centre-destroyed` | `terminal_state` | `entity: BlueBase_CommandCenter`, `removal_reason: destroyed` | [B]'s *"should not have"* question |
| C7 | `raid-leader-degraded` | `terminal_state` | `field: health`, `equals: degraded` | **CR-AS-4's `indeterminate`** |

**All seven parse in the vendored shape with nothing added.** C1–C6 are the vendored example's own
conditions or trivial re-pointings of them; C7 is the `field`+`equals` form aimed at a value the
entity never takes. No key was invented, no key was given a second meaning, and no kind was added.

### What they evaluate to, across the committed sweep

Measured over `campaigns/m5-sweep/runs/00{0..6}`, segment 0:

| `red_raid_speed_ms` | 11 | 27.5 | 55 | 82.5 | 110 | 165 | 220 |
|---|---|---|---|---|---|---|---|
| **C1** proximity ≤ 3000 m | not_met | not_met | not_met | not_met | not_met | **met** | **met** |
| &nbsp;&nbsp;closest approach, m | 8693.17 | 7773.13 | 6283.62 | 4892.45 | 3711.87 | 2934.45 | 2934.45 |
| **C3** corridor polygon | not_met | not_met | not_met | not_met | **met** | **met** | **met** |
| &nbsp;&nbsp;first inside, `sim_time_s` | — | — | — | — | 51.95 | 34.65 | 26.00 |
| **C4** leader destroyed | not_met | not_met | not_met | not_met | not_met | not_met | not_met |
| **C5** airfield operational | met | met | met | met | met | met | met |
| **C6** command centre destroyed | not_met | not_met | not_met | not_met | not_met | not_met | not_met |
| **C7** leader degraded | *ind.* | *ind.* | *ind.* | *ind.* | *ind.* | *ind.* | *ind.* |

**CR-PAR-2's third criterion is finished, and by two conditions rather than one.** C1 flips
between 110 and 165 m/s; C3 flips between 82.5 and 110. Two kinds, two thresholds, both
monotone in the parameter, both with a mechanism — the raid crosses 8.6 km to the defended
cluster inside a fixed 60 s window, so a faster raid gets further in. That is a *trend* in the
sense CR-PAR-2 and [B]'s acceptance criterion 3 ask for, and it is now a trend in **verdicts**
rather than in counts read off a capture, which is exactly the half M5 recorded as unmet.

**C2 is redundant with C1 as the vendored example writes it** — the circle's centre is the
airfield's own position and the airfield never moves, so the two conditions agree in all seven
runs to the centimetre. That is a property of the example, not of the kinds. The committed file
gives C2 a different centre and radius so the circle kind is exercised distinctly; recorded here
because copying the vendored example verbatim would have shipped a condition that proves nothing
the one above it did not.

---

## 3. What broke — and none of it is the shape

### 3.1 The unknown-key rule is inverted, and M5 already paid for finding out why

The vendored digest: *"Any key the loader does not recognise is ignored, which is what lets a
`_comment` live in the file."*

That is `contract/capture-format-v1.md` §13's rule, correctly applied to a *capture* — a producer
adds keys and an old reader must survive them, which is why `format_version` has held across three
producer releases. **A condition file is not a capture. A person writes it**, and M5 measured what
that rule does to a human-authored file: **F-23**, a campaign file silently accepted `"value"` for
`"values"` and resolved a duplicated key first-wins, so a sweep that did not happen looked like a
sweep that did. The condition-file equivalent is `"within_meters"` for `"within_m"` — a proximity
condition with no threshold, in a campaign that then reports twenty confident passes.

CR-AS-1 requires a named parse error for exactly this. **Superseded, on the same terms M5 set for
campaign files:** an unknown key and a duplicated key are each refused by name, and a key
beginning with `_` is a comment. `_comment` therefore still lives in the file, which is the only
thing the vendored rule was buying.

This inverts a `contract/` rule for the second time and for the same reason. It is not a defect
in `contract/` — the rule is right for the file it was written about.

### 3.2 The verdict is two-valued, and CR-AS-4 needs three

The vendored verdict semantics are exact and they are two-valued: *"At the first moment it is
satisfied, or an explicit `met: false` at end of run."* And the digest is emphatic about the
second half: *"The not-met verdict is the load-bearing half."*

It is, and this project agrees — CR-AS-2 adopts it word for word. But **that `met: false` is
derived from no record saying otherwise**, which is precisely the reading ADR-6 and CR-AS-4
forbid. `command-centre-is-destroyed NOT MET` is a conclusion drawn from absence, on a stream
whose loss has been measured with every platform counter reading zero.

This is **not a defect in EXT-08's schema.** EXT-08 referees a live run it is itself ending, and
CR-AS-4 is this project's originated rule, not a shared one. It does mean the verdict half cannot
be adopted as written. **Superseded: three-valued** — `met`, `not_met`, `indeterminate` — with the
cardinality rule kept exactly (one verdict per condition per run, and silence is never a verdict).

Which of `not_met` and `indeterminate` applies is not a matter of taste; §4 measures it.

### 3.3 `removal_reason` matched verbatim admits the teardown, and the teardown is every entity

The digest says only that `removal_reason` is *"matched verbatim against `entity_remove.reason`.
The platform's vocabulary is open, so a supplier-specific reason this build has never seen still
matches."* Measured across the seven committed captures:

| `entity_remove.reason` | count | when |
|---|---:|---|
| `scenario_unload` | **267** | `sim_time_s` **0**, every surviving entity, at teardown |
| `expended` | 74 | in-run, a weapon reaching its end |
| `destroyed` | 44 | in-run, a kill |

**Two thirds of every removal in a capture is the harness stopping the run.** A `terminal_state`
condition on `scenario_unload` is met in every run for every entity, and its deciding
`sim_time_s` is `0` — which under CR-REP-2 is a coordinate that sends the analyst to the wrong
end of the run. [B]'s question is *"did anything reach a terminal state it **should not**
have"*; being unloaded at the end of the run is not that.

**Superseded:** `scenario_unload` is excluded from `terminal_state`, and naming it in a condition
file is a named parse error rather than a condition that is trivially met. The exclusion is
named in the verdict when it applies.

Two spellings are worth keeping straight, because a `grep` over a capture conflates them and did
so once while this was being measured: `scenario_unload` is an **`entity_remove.reason`**;
`scenario_unloaded` is a **`segment_close.reason`**; `host_lost` is a **`trailer.end_reason`**.
Three vocabularies, three record types, and only the first is what `removal_reason` matches.

### 3.4 There is no entity pattern, so M5's cleanest flip is not expressible

M5's headline result was that *"no CIWS gun engaged"* is true at 11/27.5/55 m/s and false at 82.5
and above — a clean binary flip with a mechanism. **It cannot be written as a condition.** The gun
rounds are entities named `BlueGun_East_01_wpn_44749_4`; the numeric parts are generated and
differ every run. The vendored schema names entities, and so does this one.

Under the PRD's resolution rule this is recorded, not designed around: C1 and C3 flip across the
same sweep, are expressible, and satisfy CR-PAR-2's third criterion, so **no required condition
is lost**. It goes in the README's limits section.

Worth recording for whoever revisits it: **M5's reason for refusing a glob does not apply here.**
That refusal was about the *parameterisation* axis, where resolving a pattern would mean
subscribing the control path to `sim/entity/state` and perturbing the publication schedule the
determinism gate measures. A condition is evaluated over a stored capture, with no host and no
subscription — the objection is simply absent. A pattern stays available to a future revision on
its own merits, and is declined here because it is not needed and because CR-AS-3 closes the
vocabulary deliberately.

---

## 4. CR-AS-4, decided per form rather than per kind — and it is not a shrug

CR-AS-4 asks that *"every condition kind is classified as absence-dependent or not"*. Classifying
per **kind** turns out to be the wrong granularity: within `terminal_state` the two forms differ
completely, and the classification that matters is per (kind, form, polarity).

A `met` verdict is always sound — it is computed from records that are present. The whole
question is what a **not-met** verdict is entitled to say.

| form | not-met is decidable when | mechanism |
|---|---|---|
| `proximity` | the closest observed approach clears the threshold by more than the largest possible unobserved excursion, **and** both tracks span the segment | continuity over present samples |
| `area` | same, measured to the region boundary | continuity over present samples |
| `terminal_state` + `removal_reason` | the named `(entity, occupancy)` carries a sample at the segment's last sampled instant | **`contract/capture-format-v1.md` §8.1**, normative |
| `terminal_state` + `field`+`equals` | **never** | nothing bounds a string field's rate of change |

**The `removal_reason` row is the strong one and it is not our inference.** §8.1 states the
invariant and states that it holds: *"Within one `(entity, occupancy)` pair, no `sample` ever
appears after that pair's `entity_remove`. That invariant does hold, and it is the one worth
asserting."* So a sample carrying `(E, k)` at time *t* is **positive evidence** that occupancy *k*
had not been removed at *t*. Sampling gaps do not weaken it — a sample on the far side of a gap
proves non-removal across the whole gap, because a re-created entity would carry a *higher*
occupancy. If the occupancy's last sample is the segment's last sampled instant, `not_met` is
sound. If its samples stop earlier, the tail is unobserved and the verdict is `indeterminate`.

This is what saves [B]'s own dangerous example. *"Did anything reach a terminal state it should
not have"* is answerable here — soundly, as `not_met` — **because the entity kept publishing**,
not because nothing said it did. C4 and C6 are `not_met` in all seven runs on exactly that
ground.

### The continuity bound, measured

For `proximity` and `area`, the bound is the furthest the pair could have closed inside an
unobserved window: `(v_A + v_B) × Δt_max`, over the largest gap in either track. Measured for C1:

| value | closest, m | verdict | margin, m | Δt max, s | v_rel, m/s | bound, m | margin / bound |
|---:|---:|---|---:|---:|---:|---:|---:|
| 11 | 8693.17 | not_met | 5693.17 | 0.1000 | 11.0 | 1.10 | **5176 ×** |
| 27.5 | 7773.13 | not_met | 4773.13 | 0.1000 | 27.5 | 2.75 | **1736 ×** |
| 55 | 6283.62 | not_met | 3283.62 | 0.1000 | 55.0 | 5.50 | **597 ×** |
| 82.5 | 4892.45 | not_met | 1892.45 | 0.1000 | 82.5 | 8.25 | **229 ×** |
| 110 | 3711.87 | not_met | 711.87 | 0.1000 | 110.0 | 11.00 | **65 ×** |
| 165 | 2934.45 | **met** | — | — | — | — | — |
| 220 | 2934.45 | **met** | — | — | — | — | — |

**No run in the committed sweep is indeterminate on C1, and the tightest margin clears its bound
by a factor of 65.** The maximum gap is `0.1000 s` in every one of the seven runs — exactly one
missed frame at the platform's 0.05 s period, which is F-11 showing through at the size F-11
measured it. Both tracks span segment 0 in all seven runs.

**This is the axis's choice paying off, and it is worth naming as such.** `m5-oq4.md` §8.1 chose
this axis partly because *"its effect appears as different values in samples that are present"* —
the only one of [B]'s three of which that is true. The table above is what that sentence buys: a
condition over the swept parameter returns a real verdict at every value rather than
`indeterminate`, and CR-AS-4 does not bite on the one column the sweep exists to show.

**And the degenerate case is right rather than lucky.** C7's two entities are static — both bases
have `velocityNed [0,0,0]` in every sample — so `v_rel` is 0 and the bound is 0 m: nothing
unobserved can move them. Measured separation 1393.49 m against a 10 m threshold, so `not_met` is
certain. A bound of exactly zero is nonetheless a bound computed from a *sample field*, and a
field can change between samples; the implementation adds the platform's own measured
acceleration clamp, `½ × 20 m/s² × Δt²` = **0.10 m** at Δt = 0.1 s (F-21's 20 m/s²), so the bound
is never zero and never rests on an entity being static forever.

*A vendored comment that is a claim rather than a measurement.* `contract/example.conditions.json`
justifies its never-met proximity case with *"the two bases are about 400 m apart, so a 10 m
threshold cannot be reached."* Measured here, `BlueBase_Airfield` to `BlueBase_AmmoDepot` is
**1393.49 m**; the 540.73 m pair is Airfield to CommandCenter. The condition is never met either
way, so nothing depends on it. Recorded because it is the same shape as F-18: a number in
`contract/` that nobody re-derived.

---

## 5. The digest is faithful, and it is truncated — E-5

**F-19 said a pin check cannot verify `condition-file-schema.md` because no EXT-08 file has that
name. That is true and it is not the end of the check.** The digest cites its own source twice —
a commit, `eedc228`, and a section, *"documented in README.md under 'Declaring conditions'"*.
Both resolve. `eedc228` is a real commit; the section exists there; and the section is **byte-
identical at `eedc228` and at `main` (`eb13485`)**, so the digest's own pin is valid and its
source has not moved since. That is a better record than `PROVENANCE.md`'s own pin, which is
stale (F-18). Checked by correspondence rather than by identity:

**Everything the digest contains is verbatim.** *"Declaring conditions"* and *"Verdict
semantics"*, from the closed-vocabulary paragraph through the three-line verdict example, are
reproduced word for word including the key table. So F-19's record improves: the file is not
fabricated and it is not paraphrased — it is an **excerpt**.

**What did not cross is the part M6 needs.** EXT-08's README continues immediately, and the two
sections after the ones that were taken are:

- **"How distance is computed"** — that positions are converted to **ECEF on WGS-84** and the
  distance is the straight-line Euclidean distance in metres, with Haversine rejected for
  ignoring altitude and Vincenty for not converging near-antipodally.
- **"Boundary semantics"** — that the comparison is `<=` so a point exactly at `within_m` or
  exactly on a circle's edge is **inside**; that a point on a polygon's edge or vertex is
  inside; and that polygons are plane figures in latitude/longitude, unsupported across the
  antimeridian or a pole.

Neither is in `contract/`. **Both are required to satisfy CR-AS-2's third acceptance criterion** —
*"A verdict's numbers are reproducible: recomputing them by hand from the samples it names gives
the same values."* From the digest alone, `within_m` is a distance with no stated metric: a
consumer could reasonably compute great-circle, or a 2-D horizontal separation, and produce
verdicts that disagree with EXT-08's on the same capture while parsing the same file. That is
silent divergence across the project boundary, which is the failure the whole `contract/`
discipline exists to prevent — and it would have been invisible, because the files still parse.

**And the omission is not drift — the material was there when the excerpt was taken.** Both
sections exist at `eedc228`, the commit the digest itself names, sitting immediately below the
last paragraph that crossed. Nothing was added upstream afterwards; the excerpt stopped one
heading early.

**This is a defect in `contract/`, and it is raised, not worked around** — **E-5**, in the class
of E-3 and E-4. It has two parts: the digest omits material its own cited source carries at its
own cited commit, and `PROVENANCE.md`'s table lists the digest beside two files that are verbatim
vendored, which is what let the omission go unnoticed until something tried to compute a distance.

**It differs from E-3 and E-4 in one way that matters for how it is handled.** Those are
imprecisions in a **frozen, verbatim-vendored** specification, so EXT-17 implements what the
specification says and names the gap beside it. E-5 is a gap in a **digest** — there is no
frozen text here to implement, and no EXT-17 behaviour that can be made to match a document that
does not state the behaviour. So E-5 asks for the two sections to be vendored, and until they
are, EXT-17 must **decide** the computation rather than defer to it. That is the next paragraph,
and it is the first time this project has had to fill a hole in `contract/` rather than route
around one.

**What EXT-17 does meanwhile, on the record.** `contract/` is read-only and the omission does not
block M6, because the computation is decidable here from what *is* vendored: `capture-format-v1.md`
§15 forbids converting units, and positions arrive as three-component `[lat, lon, alt]` geodetic.
A metric that discards the third component discards data the format went to trouble to preserve,
and two entities stacked vertically are not close. **EXT-17 computes ECEF on WGS-84 and
straight-line Euclidean distance in metres, with `<=` at the threshold**, and states the constants
in the README so any verdict is reproducible with a calculator.

**That is the same method EXT-08 uses, and honesty about how that is known matters.** It was read
in EXT-08's README while checking whether the digest was faithful — a pin check, which is a thing
this project does under R4/R11 — and not taken from `contract/`, because it is not there. So it is
recorded as a **corroboration** of a choice EXT-17 had to make anyway, not as an inheritance. If
E-5 is accepted and the sections are vendored, the method becomes inherited properly and this
paragraph becomes unnecessary. Until then the README states it as EXT-17's decision, which is what
it currently is.

---

## 6. The reason the PRD expected to supersede did not arise

PRD rev 1 named one likely cause: *"Superseding it may be necessary if the campaign needs
cross-run conditions, which EXT-08's per-run schema does not express."* Checked directly against
the thing that would need it — CR-PAR-2's requirement that a condition change outcome across the
sweep.

**It needs no cross-run condition.** C1 is one per-run condition, evaluated independently in each
of seven runs, and the flip is visible because the **report** puts the seven verdicts in
parameter order. The comparison across runs belongs to the sweep table, not to the condition
language. Putting it in the condition file would move a reporting concern into a parser and would
be the first step of the expression-language rabbit hole ADR-5 exists to close.

Recorded because the PRD named it as the thing to check, and the answer is a clean no.

---

## 7. What this decision commits M6 to

- **The condition file is the vendored shape, parsed by EXT-17's own loader** in `src/assert/`,
  linking nothing — the fourth component under that rule, after `src/capture/`, `src/compare/`
  and `src/param/`, and for the same reason: the whole of CR-AS-1's and CR-AS-3's surface then
  tests with no simulator.
- **Unknown key, duplicate key, duplicate id, unrecognised kind, and `scenario_unload` as a
  `removal_reason` are five distinct named parse errors**, all before any host starts (CR-AS-1).
- **A verdict is three-valued**, one per condition per run, carrying the condition id, each
  entity **with its occupancy**, the segment, the deciding `sim_time_s`, the deciding values, and
  — for a `not_met` — the bound that made it sound (CR-AS-2, CR-REP-2).
- **`indeterminate` is a verdict state and never a fifth run outcome.** A run carrying one is
  reported with its four-state outcome and the indeterminate verdict named, so [B]'s acceptance
  criterion 5 stays exactly satisfied.
- **The committed example carries C1–C7**, which between them exercise all three kinds, both
  never-met paths, a genuine `indeterminate`, and two conditions that flip across the sweep at
  different values.
- **The README states the per-form absence classification of §4**, the two limits of §3.4 and
  §4, and the distance method of §5 with its constants.

## 8. What this did not decide, and what it did not measure

- **It did not answer OQ-2 or re-raise E-1.** Neither is M6's, and the gate basis is still a flag
  with a default.
- **It did not measure a condition against a rotated capture set.** The reader supports rotation
  (M3) and the campaign does not produce one under `stop`; a condition over a multi-part set is
  untested and is not in M6's scope.
- **It did not measure the `field`+`equals` form against a field that actually transitions
  mid-run and reverts.** No such field was observed — `health` moved through
  nominal → degraded → disabled → wrecked → destroyed with **zero regressions in seven runs** —
  but seven runs supporting a direction is not a guarantee, and the classification in §4
  deliberately does not lean on it. If a future measurement establishes monotonicity as a
  platform property, `field`+`equals` becomes decidable and §4's last row is revisited.
- **It did not check the digest against every commit between `eedc228` and `eb13485`.** The
  correspondence in §5 is to those two, which agree; the path between them is unexamined and
  nothing here depends on it.
- **It did not judge a run that failed.** All seven committed captures are of runs that completed.
  A condition evaluated over a `timeout` or `infrastructure_error` run is M6's to build and is
  not measured here.
