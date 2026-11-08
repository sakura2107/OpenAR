# C++ reaction loop API

`ar/include/loop/Loop.h` and `ar/src/loop/` provide an alternative to JSON pipelines. They use the existing `DeviceSession::capture/perform`, frame generation validation, native matcher/OCR and `TraceWriter`. `Resource`, `Tasker`, JSON `next/on_error` and their existing execution semantics are unchanged.

```cpp
using namespace ar::loop;
Options options;
options.trace_root = "out/traces";
options.full_frames = true; // required for exact decision replay
Context ctx(session, "templates", "ja", options);
ctx.recover(Phase("anchor recovery")
    .goal(see("screen/anchor", roi(10, 10, 200, 50)))
    .on("back", see("button/back", roi(0, 0, 100, 80)), key(4)));

auto task = Task("navigation example")
    .done(completed("target"))
    .phase(Phase("target")
        .goal(see("screen/target", roi(100, 0, 300, 60)))
        .on("open", see("button/open", roi(100, 500, 160, 80)), tap())
        .timeout(30s).max_frames(250))
    .timeout(90s).retries(2);
auto result = task.run(ctx, stop_token);
```

Template `button/open` resolves to `<template-root>/<server>/button/open.png`. An in-memory asset can be supplied with `ctx.asset(name, mat)` for tests. A `Roi` can override coordinates using `.server("other", cv::Rect(...))`. Images and regions must fit the session reference size. Missing ROI, a full-screen ROI without `full_screen(reason)`, invalid numeric expectations and missing effect confirmations fail before device capture.

`Phase.run` runs one bounded phase and reports its result. `Task.run` owns the completion check, recovery and restart policy. Definitions and Context must not be mutated while a run or replay is active.

Conditions: `see`, `gone`, `not_`, `any`, `all`, `ocr_text` (exact UTF-8 text), `ocr_number` (strict typed numeric comparison), `completed`, and `custom(name, roi, pure_predicate)`. Configure native OCR through `ctx.ocr(params, backend)`; OCR uses `recognize_crop_checked` and caches results per frame/ROI/channel. Nonmatching OCR strings are retained in trace. Numeric comparison preserves operators: `6/6` is not `66`, `x1` is not `1`, and `15%` is not `15`.

`completed(phase)` is a task-local completion marker, not an image search. It persists across recovery/retries and resets on a new `Task.run`. A visual completion predicate should be used for operations that may already have been completed before the process started. By default recovery restarts ALL phases from the beginning, so required navigation is repeated; their goals still run before any action. Only phases explicitly marked `.checkpoint()` are skipped after completed work remains valid across anchor recovery. Do not checkpoint a navigation prerequisite.

## Decisions and input

Each phase retains a do-while reaction loop. Capture through `DeviceSession`, evaluate stability (busy frames can proceed immediately), evaluate the goal and explicit stop guards, resolve effect/settle state, then apply busy, global and phase priority. A goal may complete during busy; the next phase takes a fresh frame and busy still prevents input. Only the first matching rule executes. The next action always requires a new frame. A stop guard can stop even when a background goal anchor is visible beneath a modal; no input is made in that case.

`tap()` uses the recognition box centre. Explicit `tap_at(point, reason)` is allowed only as a non-global delayed recovery fallback with `.after_unknown(duration)`, `.expect(...)` and `.max_attempts(...)`; startup validates its finite bounds. Normal actions still reject arbitrary fixed targets. In compound conditions, the first positive visual match supplies that box; put the element to tap first. A `gone`, checkpoint or boolean custom predicate cannot independently supply a tap target. `key(code)` is still gated by a visual ROI condition.

```cpp
Phase("submit")
    .goal(see("screen/submitted", roi(100, 20, 300, 60)))
    .on("submit once", see("button/submit", roi(400, 500, 160, 70)),
        tap().effect().expect(gone("button/submit", roi(400, 500, 160, 70)), 5s));
```

The effect attempt is recorded BEFORE invoking `perform`. Lost acknowledgements still enter confirmation. While confirmation is pending, ordinary phase rules cannot execute; declared safe global rules may execute. Confirmation timeout is `outcome_unknown`, not a permission to retry. The attempt ledger survives recovery. Even a matched phase goal cannot bypass an outstanding explicit confirmation. Global `.effect()` rules are rejected at registration.

Limits: phase timeout/max captures, rule execution limit (default5, independent of image differences), unknown-time budget `.unknown_after(8s)`, task total timeout and maximum recoveries (default2). `.stuck_after(n)` now bounds executions per rule per Phase invocation, including non-consecutive hits; n is1–5. `.max_attempts(2)` can tighten one rule. Busy does not consume unknown time; phase/task clocks continue. Positive UI recognition resets the unknown interval. Pure background safety predicates do not count as a known UI anchor. Cancellation is checked during capture/stability polling, between recognition calls and before/after input. A single native recognition call is not preempted; cancellation completes within the current frame/operation. Task and JSON pipeline runners share the same in-process device run lock.

## Navigation settle and busy (B2b)

```cpp
ctx.busy(see("common/loading", roi(900, 600, 350, 100)));
Phase("open")
    .goal(see("page/title", roi(90, 0, 330, 55)))
    .on("entry", see("home/entry", roi(150, 660, 140, 50)),
        tap().expect(see("page/title", roi(90, 0, 330, 55))).settle(2s))
    .unknown_after(8s);
```

Every phase navigation action opens a settle window (default2s). Phase rules are suppressed until goal, expect, trigger-gone or timeout. Safe globals may still run, except while busy. Trigger-gone is only evaluated on non-busy frames: an overlay can hide a button without proving navigation completed. Goal/expect and elapsed deadlines still apply during busy. Navigation timeout permits another attempt, with a hard maximum of five per rule in a Phase. Side effects retain their separate one-shot confirmation ledger and never retry an unknown result.

`ctx.busy(condition)` accepts composable ROI conditions. A busy frame is `$busy`, makes no input (including globals), and pauses unknown time. A matched goal can complete the phase during busy. All recognition predicates must be pure so replay uses the same state machine.

## Recording and replay

Every capture, including intermediate stability frames, is recorded. Default successful images are thumbnails (maximum width640); failed frames always retain full canonical and original images under `failures/`. Set `full_frames=true` to retain lossless canonical images suitable for exact replay. Thumbnail-only traces reject exact replay instead of silently comparing resized pixels.

`task.replay(ctx, trace_directory, observer)` does not call `capture`, `connect` or `perform`. The optional observer receives each `ReplayDecision` (frame ID, phase, recorded/recomputed rule and equality), without writing into the original trace. Replay re-evaluates stability and conditions on recorded frames, reproduces goal/global/phase priority, confirmation windows and the effect ledger, and compares selected rules, input types/keys/targets and B2b busy/settle/attempt/unknown fields. B2b frames carry `loop_revision=2`; legacy B2 traces explicitly require their archived runner for exact replay. Recorded frame timing is used; it does not simulate new device latency. Recognition changes that change decisions are reported as replay mismatches. Custom predicates must be pure for this guarantee.

`task.project(ctx, trace_directory, observer)` is separate counterfactual analysis. It ignores old phases/decisions, includes the initial task completion check, and advances the new task over old full-size images and capture timestamps without device calls. It emits one JSON row per source frame. Suppressed inputs would alter future device images, so projection is a fixed-sequence hypothesis, not a causal simulator or proof of live success.

The one-shot ledger is scoped to one running task and its recovery attempts. Automatic process-crash resume is not implemented. A restarted process must perform the script's business completion checks rather than treating a new process as an authorized repeat of an uncertain effect.

The deterministic `loop/Simulator.h` supplies image states and click/key edges, seeded popups, delayed transitions, lost acknowledgements, ignored actions, loading stalls and animation. B2b adds a millisecond-delayed transition with an optional loading overlay, timed black loading, and a one-shot hidden-UI state injection; all are generic image-state faults. It implements the existing controller contract only for offline tests; production captures and actions continue through `DeviceSession` and the existing device plugins.

Event callback/JSON Lines contract: [LOOP_EVENTS.md](LOOP_EVENTS.md). Generic contract tests: `tests/LoopTests.cpp`.

## Task-wide action budgets

`tap().task_budget("recovery-wake", 2)` shares a named budget across all Phases and recovery attempts in one Task.run. The attempt is counted before perform, including lost acknowledgements. Exhaustion selects `$budget.exhausted/<key>`, returns blocked and stops the task without entering recovery again. Ordinary navigation still has its per-Phase5 limit; side effects still retain their one-shot ledger. A new Task.run clears the budget. This is a generic limit; BA names and wake behavior remain in user scripts.

The trace's `task_attempts` object records these shared counts. Exact replay compares it and requires it once a configured budget is used. Use the archived runner/configuration to replay a trace recorded before adding a task budget rather than silently claiming the new policy would have followed the old recovery sequence.


## Required navigation confirmation and timed simulator states (B2c)

`tap().expect(target, 2s).settle(2s).require_confirmation()` is an opt-in phase-local navigation stop. Unlike ordinary navigation, disappearance of the trigger does not end its settle window. If neither the phase goal nor the declared target appears before the settle deadline, `$nav.unconfirmed/<rule>` returns `blocked`; Task.run does not recover or retry. This is still navigation and does not enter the side-effect ledger. The deadline is the settle window; use matching expect/settle durations for a single strict deadline. Strict confirmation requires an expect condition and is rejected for globals. Exact replay and counterfactual projection use the same stop decision.

`SimState::expires_to` and `expires_after_ms` model a transient image state. The simulator moves to the expiry target on the first capture at/after its lifetime. Entering the state again restarts the lifetime. Both fields must be supplied together with a valid target. Business state names and rules remain outside OpenAR.
