# Loop event stream v1 · frame revision2 (B2b)

Register `ctx.events(callback)`; the callback receives one complete UTF-8 JSON object per call (append a newline in a CLI). It must return promptly. Schema: `schemas/loop-event.schema.json`.

```json
{"schema_version":1,"type":"loop.frame","run_id":"...","phase":"target","frame_id":7,"time_ms":230,"data":{"schema_version":1,"event":"loop.frame","loop_revision":2,"busy":false,"settle_until":2230,"settle_reason":"","settle_rule":"open","rule_attempts":{"target/open":1},"retry_count":0,"unknown_ms":0,"unknown_limit_ms":8000,"phase_deadline_ms":10000,"kind":"decision","selected":"target/open","goal_evaluated":true,"goal":{"matched":false,"score":0.2,"text":"","box":[0,0,0,0]},"rules":[{"name":"target/open","evaluated":true,"result":{"matched":true,"score":0.99,"text":"","box":[100,500,40,30]}}],"action":{"type":"tap","side_effect":false,"keycode":0,"target":[120,515]},"action_result":"accepted","timing":{"capture_ms":3.1,"stable_wait_ms":82,"recognition_ms":1.2,"action_ms":2.3,"trace_image_ms":12},"image":"frames/7.png","replayable":true}}
```

Stable event types: `loop.task.start`, `loop.task.end`, `loop.phase.start`, `loop.phase.end`, `loop.phase.skip`, `loop.frame`, `loop.recovery`, `loop.error`. Non-frame events carry a `data.status` string. Terminal task statuses: `success`, `already_done`, `failed`, `cancelled`, `outcome_unknown`, `blocked`.

`time_ms`, `capture_clock_ms`, `decision_ms`, `action_started_ms` use the session's monotonic clock; `utc_unix_ms` is the record-time UTC stamp. `frame_id` and `generation` bind a decision to the existing Frame contract. Stability sampling events use `kind=stabilizing`, `selected=$stabilizing`, and `goal_evaluated=false`; they make no input. `stable_capped` indicates recognition proceeded at the maximum wait.

`rules` are in priority order. Entries with `evaluated=false` were short-circuited and have no fabricated match/score. Goal completion may skip the entire rule list. `result.raw_ocr` retains native OCR items even when they do not satisfy the expected text/number. Composite rule score/box come from its first positive visual child; they are not a calibrated joint probability.

`selected`: normal IDs are `global/<rule>` or `<phase>/<rule>`. Control labels include `$goal`, `$wait`, `$busy`, `$settle`, `$confirm.wait`, `$confirm.ok`, `$confirm.unknown`, `$effect.already_attempted`, `$stuck/<rule-id>`, `$blocked/<guard>` and `$stabilizing`. Confirmation observations appear under `confirmation/<rule>` in `rules`. No `action` means no input was attempted. An `action_result=unknown` and optional `action.error` record a failed/lost acknowledgement, without claiming the action did not happen.

`timing.capture_ms`, `recognition_ms`, `action_ms`, `trace_image_ms` are wall-clock measurements. `stable_wait_ms` uses the session clock; do not sum its cumulative intermediate samples as separate delays. A simulated ManualClock advances logical time without sleeping the host. Recognition, PNG encoding and scheduling remain measurable wall time.

The existing `TraceWriter` envelope is unchanged: `events.jsonl` uses its original `type/node/frame_id/time_ms/details` fields; `details` is the encoded v1 `data` object. The CLI callback adds the v1 outer envelope above. B3 can consume callback JSON Lines directly or decode `details` when reading saved traces.

## B2b frame state

All new frames require `loop_revision:2`; the event envelope remains v1. `settle_until` is the session-clock deadline, -1 before navigation; `settle_rule` names the outstanding navigation rule. `settle_reason` is empty or `goal/gone/expect/timeout`, emitted on its terminating decision (and retained on intervening stability samples). `busy` describes input suppression. `rule_attempts` maps full rule IDs to execution counts in the current Phase invocation, max5. `retry_count` is last attempted rule's count minus one (0–2). `unknown_ms` accumulates unknown elapsed time, excluding busy and settle; `unknown_limit_ms` records the configured duration. `phase_deadline_ms` preserves the actual deadline used for exact replay.

Replay compares these fields in addition to rules and input coordinates. Bad field types return an error. Legacy frames lack revision2 and are not silently judged by the new policy. Keep their original event schema/runner; use projection when deliberately changing the decision policy.

`task_attempts` is an additive map of named action counts shared across Phase/recovery boundaries. `$budget.exhausted/<key>` is terminal blocked and makes no input. When a task budget is configured and used, missing or changed shared counts reject exact replay. Earlier B2b traces remain associated with their recorded runner/configuration.


B2c raises the ordinary per-Phase rule execution ceiling to 5 (`retry_count` maximum 4). The event envelope and frame revision remain unchanged. `$nav.unconfirmed/<rule>` is a terminal blocked navigation decision, has no input, and prevents Task recovery. It is emitted when an opt-in strict navigation settle window ends without its expected destination. Counts in live reports are informational; task completion, busy suppression, side-effect uniqueness, navigation-only scope and exact replay determine acceptance.
