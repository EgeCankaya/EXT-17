# Escalations and questions out

One row per question that leaves this project. A question is only ever marked **Answered** when
a reply exists; "asked" and "answered" are separate columns because conflating them is how a
project ends up believing it has a ruling it never received.

| # | Question | To | Raised | Status |
|---|---|---|---|---|
| E-1 | **OQ-3** — is this the intended production invocation of the headless host? | Mentor | 2026-08-31 (M1) | **Drafted, not yet sent.** Extended at M2 with (e) and (f) below. M2 built on the bus-publish route deliberately and says so — see "Why the answer is worth having" |
| E-2 | **OQ-2** — is the determinism gate keyed on content or on bytes? | Owner of [B] | 2026-08-31 | Sent by EXT-08 as its E-1; awaiting reply. This is the downstream half |

---

## E-1 — OQ-3: confirm the headless invocation

**Status: drafted at M1, awaiting sending.** [B]'s surface table asks its reader to confirm this
directly — *"the host binary that runs an engine with no GUI. **Confirm the invocation with your
mentor**"* — and EXT-08 closed its own copy of the question and passed it here, on the grounds
that EXT-17 is the project that runs the host in production.

It does not block. The invocation below is known to work: it ran one full scenario to a frame
budget on 2026-08-31 (`docs/m1-lifecycle.md`). What is not established is that it is the
**intended** shape, and the binary cannot settle it — `n8ro-sim-app.exe` has no `--help` and no
usage text.

### The question

> Is this the intended production invocation of the headless simulation host?
>
> ```
> set N8RO_RELEASE=C:\N8RO
> n8ro-sim-app.exe --sim-config SimEngineHost_SharedMemory ^
>                  --model-path C:\N8RO\data\db ^
>                  --schema-file N8roSimSchema
> ```
>
> It takes **no scenario argument**. Loading is a separate publish of
> `{"command":"load_scenario","scenarioName":"…","modelName":"…"}` on `sim/scenario/command`, and
> starting is `{"command":"start"}` on `sim/engine/command`. Verified working end to end at M1.
>
> Six specifics, in the order they would change what EXT-17 builds. (a)–(d) were drafted at M1;
> (e) and (f) were added at M2, from what automating the run measured.
>
> **(a) Is the bus-publish route the intended control path at all?** [B]'s surface table also
> offers *"the scenario-control and simulation scripting namespaces"*. EXT-17's PRD declines the
> scripting route (Option 5) and builds on the bus. M1 found the two are the same mechanism —
> `scenarioControl.requestLoadScenario` is documented as *"publishes the command"* — but they
> differ in **where** the command is applied: published from outside the pipeline it takes effect
> during message processing, while a script call from inside is queued to the next frame boundary.
> EXT-17 wants the outside-the-pipeline path. Confirm that is right for unattended use.
>
> **(b) Is `N8RO_RELEASE` expected to be set for production runs?** M1's first attempt did not set
> it, and **every scenario load was refused**: with it unset the host resolves its plugin
> directory from the current working directory, skips the plugin scan, never registers
> `componentPhysics` (supplied by the stock `bin\plugins\sim\n8ro-physics.dll`), and refuses any
> scenario whose entities need it. The host does **not** fail — it sits idle indefinitely, which is
> the dangerous shape for an unattended campaign. Setting `N8RO_RELEASE=C:\N8RO` fixed it. Is that
> the intended provisioning, or is there a supported way to run without it?
>
> **(c) Is `SimEngineHost_SharedMemory` the right transport for unattended campaign use?** The
> install ships seven other `SimEngineHost_*` entries, including
> `SimEngineHost_SharedMemory_BestEffort`. EXT-17 runs host and observers as separate processes on
> one workstation, twenty-plus times in a row, and depends on the published stream being as
> complete as the platform can make it — so a best-effort variant looks actively wrong here, but
> that is inference, not confirmation.
>
> **(d) Is the degraded terrain configuration expected on this install?** Every run floods with
> `TerrainElevationServiceClient` / `GeoidGridModel` errors: there is no elevation service running
> and no geoid grid under `C:\N8RO\data\geoid`. EXT-17 is **deliberately not fixing this** — every
> measurement it inherits from EXT-08 was taken in this configuration, and provisioning terrain now
> would invalidate the comparability of all of it. Confirming that this is the expected state of
> the install, rather than a defect, would let that decision stop being a judgement call.
>
> **(e) Is the host expected to append to a shared log inside the install tree, and is there
> really no shutdown command?** Two things measured at M2 that a campaign has to work around.
> `C:\N8RO\logs\n8ro-logger-n8ro-sim-app.log` is one fixed filename that every host process
> **appends** to — two identical runs produced a file of exactly twice one run's size, carrying
> two startup banners — so a twenty-run campaign accumulates all twenty in one file inside a tree
> it treats as read-only. EXT-17 works around it by recording the file's size before each host
> start and copying only the bytes after it, which is correct but is a workaround. Separately,
> `sim/engine/command`'s vocabulary is closed at `start` / `stop` / `pause` / `step`, so there is
> no way to ask the host to exit; EXT-17 sends a `CTRL_BREAK_EVENT` to the process group it
> created, which shuts the host down in an orderly way (the `.running` marker is removed, no
> crash rename follows) but still reports exit `0xC000013A`. **Is a console control event the
> intended way to end an unattended host, and is a non-zero exit code from it expected?** The
> answer changes how CR-EX-5 tells "the host died" from "we stopped it".
>
> **(f) `C:\N8RO\bin` on `PATH` is a second precondition, separate from `N8RO_RELEASE`.** An
> SDK-linked binary run from any other working directory exits 53 with no output — a missing
> `n8ro-sim.dll`. It is folded into (b): confirming the intended provisioning should cover both.

### Why the answer is worth having even though nothing is blocked

CR-EX-2 marks the invocation **[ORIGINATED]** precisely because [B] does not specify it. If the
answer to (a) is "the scripting namespaces", CR-EX-2's acceptance criteria change shape and
Option 5 has to be reopened. If (b) has a supported alternative, the campaign's per-run
environment setup changes.

**M2 has now built on the current assumption, deliberately, with the cost stated rather than
discovered.** The whole control path is one component — `src/control/EngineControl` — and every
wait, publish and observation the campaign makes goes through it. An answer of "the scripting
namespaces" to (a) is therefore one file to rewrite, not a redesign. That was the reason for the
shape, not a coincidence. The rest of (a)–(f) change configuration and documentation rather than
structure. The answer is still cheaper now than after M4 keys a determinism gate to runs produced
this way.
