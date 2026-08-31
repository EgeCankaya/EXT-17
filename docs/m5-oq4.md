# OQ-4 — which parameterisation axis, and what a sweep across it can honestly claim

**Status: DECIDED at M5, on measurement taken here, before any of M5 was built.** The decision
governs what M5 builds; it is carried into `docs/prd.md`, `README.md` and `n8ro-campaign`'s
defaults as M5 lands.

**Decision**

> **The axis is *initial positions and velocities* — [B]'s first — realised as one declared
> scalar applied to a declared set of entities before `start`.** The committed example sweeps
> the closing speed of the Red raid in `Atacama Air Defense`.
>
> It was chosen over the other two on the two criteria the PRD names, and on one it does not:
>
> 1. **Fidelity, measured.** The injected value is honoured *exactly* — first frame of the run
>    to last — across the whole range this project will sweep, and the range has a **measured
>    upper bound at 400 m/s** above which the platform clamps it. The bound was found by
>    exceeding it, not assumed.
> 2. **What CR-AS-4 can assert over it.** The axis's effect appears as *different values in
>    samples that are present*, never as an absence. It is the only one of the three of which
>    that is true.
> 3. **The roster is identical at every value** — 42 authored `(entity, occupancy)` keys at all
>    seven probed values. Two runs at different values are computed over the same set of
>    entities, which is what makes a sweep's results comparable across it at all.
>
> **And it carries one cost, measured and not hidden.** The parameter is applied before `start`,
> and it can land between two publications of the start-up roster burst — which makes segment 0
> `frozen` with **differing** repeated values, a third shape through a test §5.1 presents as
> detecting one thing. **4 of 35 parameterised runs**, against R12's 1 in 27 for ordinary ones.
> See §6 and §7. **The gate is not weakened to accommodate it, and no retry is added.**
>
> What the same measurement also establishes, in the other direction: **a parameterised run is
> still a valid input to the gate.** Four runs at one value, all six pairings, 293 576 samples
> compared, zero differing — CR-PAR-1's third criterion discharged by demonstration (§7).

---

## 1. Why this needed measurement rather than an argument

R9 — *"the parameterisation axis may require writing into a read-only tree"* — was **closed at
M2** by `tools/spike-axis`: all three of [B]'s axes are reachable over the bus with no authoring
into `C:\N8RO`. That closed the constraint and freed the choice, and PRD rev 3 says so: *"the
choice at M5 is free on merit."*

But M2's spike stated its own limit, in its own words:

> **It measured feasibility, not fidelity.** That an injected position is honoured says nothing
> about whether a swept range of positions produces a scenario that still makes sense — an
> entity placed somewhere absurd is reachable and useless.

Fidelity is OQ-4's first deciding criterion and **nothing had measured it**. So M5 measured it,
the same way M3 decided OQ-6: by exercising the thing rather than reading about it.

---

## 2. What was run

`tools/spike-oq4/` — a spike, not a product, in the same shape as `tools/spike-axis`. It links
the N8RO SDK through the same `src/` components `n8ro-campaign` uses, so what it measures it
measures about the product's own control path. It acts through `RunConfig::afterLoadBeforeStart`,
the seam M2 built; it adds no second mechanism.

| probe set | what | runs | evidence |
|---|---|---|---|
| the sweep | one scalar — the Red raid's closing speed — at 11, 27.5, 55, 110, 220, 440 and 900 m/s, 1200 frames each | 7 | `campaigns/m5-oq4/` |
| the sensitivity batch | six runs at 110.0 … 110.5 m/s — six *different* values, 0.1 apart | 6 | `campaigns/m5-oq4-repeat/` |
| the pair batch | **four runs at exactly one value**, and all six pairings put through the gate | 4 | `campaigns/m5-oq4-pair/` |
| the catalogue | one run that asks the platform's own scenario query what exists | 1 | `campaigns/m5-oq4/catalogue/` |

Eighteen runs, about 480 MB, an hour of machine time.

Read back with `python tools/m5-checks/oq4_fidelity.py campaigns/m5-oq4`, and with
`n8ro-capture` and `n8ro-compare` — the milestone's own binaries — for anything that decides
something.

**Every one of the eighteen runs completed, and every capture is conformant and covers its
whole run.** That is the floor, not the result.

### The candidate, and why this scalar

`Atacama Air Defense` authors twelve Blue defenders — a short-range SAM battery, three CIWS
guns, two radars, an EO tracker, a command post and four fixed installations — clustered at
about `(-23.4965, -68.2474)`, and thirty Red loitering munitions in two groups: twelve flying
south from the north, eighteen flying west from the east, **every one at 400 m and 55 m/s**,
about 8.6 km out.

A frame budget of 1200 is 60 s. At the authored 55 m/s the raid covers 3.3 km of that 8.6 km,
so **the closing speed decides how far the raid gets inside the window** — and the baseline run
already contains real engagement: `health` leaving `nominal`, and SAM rounds created and
deleted as entities. It is a scalar with a physical meaning, a natural order, and an outcome
that plausibly depends on it.

Position was deliberately **not** touched (`std::nullopt`), so the formation the scenario
authored is the formation that flies and the only thing varying is the scalar.

---

## 3. Fidelity: honoured exactly, up to a ceiling this project found by exceeding it

| injected | at the run's first frame | at its last frame | honoured? |
|---:|---:|---:|---|
| 11 | 11.0 | 11.0 | **yes** |
| 27.5 | 27.5 | 27.5 | **yes** |
| 55 *(authored)* | 55.0 | 55.0 | **yes** |
| 110 | 110.0 | 110.0 | **yes** |
| 220 | 220.0 | 220.0 | **yes** |
| 440 | 439.0 | **400.0** | **no — clamped** |
| 900 | 899.0 | **400.0** | **no — clamped** |

**There is a hard speed ceiling at 400 m/s, and the platform walks the entity down to it at
exactly 20 m/s²** — 1 m/s per 0.05 s frame. Measured on `RedUAV_N_01`:

```
v440    t=0.05  439.00     t=0.10  438.00     ...   t=7.45  400.03    t=60.00  400.00
v900    t=0.05  899.00     t=7.65  747.00     ...   t=30.10 400.04    t=59.90  400.00
```

So a value above the ceiling is not the value that flew: `v440` spends 2.0 s of its 60 off
parameter, and **`v900` spends 25.0 s — 42% of the run — decelerating through values nobody
asked for.** A sweep that crossed the ceiling would plot a result against a number that stopped
being true a third of the way in. That is precisely the failure mode "fidelity" names, and it
is why the criterion was worth a day.

**The usable range of this axis is therefore `(0, 400]` m/s on this scenario's profile**, and
the committed sweep stays inside it. The lower end was probed only to 11 m/s; nothing here
establishes what happens at 0, and no claim is made about it.

Altitude held at 400 m at every value, and **not one non-finite number appears in any of the
seven captures** — no NaN, no infinity, in any position or velocity component.

---

## 4. The roster is identical at every value, and that is not a detail

| | |
|---|---:|
| authored `(entity, occupancy)` keys carrying samples, at every one of the seven values | **42** |
| Red raiders present, at every value | **30** |

Nothing the sweep does changes which entities the scenario authors. That matters twice:

- **Across the sweep**, every run's result is computed over the same set of entities, so a
  number at 11 m/s and a number at 220 m/s are the same number about different runs rather than
  different numbers about different rosters.
- **Inside a self-test pair**, the comparison keys identity on `(entity, occupancy)` and treats a
  key present in one run and absent from the other as a **fail**. An axis that moved the roster
  would be putting pressure on exactly that rule. This one does not touch it.

The keys that *do* vary are the weapons — `BlueSAM_ShortRange_wpn_*`, `BlueGun_*_wpn_*` — which
are created during the run. **They are a result of the parameter, never part of it**, and the
distinction is worth keeping because they are counted in `entity_add` and in `trailer.counts`.

---

## 5. A condition really does change outcome across the range

CR-PAR-2's third acceptance criterion is that at least one condition actually changes outcome —
*"so the trend is real rather than a flat line"*. Read off the seven captures:

| speed m/s | closest any raider got, m | raiders leaving `health: nominal` | weapon rounds fired | **CIWS gun rounds** |
|---:|---:|---:|---:|---:|
| 11 | 7 948 | 3 | 6 | **0** |
| 27.5 | 6 965 | 3 | 6 | **0** |
| 55 | 5 326 | 3 | 5 | **0** |
| 110 | 2 048 | 12 | 19 | **13** |
| 220 | 17 | 10 | 20 | **15** |
| 440 | 21 | 5 | 11 | **8** |
| 900 | 20 | 2 | 6 | **4** |

Two things in that table, and the second is the more honest one.

**A binary condition flips, cleanly and for a physical reason.** *"No CIWS gun engaged"* is true
at 11, 27.5 and 55 m/s and false at 110 and above: below about 110 m/s the raid never reaches the
guns' envelope inside the 60 s window, and above it, it does. Closest approach falls
monotonically 7 948 → 6 965 → 5 326 → 2 048 → 17 m as it does. That is a real trend with a
mechanism, not a coincidence of counters.

**And it is not monotone at the top, which the report must not hide.** Raiders hit peaks at 12
(110 m/s) and falls to 2 (900 m/s): a raid fast enough to overfly the battery is engaged less,
not more. Two of those three top rows are also **above the fidelity ceiling**, so their numbers
describe a raid that decelerated to 400 m/s rather than one that flew at the swept value. A
sweep whose committed range stops at the ceiling has neither problem; a report that plots all
seven must say which points are which.

---

## 6. The cost this axis carries, and it lands on the determinism gate

This is the finding that nearly changed the decision, and it is the reason the axis is chosen
*with* a named cost rather than cleanly.

The parameter is applied between `load_scenario` reporting loaded and `start` being published.
The platform publishes a start-up roster burst at load — and M4 measured (F-13, E-4) that **part
of that burst is sometimes published twice**. When it is, and our update landed between the two
publications, the second copy carries **different values from the first**:

```
v440   RedUAV_N_01  t=0.00  velocityNed |55|      <- authored, published at load
       RedUAV_N_01  t=0.00  velocityNed |440|     <- ours, published after the update
```

Measured across the sweep:

| | |
|---|---:|
| runs whose segment 0 the format's §5.1 test classifies **frozen** | **2 of 7** (`v110`, `v440`) |
| duplicated instants in each such run | **31** |
| …carrying **identical** values — the Blue entities, which nothing updated | **12** |
| …carrying **differing** values — Red raiders, updated between the two bursts | **19** |
| runs where the burst was published once: duplicated instants | **0** |

**This is a third shape through one test.** §5.1 presents its test as detecting a reset clock;
M4 measured a second phenomenon satisfying it — a duplicated publication with identical values —
and raised it as E-4. This is a duplicated publication whose values **differ**, and the
difference is one this project introduced. It is, by value alone, indistinguishable from a real
determinism failure — which is an argument for the exclusion rule being right, not for relaxing
it.

**Consequence, stated plainly:** when this happens the comparison refuses `no_comparable_segment`,
the campaign stops at its own gate with exit 3 and no run is attempted. R12 measured ~1 pair in
14 for ordinary runs; §7 measures the run-level rate here and finds it higher on a sample too
small to attribute.

**There is deliberately no retry**, exactly as R12 requires. A harness that re-rolls its gate
until it likes the answer has no gate.

---

## 7. A parameterised run is still a valid input to the determinism gate — six pairs of it

CR-PAR-1's third acceptance criterion is that *"two runs with the same parameter value are
identical configurations, and are therefore valid inputs to CR-DET-1's self-test."* That is a
claim, and until now nothing had run it. **Four runs at exactly 110 m/s**
(`campaigns/m5-oq4-pair/`), compared with `n8ro-compare` — the binary that decides the gate —
in all six pairings:

| pair | compared | agreeing | differing | coverage |
|---|---:|---:|---:|---:|
| 000 vs 001 | 48 951 | 48 951 | **0** | 99.8470% |
| 000 vs 002 | 48 927 | 48 927 | **0** | 99.6761% |
| 000 vs 003 | 48 929 | 48 929 | **0** | 99.7066% |
| 001 vs 002 | 48 912 | 48 912 | **0** | 99.7675% |
| 001 vs 003 | 48 903 | 48 903 | **0** | 99.7491% |
| 002 vs 003 | 48 954 | 48 954 | **0** | 99.7575% |
| **total** | **293 576** | **293 576** | **0** | worst **99.6761%** |

**Six of six pass the content gate.** All four runs classify segment 0 `running`, and all four
carry exactly **61** `(entity, occupancy)` keys, 61 adds and 61 removes — the same roster
lifecycle, weapons included. The criterion is discharged by demonstration rather than asserted.

**One number to watch rather than to celebrate.** The worst coverage here is **99.6761%**, where
the worst of M4's 190 unparameterised pairs was 99.8513%. The 99% floor still clears it by about
four times the observed shortfall instead of seven. That is comfortably inside the floor and it
is a smaller margin, and a sweep at 20 values will produce more pairs than this to draw on.

### And two runs at *different* values are correctly not a pair

The other half of the same criterion, which is the one that could quietly go wrong. Six runs at
110.0, 110.1 … 110.5 m/s (`campaigns/m5-oq4-repeat/`) — differences of 0.1 m/s, physically
negligible and configurationally total. Comparing 110.0 against 110.1:

```
  GATE                  FAIL   on the content basis
                        33546 sample(s) present in both runs at the same simulation instant
                        carry DIFFERENT values. This is not the publication schedule; it is
                        the simulation.
```

The comparison is right, and **M5 must ensure nothing ever hands it such a pair.** Two runs at
different parameter values are two configurations; a gate over them would report a difference
that means only that the sweep worked.

### What that batch also measured: the axis is sharply sensitive, and deterministically so

| value m/s | 110.0 | 110.1 | 110.2 | 110.3 | 110.4 | 110.5 |
|---|---:|---:|---:|---:|---:|---:|
| closest approach, m | 2 048 | 2 042 | 2 036 | 2 030 | 2 024 | 2 018 |
| raiders leaving `nominal` | 12 | 13 | 10 | 10 | 12 | 13 |
| CIWS gun rounds | 13 | 12 | 13 | 12 | 12 | 14 |

**The continuous quantity is exactly linear — 6 m per 0.1 m/s over 60 s, which is the
kinematics — and the discrete counters jitter.** The jitter is *not* nondeterminism: the four
same-value runs above agree exactly, on 61 keys each. It is the axis genuinely being that
sensitive near the engagement threshold, where a few metres decides one intercept.

That is a design instruction for M5's report rather than a defect: **a sweep should carry a
continuous quantity for the trend and a discrete condition for the flip, and must not present a
non-monotone integer counter as if it were noise-free.**

### The frozen-segment rate under injection

| batch | frozen segment 0 |
|---|---:|
| the OQ-4 sweep, 7 values | **2 of 7** |
| the 110.x sensitivity batch, 6 runs | 0 of 6 |
| the same-value pair batch, 4 runs | 0 of 4 |
| the committed sweep, first execution, 9 runs | **2 of 9** |
| the committed sweep, re-executed after F-24's fix, 9 runs | 0 of 9 |
| **every parameterised run** | **4 of 35** |
| R12's baseline for ordinary runs (M4, F-13) | 1 of 27 |

**Elevated, consistent across two independent batches, and still not established as caused.**
4 of 35 (11.4%) against 1 of 27 (3.7%) is three times the rate on a sample that supports a
direction and not a number. What *is* established is the **shape**: when the burst duplicates
under injection the repeated values **differ** (§6). Reproduced on the committed sweep's own
runs — 23 and 30 duplicated instants, **12 identical in both** (the Blue entities, which nothing
updated) and 11 and 18 differing (the raiders, which were). The 12 is the same number the OQ-4
sweep produced, which is the untouched roster showing through in every case.

R12's instruction stands: if the rate is materially higher at M6's scale that is an escalation
about the host, not a reason to weaken the gate.

**Note on the two executions of the committed sweep.** It was run, F-24 was found in the report
it produced, the reporting was fixed, and it was run again — and the second execution happened
to freeze nothing. Both are kept (`campaigns/m5-sweep-first/` and `campaigns/m5-sweep/`) and
both are counted above. **The re-run was because the tool changed, not because the numbers were
unwelcome**, and reporting only the clean one would be choosing evidence.

---

## 8. The other two axes, against the same criteria

### 8.1 Which entities are present

Feasible in both directions, measured at M2 (`p3` deletes, `p4` creates). The objection is not
feasibility, and it is not quite absence either — the honest version is sharper than the PRD's
shorthand.

Re-read off `campaigns/m2-axis/p3-delete-pre/` with this milestone's tools:

| | |
|---|---|
| `RedUAV_N_01` occupancy 1 appears in `entity_add` | **yes** |
| its `entity_remove` reason | **`commanded`** |
| samples it carries in segment 0 | **0** |

So the deletion itself **is** positively recorded: `entity_remove` with `reason: "commanded"` is
evidence a record makes, not an absence. A condition *"was this entity commanded away"* is
soundly assertable, and the PRD's shorthand — *"a deleted entity still appears in
`entity_add`"* — understates what the capture holds.

**But that assertion is about our own input echoing back, not about the simulation.** The
question a campaign actually asks of this axis is what changed *because* the entity was not
there, and every direct reading of that is a zero-sample count — which is absence, which CR-AS-4
requires to report `indeterminate`. An axis whose interesting result is `indeterminate` by
construction is a poor axis to build a sweep on.

Two further costs, neither fatal on its own:

- **The roster moves with the parameter.** Each value has a different comparable universe, so a
  result at one value is not automatically the same measurement as at another (§4).
- **It is coarse.** Thirty raiders means at most thirty values, and the interesting range is
  probably five or six of them.

### 8.2 Which scenario from the catalogue

Measured here, since CR-PAR-1's fourth criterion makes enumeration a requirement rather than a
convenience if this axis is chosen. `campaigns/m5-oq4/catalogue/`:

```
catalogue answered in 119 ms with 10 scenario(s)
  Alpine Paramotor Traverse            Global Simulated Air Traffic Showcase
  Atacama Air Defense                  Istanbul KAAN Su-35 BVR Angajmanı
  Baltic Sentinel                      Mariana Shield
  Erzurum Corridor                     Outback Kamikaze Swarm
  GenericCivilianPresence
  GenericTwoShipFormation
```

**The enumeration works, it is genuinely asynchronous, and it is cheap.** The answer arrives on
`sim/scenario/query-result` and the wait for it rides the engine-state heartbeat like every
other wait in this project — 119 ms, bounded, no poll. CR-PAR-1's fourth criterion is
satisfiable, and it is satisfiable today.

Fidelity is this axis's *strength*: every value is an authored scenario, so no value can be
absurd. It fails on the other two.

- **There is no order, so there is no trend.** CR-PAR-2 requires the sweep to *"order runs by
  parameter value"* and make *"the dependence of the result on the parameter"* visible. Ten
  unrelated scenarios can be listed alphabetically; they cannot be ordered *by the parameter*,
  because the parameter is a name. What such a chart shows is ten differences, not a trend, and
  [B]'s acceptance criterion 3 asks for a trend in those words.
- **No result is comparable across values.** `Atacama Air Defense` has 42 entities and `Baltic
  Sentinel` 18 (M2, `p5`). Any per-run number is computed over a different roster at every point,
  so CR-AS-4's soundness question has to be re-answered for each of ten scenarios rather than
  once.

One practical note recorded for whoever revisits this: **one catalogue entry's name is not
ASCII** — `Istanbul KAAN Su-35 BVR Angajmanı`. If this axis is ever chosen, that name becomes a
run-directory name, a JSON string and a report label, and its encoding is then load-bearing on
three paths that have never carried a non-ASCII byte.

---

## 9. The decision, and what actually decided it

| criterion | **A — positions and velocities** | B — which entities are present | C — which scenario |
|---|---|---|---|
| Reachable with no authoring (R9, M2) | yes | yes | yes |
| **Fidelity — does a swept range still make sense** | **yes, with a measured ceiling at 400 m/s** | untested; plausible, and coarse | **yes, by construction** |
| **What CR-AS-4 can soundly assert** | **values in samples that are present** | the delete echo, positively; the *effect* only as absence | per scenario, and never across them |
| Ordered, so a trend exists (CR-PAR-2) | **yes — continuous** | by count; coarse | **no — a name has no order** |
| Roster stable across values | **yes, 42 at every value** | no, by construction | no |
| A condition changes outcome (CR-PAR-2) | **yes — CIWS engagement flips between 55 and 110 m/s** | likely | likely, but not as a trend |
| Cost to the determinism gate | **4 of 35 runs refused (§6, §7)** | unmeasured; acts through the same pre-start seam, so the same shape is available to it | none beyond R12's baseline |
| Extra bring-up | none | none | an asynchronous catalogue query (119 ms, measured) |

**Axis A is chosen.** C loses on ordering, which is not a preference — CR-PAR-2 and [B]'s
acceptance criterion 3 both ask for a *trend*, and a nominal parameter cannot have one. B loses
on CR-AS-4, though not as bluntly as the PRD's shorthand suggests: the deletion itself is
positively recorded and is assertable, but the question the axis exists to answer — what changed
*because* an entity was not there — reads as a zero-sample count, and this project does not
answer questions from absence.

A's own cost is real and it is named: it presses on the determinism gate. **The gate is not
adjusted to accommodate it.**

---

## 10. What this decision commits M5 to

- **The axis and its values are declared in campaign configuration**, and changing them is not a
  rebuild (CR-PAR-1).
- **The value reaches the run record as the text it was declared as**, not as a re-formatted
  double. M4's comparison never converts a number for a decision (CR-DET-2); a report that
  printed a locale-formatted value would put the hazard back on a path the build searches.
- **The self-test runs at a value the campaign actually sweeps**, and the report says which value
  was gated and that determinism is established *for that value*.
- **Two runs at different parameter values are never compared.** They are different
  configurations; the comparison would be right to fail them and the failure would mean nothing.
- **The committed example sweep stays inside the measured fidelity range**, and any point outside
  it is labelled as outside it rather than plotted as if it were not.

## 11. What this spike did not measure

- **It did not measure the axis below 11 m/s**, and makes no claim about 0.
- **It did not measure fidelity for axes B or C.** B is untested and C needs none; neither was
  chosen, and re-opening either means measuring first.
- **It did not vary position.** The axis as decided is *"initial positions and velocities"*, and
  the committed sweep varies velocity. Position is reachable (M2, `p1`) and unswept.
- **It did not judge anything.** No condition is evaluated, no verdict exists. The outcome
  columns in §5 are counts read off captures, which is what M5 has; verdicts are M6.
- **It did not measure a second scenario's ceiling.** 400 m/s is this profile's; another
  scenario's raiders may clamp elsewhere.
