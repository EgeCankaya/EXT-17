# The referee's condition-file schema

**Vendored from EXT-08 at commit eedc228.** This is the reference shape, not a standard:
PRD OQ-6 resolved that it was designed for EXT-08's own needs and documented fully, and that
EXT-17 may **adopt it or supersede it**. An over-designed schema is harder to supersede than a
simple documented one, so this one is deliberately small.

What is worth inheriting regardless of whether the file format is:

- **A malformed condition file is a named parse error and a non-zero exit BEFORE anything
  starts.** Never a silent zero-condition run. A campaign that reports "all passed" because it
  quietly loaded nothing is the failure this rule exists to prevent.
- **The vocabulary is closed.** An unrecognised condition kind is an error, not a skipped
  condition.
- **One verdict per condition per run**, and a condition never met produces an explicit
  not-met verdict rather than silence — because silence is also what an unevaluated condition
  looks like.

---

### Declaring conditions

The vocabulary is **closed at three kinds**. A fourth is a named parse error and a non-zero
exit before any subscription is made — never a silently skipped condition, because a run that
reports "all passed" after quietly dropping the one that mattered is the failure this design
exists to prevent.

```json
{
  "conditions": [
    {"id": "red-leader-reaches-airfield", "kind": "proximity",
     "entities": ["RedUAV_N_01", "BlueBase_Airfield"], "within_m": 3000},

    {"id": "red-leader-enters-base-circle", "kind": "area", "entity": "RedUAV_N_01",
     "test": "inside",
     "region": {"shape": "circle", "centre": [-23.49849, -68.25173, 7.5], "radius_m": 3000}},

    {"id": "red-leader-crosses-corridor", "kind": "area", "entity": "RedUAV_N_01",
     "region": {"shape": "polygon",
                "vertices": [[-23.47, -68.29], [-23.47, -68.23],
                             [-23.52, -68.23], [-23.52, -68.29]]}},

    {"id": "red-leader-is-destroyed", "kind": "terminal_state",
     "entity": "RedUAV_N_01", "removal_reason": "destroyed"},

    {"id": "airfield-reaches-operational", "kind": "terminal_state",
     "entity": "BlueBase_Airfield", "field": "phase", "equals": "operational"}
  ]
}
```

A working file is committed at
[`conditions/atacama.conditions.json`](conditions/atacama.conditions.json).

| key | applies to | meaning |
|---|---|---|
| `id` | all | Stable identifier, unique in the file. It is what the verdict is traced by, so a duplicate is a parse error |
| `kind` | all | `proximity`, `area` or `terminal_state`. Anything else is a named parse error |
| `entities` | proximity | Exactly two entity names. Naming the same one twice is rejected — it is met at distance zero |
| `within_m` | proximity | Threshold in **metres**. The comparison is `<=` |
| `entity` | area, terminal_state | One entity name |
| `test` | area | `inside` (default) or `outside` |
| `region.shape` | area | `circle` or `polygon` |
| `region.centre` | circle | `[latitude°, longitude°, altitude m]`. Altitude may be omitted and defaults to 0. `center` is accepted too |
| `region.radius_m` | circle | Radius in **metres**, positive |
| `region.vertices` | polygon | At least three `[latitude°, longitude°]` points |
| `removal_reason` | terminal_state | Matched **verbatim** against `entity_remove.reason`. The platform's vocabulary is open, so a supplier-specific reason this build has never seen still matches |
| `field` + `equals` | terminal_state | Matched against a sample's field value. Use one form or the other, never both |

Any key the loader does not recognise is ignored, which is what lets a `_comment` live in the
file. Units are the platform's own and are never converted: metres, degrees, and the
platform's `[lat, lon, alt]` order.

### Verdict semantics

**One verdict per condition per run.** At the first moment it is satisfied, or an explicit
`met: false` at end of run. It is not re-emitted on every later sample that also satisfies it —
"did the two aircraft come within 5 km" is answered by the first time they did.

**The not-met verdict is the load-bearing half.** Without it, a condition that was evaluated
and never satisfied is indistinguishable from one nobody evaluated.

A verdict carries enough to find the samples that caused it. A proximity verdict names both
entities, each one's **occupancy**, each one's sample `sim_time_s`, and the computed distance —
which is exactly the key needed to locate the two causing records in the capture.

```
red-leader-reaches-airfield  met  t=149.05  distance_m=2999.9981116642175  within_m=3000
red-leader-is-destroyed      met  t=149.45  removal_reason=destroyed
command-centre-is-destroyed  NOT MET
```

