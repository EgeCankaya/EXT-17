# OQ-6 — the disk ceiling, what happens on reaching it, and which per-run bound to configure

**Status: DECIDED at M3.** Both halves. The decision is written into `README.md`, into
`n8ro-campaign --help`, and into the defaults of the binary.

**Decision, in three parts**

> 1. **The campaign's ceiling is `8 GiB` over the whole campaign directory** — captures *and*
>    logs — checked against free space before run 1 and against actual usage after every run.
>    `--disk-ceiling-bytes`.
> 2. **Reaching it stops the campaign with a named outcome.** Every run already completed stays
>    valid and readable; the campaign summary is written; the exit code is 1.
> 3. **The per-run upstream action is `stop`**, and every run is given a per-capture byte bound
>    by default — `61 000 × --frames`, three times the measured per-frame capture cost — so the
>    bound exists on every run and does not fire on a normal one.

---

## Why this needed measurement rather than an argument

PRD rev 2 gave OQ-6 two decisions and named `stop` as the leading candidate on two grounds: *"a
campaign that needs the tail of a run has mis-set its stop predicate, and one file per run keeps
CR-CAP-4's segment handling simple."* Both are good arguments. Neither is a measurement, and
**nothing had ever exercised rotation** — `n8ro-campaign` passed `--capture-max-bytes` and
`--on-size-limit` through to the recorder and no run had ever used them.

So M3 ran both. Three probes, `campaigns/m3-oq6/`, 1200 frames each, recorder attached:

| probe | `--capture-max-bytes` | `--on-size-limit` |
|---|---|---|
| `unbounded` | 73 200 000 (the derived default; never reached) | `stop` |
| `stop` | 8 000 000 | `stop` |
| `rotate` | 8 000 000 | `rotate` |

Every one of them was read back with M3's own reader, and all three captures are conformant.

## What the probes measured

| | `unbounded` | `stop` @ 8 MB | `rotate` @ 8 MB |
|---|---:|---:|---:|
| capture files | 1 | 1 | **4** |
| capture bytes | 24 297 928 | 7 992 128 | 24 327 654 |
| `trailer.end_reason` | `host_lost` | **`size_limit`** | `host_lost` |
| covers the whole run | yes | **no** | yes |
| samples | 50 401 | **16 626** | 50 449 |
| `(part, segment)` keys | 2 | 1 | **5** |
| segments in the *run* | 2 | 1 | 2 |
| `entity_add` / `entity_remove` | 89 / 47 | 44 / 2 | **89 / 47** |
| run directory bytes | 29 788 003 | 13 461 342 | 29 819 897 |
| conformant | yes | yes | yes |

Three things fall out, and the third is the one that decided it.

### `rotate` works, completely, and costs nothing in fidelity

Four parts, every one a complete and independently valid capture with its own header, schemas and
trailer. The parts link correctly in both directions, the reader stitches them per §6.7, and the
stitched set reproduces the roster lifecycle of an unrotated run **exactly** — `entities_added: 89`,
`entities_removed: 47`, the same figures all twenty of M2's runs produced. Rotation is not lossy
and it is not fragile. The `rotate` column above is a working feature, not a caveat.

It costs about 30 KB of extra header and trailer across the run, which is 0.1%.

### `stop` loses the tail, and — since M3 — says so in three places

The `stop` probe recorded to `sim_time_s` **19.5 of 60.0**: a complete, valid, conformant capture
of the first third of a run. That is exactly what the format promises (§6.6) and it is not a
defect.

What *was* a defect is that the run reported `completed` and nothing outside the file said the
capture covered a third of it. M3 fixed that: `n8ro-campaign` now reads back the capture it just
produced, with its own reader, and the run record carries

```json
"capture": { "end_reason": "size_limit", "covers_whole_run": false, "conformant": true, ... }
```

and the campaign log says it in words. A stored run that M6 will later judge now states its own
coverage, because judging a third of a run and reporting it as a whole one is precisely the
failure tenet 1 exists to prevent.

### The deciding cost of `rotate` is structural, and the specification understates it

**One run's two segments became five `(part, segment)` keys.**

The run's single *running* segment was cut three times, so it appears as four keys — `(0,0)`,
`(1,0)`, `(2,0)`, `(3,0)` — every one of them reporting itself as "segment 0". Nothing inside any
part says those four are fragments of one segment. `(3,1)` is the teardown segment, the fifth key.

Two consequences that land squarely on M4 and M6:

- **A cut duplicates its boundary instant.** Part 0's last sample and part 1's first sample both
  carry `sim_time_s` `19.450000000000141`. A comparison that concatenates parts and assumes
  disjoint time windows will meet the same simulation instant twice, which is the shape of the
  alignment error CR-DET-1 spends its whole design avoiding.
- **`counts.segments` cannot be summed.** §6.7 says of a trailer's counts: *"The run's totals are
  the sum across parts."* For samples, adds, removes and verdicts that is true. For `segments` it
  is **false**, because a cut segment is closed in one part and opened in the next: the four-part
  probe sums to **5** for a **2**-segment run. This is an imprecision in the contract, and it has
  gone back to EXT-08 as **E-3** rather than being worked around quietly. The reader implements
  what §6.7 says *and* reports what is true, and names the gap:

  ```
    counts        summed across parts: segments 5 samples 50449 adds 89 removes 47 verdicts 0
    segments      5 summed across parts, but the RUN has 2: 3 segment(s) were cut by a rotation
  ```

  `runSegments` is derived from §6.7's own rule for recognising a cut — a part whose last segment
  closed with `size_limit` and which carries a `continued_in` — so it needs no heuristic.

## The decision, and what actually decided it

**`stop`.** Not because it was simpler, and not because rotation failed — it did not; it works
completely. Because of this:

> A capture whose bound fires is a capture that has told you something. With `stop` the thing it
> tells you is *"this run overran its projection"* — one fact, in one file, in one field, with
> `covers_whole_run: false` beside it. With `rotate` the thing it tells you is *"this run's
> segments are now fragments and their ordinals lie"* — which is not a fact about the run at all,
> and which every statistic downstream has to be written around whether or not any run ever
> rotates.

`rotate` moves a cost from the rare case into the common one. Once it is configured, **every**
per-segment statistic in M4 and M6 must key on `(part, segment)`, must know that adjacent keys may
be one segment, and must not trust a summed segment count — even on the nineteen runs out of
twenty that never rotate. `stop` keeps that cost at zero and pays it only where a run actually
overruns, which is a run that has already gone wrong.

And the PRD's original argument survives contact with the measurement: a campaign that needs the
tail of a run has mis-set its stop predicate. The predicate is a frame budget (OQ-1). If a capture
reaches its bound before frame N, the *bound* is wrong, not the run — and that is an operator
signal, not a data-recovery problem.

**The reader keeps its `rotate` support regardless**, tested against this real four-part capture
and against synthetic parts. `stop` is a choice about what the campaign configures, not about what
the reader can read — a capture rotated by somebody else still has to be readable here.

## The ceiling, and the numbers behind it

**Measured, not projected.** One 1200-frame run costs **29 788 003 bytes of campaign directory**:

| | bytes | |
|---|---:|---|
| the capture | 24 297 928 | 81.6% |
| `host-logger.log` | 2 849 933 | this run's slice of the host's shared log |
| `host.err` | 2 559 540 | the terrain-error flood the install is expected to produce |
| `host.out`, `recorder.out`, `run.json` | 80 602 | |
| **total** | **29 788 003** | **24 823 bytes per frame** |

**The ceiling is over the directory, not over the captures, and that is the point.** A projection
built from capture size alone under-states a campaign by **22.6%** — the logs are not incidental,
they are a fifth of the footprint, and they scale with frames exactly as the capture does. A
campaign that passed a capture-only pre-flight check could still exhaust the disk on logs, which
is the failure CR-CAP-5 exists to prevent.

Cross-checked against M2's twenty runs: **595 590 013 bytes** for the campaign directory against
**486 359 759** of captures — the captures are 81.7% of it, and a projection from them alone
under-states the directory by **22.5%**. The two measurements agree to a tenth of a percent.

**At [B]'s scale.** A 200-second run is 4 000 frames: **~99 MB**, and a twenty-run campaign is
**~1.99 GB** — against the ~1.6 GB the captures alone would suggest, and against the PRD's
inherited estimate of ~1.3 GB.

**So the ceiling is 8 GiB**, four times [B]'s scale. Large enough that no legitimate campaign the
PRD describes meets it; small enough to catch a runaway long before a 318 GB volume is in danger.
`--disk-ceiling-bytes 0` disables it, deliberately and visibly.

**The projection is `--bytes-per-frame`, default 25 400** — the measured 24 823 with a 2% margin.

### Both halves of the check, and why the second is not redundant

- **Before run 1**, the projection is compared against the ceiling and against free space. A
  refusal names *both* numbers, because "not enough space" without them is a message nobody can
  act on. Verified: a 20-run 1200-frame campaign against a 100 MB ceiling refuses with
  `exit 2` and **no run directory is created at all**.
- **After every run**, actual usage of the campaign directory is compared against the ceiling.
  This is not redundant with the projection: a projection is an estimate, and a run that
  overruns — the exact case the per-capture bound exists for — is a run whose estimate was wrong.
  Verified: four runs asked for against a deliberately low ceiling stopped after run 001 at
  15 421 214 bytes of a 10 000 000 byte ceiling, `exit 1`, campaign summary written, and **both
  completed runs read back conformant** afterwards.

## What this decision commits M4 and M6 to

- **One capture file per run, one `(part, segment)` key per segment.** M4's alignment and M6's
  assertions may assume `part == 0` *in the campaigns this project runs* — and must still key on
  the pair, because the reader will be handed rotated captures from elsewhere and CR-CAP-4 says
  the pair is the key everywhere it is used.
- **`covers_whole_run` is a precondition, not a footnote.** A capture with `end_reason:
  "size_limit"` and no `continued_in` covers part of its run. M4 must not compare two runs where
  either says so, and M6 must not report a verdict over one without saying which part of the run
  it judged.
- **The ceiling is a campaign outcome, not a run outcome.** A campaign stopped at its ceiling has
  runs that all completed; what did not happen is the rest of the campaign. It is named in the
  log and carried in the exit code, and it never becomes a failing run.
