# UI Refactor Progress

This file tracks the UI responsiveness + architecture refactor in phases.

## Goals

- Improve layout responsiveness in normal and debug modes.
- Stop mixing UI code with emulator execution flow.
- Preserve emulator behavior and debug tooling fidelity during refactor.

## Rules for Every Phase

- Build with `build.bat`.
- Run `build\nes.exe` for a short smoke test.
- Capture one screenshot showing the current state.
- Wait for user approval before moving to the next phase.

## Phase Plan

### Phase 1 - Introduce App/UI/Control state containers

- [x] Add explicit `AppState` with sub-structs for runtime, emulator control, and UI state.
- [x] Replace file-level globals usage with `AppState`-backed aliases (no behavior change).
- [x] Move persistent UI locals (debug toggles, text buffers, panel options, audio toggles) into `UiState`.
- [x] Build + run smoke test + screenshot.

### Phase 2 - Extract emulator loop and input mapping from UI section

- [x] Move emulator stepping/timing logic into runner functions.
- [x] Move keyboard/controller mapping into dedicated input function(s).
- [x] Keep exact run/pause/step/breakpoint behavior unchanged.
- [x] Build + run smoke test + screenshot.

### Phase 3 - Extract UI panels into dedicated functions/files

- [ ] Split current monolithic UI drawing into panel functions.
- [ ] Keep layout and behavior identical first (no redesign yet).
- [ ] Build + run smoke test + screenshot.

### Phase 4 - Implement responsive workspace layout

- [ ] Replace hard-coded panel coordinates with adaptive layout.
- [ ] Keep NES screen aspect-correct letterboxing.
- [ ] Organize debug panels into coherent groups/tabs.
- [ ] Build + run smoke test + screenshot.

### Phase 5 - Separate debug view rendering helpers

- [ ] Extract video/audio visualization generation from widget code.
- [ ] Reduce duplication in waveform and tile/sprite rendering paths.
- [ ] Build + run smoke test + screenshot.

### Phase 6 - Optional deeper decoupling of emulator and GUI buffers

- [ ] Evaluate moving debug GUI buffers out of `NES` core state.
- [ ] Preserve or intentionally migrate save-state compatibility.
- [ ] Build + run smoke test + screenshot.
