# Generic application job host (B3)

`ar::host::JobHost` (`host/JobHost.h`) registers an immutable device/task catalog and an application-owned Executor. It adds host control around either reaction-loop or other existing runners; it does not implement recognition or device input. `RunContext` supplies cooperative stop, event publication and confined trace association.

One worker serializes each device queue; independent devices progress concurrently. Optional stable device lock keys use Windows named mutex leases across JobHost owners. The state directory is exclusively locked, snapshots are atomically replaced, and prior queued/running jobs become interrupted at restart. No crash resume or side-effect repetition is inferred. The underlying DeviceSession and reaction-loop confirmation ledgers remain authoritative.

Job states are queued/running/succeeded/failed/cancelled/blocked/outcome_unknown/interrupted. Terminal metadata can be explicitly forgotten without deleting trace files. Submission request keys deduplicate an identical invocation across restart; changed invocation data conflicts. Retained jobs and per-device queue lengths are bounded. Persistence failures are visible; a failed terminal metadata write cannot roll back an already-executed action.

The event journal has process-scoped sequence cursors, an instance ID, count/byte bounds and explicit gap reporting. It is a UI feed, not durable full trace or a command journal. Executor trace paths must remain beneath configured trace_root, including reparse/canonical path resolution. Events/results must be bounded JSON objects. Per-call native work retains the existing cooperative cancellation limits.

Tests: tests/HostTests.cpp covers FIFO/concurrency, queued/running cancellation, idempotency/restart, capacities/forget, event gaps, exception isolation, interrupted history, state ownership, device leases, confinement/registry and shutdown.

The BA-Auto application provides a separate stdio JSON-RPC adapter, protocol schema, transport-neutral frontend bridge and portable CPU package. Its BA task names, instance mapping and templates are not part of OpenAR. Existing JSON pipelines and C++ do-while task APIs are unchanged.
