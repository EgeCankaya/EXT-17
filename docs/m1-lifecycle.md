# M1 — one headless run, by hand, and what it publishes

**Date:** 2026-08-31
**Milestone:** M1 ([B] step 1 — *"Run one scenario headless, by hand, to completion. Learn the
host's lifecycle before automating it."*)
**Platform:** N8RO runtime 2.1.328, SDK component `com.n8ro.dev` 2.1.328
**Scenario:** `Atacama Air Defense`, 42-entity roster, stopped at frame 1200
**Evidence:** `captures/m1/` — the capture, the driver transcript, and both host logs. Untracked
by `.gitignore` (25 MB); the numbers below are all re-derivable from it.

> Every number in this document was measured here, on this machine, during this milestone. Where
> it agrees with something inherited from EXT-08 the agreement is stated as a *reproduction*, not
> as a citation. Where it does not, that is said too.

---

## 1. What was actually run

Three processes, started in this order, each driven as a process and torn down by the handle
that created it — never by image name (CR-EX-1).

```
set N8RO_RELEASE=C:\N8RO

1. n8ro-bridge.exe --config SimEngineClient_SharedMemory --model-path C:\N8RO\data\db
                   --schema-file N8roSimSchema --out-dir <run-dir> --run-label 000
2. n8ro-sim-app.exe --sim-config SimEngineHost_SharedMemory --model-path C:\N8RO\data\db
                    --schema-file N8roSimSchema          (working directory: <run-dir>)
3. m1-run.exe --scenario "Atacama Air Defense" --frames 1200
```

`m1-run` is this repository's own tool (`tools/m1-run/`). It exists because **nothing the
platform ships publishes a bus command from a script** — see §6. It links the N8RO SDK only
(`SimulationEngineClient`), reads no EXT-08 source, and names no EXT-08 identifier.

---

## 2. The lifecycle, in order, with what marks each transition

| # | Transition | What is published | How it is observed | Observed value |
|---|---|---|---|---|
| 1 | Host up, engine idle | `sim/engine/state` begins publishing | first non-empty `getEngineState()` | `state=idle frame=0 simTime=0.000` |
| 2 | Catalogue answered | `sim/scenario/query` → `sim/scenario/query-result` | `getLastDbList()` / `getLastScenarioList()` become non-empty | 2 databases, 10 scenarios |
| 3 | Scenario loaded | `load_scenario` on `sim/scenario/command`; engine publishes the roster burst, then `scenario_loaded` | `isScenarioLoaded()` | `scenario=loaded(Atacama Air Defense)`, still `state=idle` |
| 4 | Running | `start` on `sim/engine/command` | `isRunning()` | `state=running frame=0` |
| 5 | Frames advance | `sim/engine/state` per frame | `getFrameNumber()` | `dt=0.05000` throughout — **20 Hz, frame N ↔ simTime N×0.05, exactly** |
| 6 | Stop | `stop` on `sim/engine/command` | `getEngineState()` / `getFrameNumber()` | `running frame=1200 simTime=60.000` → **`idle frame=0 simTime=0.000`, scenario still loaded** |
| 7 | Host exits | engine-state heartbeat stops | recorder declares host loss after 3.0 s of silence | capture closed, `end_reason: "host_lost"` |

**Three things worth pulling out of that table.**

**Load is not start, and neither is a sleep.** After `load_scenario` the engine is `idle` with a
scenario loaded; it advances no frames until `start`. Both transitions are observable on
`sim/engine/state`, so CR-EX-2's "observed condition, never a fixed delay" has a concrete signal
for every wait in the sequence. Nothing in this run needed a sleep to sequence it.

**Stop rewinds the clock and reloads the scenario, and you can watch it happen from outside.**
Line 6 is the mechanism behind the two-segment capture, visible in engine state rather than
inferred from the file: the frame counter returns to 0, `simTime` returns to 0.000, and
`isScenarioLoaded()` stays true — the scenario was reloaded from source, not unloaded. The
platform documents this as intended (`engine-lifecycle-and-control.md`: *"stop rewinds the
simulation timer to zero, reloads the current scenario from source… That is what makes two
successive runs of the same scenario reproduce each other"*).

**Frame and simulation time are rigidly locked.** `dt` was `0.05000` on every sample of the run.
That matters for OQ-1: a frame budget and a simulation-time budget are the *same* predicate on
this configuration, and both are free of any wall clock.

---

## 3. The capture: structure

52 798 records, 25 280 106 bytes. The reader's own tally agrees with `trailer.counts` exactly
(2 segments, 52 656 samples, 89 adds, 47 removes) — CR-CAP-2's criterion, met on a fresh file.

| | segment 0 — the run | segment 1 — the teardown reload |
|---|---|---|
| closed with | `scenario_unloaded` | `host_lost` |
| samples | 50 430 | 2 226 |
| `sim_time_s` span | 0 → 59.99999999999873 | 0 → 0 |
| `entity_add` | 47, **all at occupancy 1** | 42, **all at occupancy 2** |
| `entity_remove` | 47 | 0 |
| max samples for one `(entity, occupancy)` at one `sim_time_s` | **1** | **53** |

**One ordinary run produced two segments** (PROVENANCE finding 3 — reproduced). **Occupancy 2 is
real** and is where the roster is re-created under the same 42 names (finding 4 — reproduced).
**The frozen-clock test is exact and it works**: 1 in the running segment, 53 in the frozen one,
so the format's own detection rule separates them cleanly (finding 2 — reproduced).

**Both `segment_open` and `segment_close` report `sim_time_s = 0`** in both segments, so a
segment's duration computed from its boundary records is zero. CR-CAP-4 names this; it is true
here for segment 0 as well as segment 1, so duration must come from the samples.

### The 47 adds are 42 + 5, and the removes explain themselves

Segment 0's removes carry three reasons: **40 `scenario_unload`, 4 `expended`, 3 `destroyed`**.
The roster is 42; five `BlueSAM_ShortRange_wpn_*` munitions were created mid-run, four of them
expended, three RedUAVs destroyed, and the 40 survivors removed at unload. 4 + 3 + 40 = 47.

**A scope note on finding 4, stated because it would be easy to over-claim.** PROVENANCE says
the engine re-creates entities under the same name *"both mid-run and at teardown"*. This run
reproduced the **teardown** half exactly. It did **not** exercise the mid-run half: no name was
added twice inside segment 0, and occupancy 2 appears nowhere in it — the munition names embed a
spawn counter, so they are unique per spawn in this scenario. The `(entity, occupancy)` key is
still mandatory, and CR-CAP-4 still stands; M1 simply did not observe mid-run reuse and does not
claim to have.

---

## 4. The capture is not a complete transcript, and nothing in it says so

This is the most consequential thing M1 measured, and it was not sought — it fell out of the
first run recorded.

Restricting to the **40 entities that were never removed mid-run**, and counting from each
entity's first sample to the end of segment 0:

```
frames in segment 0                     1198
entities alive to the end                 40
   missing 0 frames                       15 entities
   missing 1 frame                        19 entities
   missing 2 frames                        6 entities
total missing              31 of 46 890 expected  =  0.066%
```

And every counter that could have reported it:

```
drops       samples_not_recorded 0  events_not_recorded 0  samples_orphaned 0
            samples_unnamed 0       samples_untimed 0
bus_metrics schema_hash_drops 0  message_id_drops 0  decode_failures 0
            missing_schema_passthrough 0  legacy_payload_passthrough 0
            messages_dropped 0  dropped_by_backpressure 0
            dropped_by_queue_overflow 0  dropped_by_rate_limiting 0
```

**Fourteen counters, all zero, on a file that is missing 31 samples.** No deliberate overload was
applied; this is an ordinary run. The loss is frame-shaped — per-frame entity counts run 39, 40,
41, 42, 43 across the run with isolated frames at 17, 18 and 34.

R5, PROVENANCE finding 5 and ADR-6 are inherited claims in the PRD. **They are now this
project's own measurement.** CR-AS-4's rule — an assertion never reads absence as evidence — is
not a precaution against something upstream once saw; it is a response to something visible in
the first run this project ever recorded.

---

## 5. Disk, measured here

**478.8 bytes per record**, against the PRD's ~465 B/record estimate [C3]. The 1200-frame run
cost **25.28 MB for 60 s of simulation**, which extrapolates to **~84 MB per 200 s run** —
higher than the ~64 MB inherited from [C2].

Two figures for OQ-6 and CR-CAP-5, from this project's own numbers rather than upstream's:

| Campaign shape | Projected footprint |
|---|---|
| 20 runs × 1200 frames (60 s) | **~506 MB** |
| 20 runs × 200 s | **~1.7 GB** |

Not a decision — OQ-6 is M3's, and it now has a measurement to work from.

---

## 6. Nothing the platform ships can drive the host from a script

Every candidate was checked. This is why `tools/m1-run/` exists at all, and it is worth
recording because a plan written from [B]'s surface table alone would assume otherwise.

| Binary | What it actually is |
|---|---|
| `n8ro-shark` | Passive bus subscriber, GUI. Documented as observe-only |
| `n8ro-sim-starter` | A process launcher wrapper — `<target.exe> [args...]` |
| `n8ro-sim-bot` | An MCP plugin server over ZMQ IPC; the AI-facing surface |
| `n8ro-sim-app` | The host. **No `--help` and no usage text**; `--help` and an unknown option both fall through to normal startup and exit 1 on config failure |
| `n8ro-workbook` | The documented interactive driver — but **GUI-only, no headless mode** |

The control surface itself is fully documented in `data\resources\missions\stubs\`:
`scenarioControl.requestLoadScenario(scenarioName, modelName)`, `.requestStart()`, `.requestStop()`,
`.requestUnloadScenario()`, and `simulation.getFrameNumber() / getState() / isRunning() / getTimeS()`.
Each docstring says it *"publishes the command"* — so the scripting route and the bus route are
the same mechanism underneath, and what M1 observed through one transfers to M2's client.

`SimulationEngineClient` also supplies the asynchronous catalogue queries [B]'s surface table
names — `requestDbList()` / `requestScenarioList(modelName)`, answered on
`sim/scenario/query-result` and polled through `getLastDbList()` / `getLastScenarioList()`. Both
answered in this run. **OQ-4's fallback axis — "which scenario from the catalogue" — is
enumerable over the bus, from a catalogue of ten scenarios, with no authoring into the read-only
install tree.** That does not choose the axis; it removes the R9 objection to one candidate.

---

## 7. Corrections to what this project believed before M1

**(a) `N8RO_RELEASE` is required by the headless host, not only by `n8ro-sim-local`.**
`CLAUDE.md` recorded this requirement against `n8ro-sim-local.exe` alone, and
`contract/PROVENANCE.md` finding 6 gives the host's invocation without it. **The first by-hand
run failed because of it.** With `N8RO_RELEASE` unset, the host resolves its plugin directory
from the current working directory:

```
[WARN] Plugin scan skipped: invalid directory from N8RO_RELEASE/bin/plugins/sim -> <cwd>\bin\plugins\sim
[ERROR] (ScenarioManager) Scenario load refused: Component type 'componentPhysics' has no
        registered factory, so 42 entities, e.g. 'BlueBase_AmmoDepot' would load without it
```

`componentPhysics` comes from `C:\N8RO\bin\plugins\sim\n8ro-physics.dll`, a **stock install
plugin**. Without the scan, every 42-entity scenario load is refused and the host sits idle
indefinitely — it does not fail, which is the dangerous shape. `CLAUDE.md` is corrected.

*This does not weaken [B]'s "Track C, no plugin" constraint: that governs whether **we** author a
plugin, which we do not. It also does not move R6 — the `0xC0000005` teardown observation was
about a plugin-loaded configuration, and this is one, so R6's scoping note is now the load-bearing
half of that risk rather than the reassuring half. Watch teardown across the 20-run campaign.*

**(b) The host writes into its working directory.** Run with CWD at this repository's root, it
created `data/db/` and `logs/`. Run the host from a scratch directory — the same rule `CLAUDE.md`
already carried for `n8ro-sim-local`, now known to apply to `n8ro-sim-app` too.

**(c) PROVENANCE finding 7's deadline is the scenario load, not the host start.** The recorder
logs *"Waiting for the simulation host; no start order is required"* — it tolerates any order
relative to **host start**. The constraint is real but attaches one step later: the
`entity_created` burst fires at **scenario load**, once. For a campaign starting a fresh host per
run the two coincide, but M2's bring-up should order the recorder against the load, which is the
actual deadline. This run attached before the load and got `attached_mid_run: false` with zero
orphans.

**(d) The recorder exits on its own when the host dies**, closing the capture with a well-formed
trailer and `end_reason: "host_lost"` within a 3.0 s window. CR-EX-6's "host dies mid-run" path
therefore yields a valid, readable capture rather than a truncated file — measured twice today,
once deliberately with no scenario loaded at all (a 2-record capture: header + trailer, complete
and conformant).

**(e) `header.platform.runtime_version` came through as `"unknown"` and `schema_version` as `""`.**
A reader should not depend on either. Not a defect to escalate — the format does not promise them
— but worth knowing before M3 builds anything that keys on provenance.

---

## 8. `contract/` pin — re-checked, confirmed, not re-pinned (R4)

| Vendored file | Check | Result |
|---|---|---|
| `capture-format-v1.md` | blob hash vs EXT-08 `78fd4ef` | `7546587…` — identical |
| `capture-atacama-…n8rocap.jsonl` | blob hash vs `78fd4ef` | `a089d23…` — identical |
| `example.conditions.json` | vs `conditions/atacama.conditions.json` | `f5bc63b…` — identical |
| `condition-file-schema.md` | its source unchanged since `eedc228` | `atacama.conditions.json` and `Conditions.{h,cpp}` unchanged across `eedc228..eb13485` |

**No drift.** EXT-08's `main` has moved two commits beyond the pin (`78fd4ef` → `eb13485`), but
the diff is `docs/prd.md` and `docs/demo-recording-script.md` only — nothing in `contract/`'s
scope, and `capture-format-v1.md` is byte-identical at `eb13485` as well.

One honest nit, left alone: `PROVENANCE.md` states the pin is EXT-08's `main` head *"deliberately
— it makes 'is this current?' one comparison against `main`"*. That property is now false in form
though not in content. `contract/` is read-only from here, so it is recorded rather than edited.

**A gap R4 asked for is now closed.** R4's mitigation says to *"test against a capture written by
the pinned producer, not only against the vendored fixture"*, because the fixture is producer
`0.5.0` and predates `header.sample_form`, `header.limits`, `header.part`, `header.continues_from`
and `trailer.continued_in`. `captures/m1/` holds a **producer 0.9.0** capture carrying
`sample_form`, `limits` and `part`. M3's reader conformance suite has one to test against.

---

## 9. What M1 did not do

- **It did not choose the end-of-run predicate.** That is OQ-1, decided at M2 against the four
  criteria the PRD already fixes. What M1 establishes is that a frame budget is *observable*
  (`getFrameNumber()` off `sim/engine/state`), *wall-clock-free*, and *reached* — and that on this
  configuration it is interchangeable with a simulation-time budget at exactly 20 Hz. The
  platform's own workbook guide independently recommends the shape: *"a frame-count predicate like
  `simulation.getFrameNumber() >= N` makes a run reproducible, where a fixed Duration delay depends
  on wall-clock timing."* That is corroboration for CR-EX-3's [ORIGINATED] wall-clock prohibition
  from the platform vendor rather than from this project alone. **It is not a decision.**
- **A trap for OQ-1, found and worth carrying to M2.** `n8ro-bridge` offers
  `--capture-max-samples`, a record-count bound not mentioned in `PROVENANCE.md` finding 8 or in
  PRD rev 2. It is attractive and it is wrong as a stop predicate: sample counts are precisely the
  quantity that varies run to run (§4), so two runs stopped at the same sample count have stopped
  at **different simulation frames** — the failure CR-EX-3 exists to prevent. It is a safety bound,
  not a definition of the end. It is a genuine input to **OQ-6**.
- **It did not run the determinism comparison.** That is M4, gated on OQ-2.
- **It did not confirm the invocation.** That is OQ-3 — drafted in `docs/escalations.md`, and now
  with a concrete addition M1 discovered: the `N8RO_RELEASE` requirement, which changes the
  invocation as `PROVENANCE.md` records it.
- **It observed one run.** Nothing here is a claim about run-to-run behaviour.
