<!--
SPDX-FileCopyrightText: Copyright 2026 LG Electronics Inc.
SPDX-License-Identifier: MIT
-->

# SRS-011: Runtime Table Update — Phase 0 Software Requirements Specification

**Date:** 2026-10-28  
**Status:** Draft — Phase 0 only; neither implementation complete nor approved for safety operation  
**Parent baseline:** [REQ-011](REQ-011-runtime-table-update.md) §1.5, §7.1, §8.2, §9  
**Related documents:** [HLD-011](HLD-011-runtime-table-update.md), [DDR-011](DDR-011-runtime-table-update.md), [LLD-011](LLD-011-runtime-table-update.md)

## 1. Purpose and Applicability Rules

This document defines only the software requirements needed for the **basic operation check** of runtime table update. It does not automatically inherit the requirements of the existing PoC Level 1 and v2.x/v3.0 document sets. The safeguards of the existing implementation are not removed, but implementing later-phase functions is not made a Phase 0 completion condition.

Requirement IDs use `REQ-RTU-P0-F/N-###`, which are distinct from the existing SRS. In the table below, `F-###` and `N-###` are abbreviations of the parent `REQ-011-F/N-###`, and `AC-P0-###` denotes the Phase 0 acceptance criteria of the REQ. Within this document's scope, all active items are SHALL. For some parent requirements, satisfaction is **limited to the Phase 0 applicable portion**.

## 2. Test Boundary

| Category | Phase 0 condition |
|:--|:--|
| Execution target | Single node, one or more pre-designated isolated CPUs (`0..63`), L1 TT only |
| Time base | The clock is stable; all isolated CPUs share the confirmed node-wide execution frame `H` and epoch `E`, fixed for all Phase 0 updates including CLEAR. Candidate TT periods are exactly representable and form a harmonic set whose minimum LCM divides `H`; this minimum repeat cycle does not replace `H`. Slots are expanded over the same `H`, with retained tasks' repeated slots unchanged |
| Table | A single isolated CPU is assigned per task, and the existing assigned CPU of each retained active task is pinned. Test configuration with slots that do not cross a boundary and with sufficient common TT-free margin for preparation and publication after the last TT slot of every isolated CPU in use. An empty table also keeps `E` and `H` |
| Semantic operations | REPLACE, ADD, REMOVE (including recovery STOP, `RECOVERY_STOP`), CLEAR. All are delivered through the existing `ControlCommand.full_table` |
| Input | Workload changes are provided through O's existing inputs (`AddSchedInfo`, `EnforceRecoveryPolicy(RECOVERY_STOP)`) or through an O-internal test path. O generates and validates, and N executes. End-to-end success SHALL NOT be judged on the basis of direct injection into N or `RecoverySignal` delivery alone |
| Requests | Only one request per node is processed at a time in O and N. The next request is submitted only after the preceding result has been confirmed |
| Initial conditions | The same baseline table is confirmed on O and N, and task registration, communication, BPF, and SHM are normal. Initial startup itself is not a test target |

The following are excluded: UPD-MODIFY, hyperperiod/epoch changes, changes to the node's isolated CPU configuration, cross-core migration of active tasks, CBS, composite pipelines, multiple nodes, dynamic CPU provisioning, and a new RPC for normal undeployment. Changes to the existing protocol schema, public methods, or application ABI SHALL NOT be made without approval.

## 3. Functional Requirements

### 3.1 Input, Generation, and Acceptance

| ID | Requirement | Parent REQ | REQ acceptance criteria |
|:--|:--|:--|:--|
| **REQ-RTU-P0-F-001** | The four operations SHALL be realized as full snapshots. Active tasks retained after an update SHALL be pinned to their existing assigned CPUs; ADD/REMOVE SHALL preserve the slot, timing, and CPU parameters of retained workloads; and CLEAR SHALL generate a valid empty table. | F-002..F-007, F-021 | AC-P0-01, AC-P0-02, AC-P0-03, AC-P0-06 |
| **REQ-RTU-P0-F-002** | Only O SHALL generate and validate tables, from the existing inputs (`AddSchedInfo`, `EnforceRecoveryPolicy(RECOVERY_STOP)`) or from an authorized test path. A recovery STOP SHALL also transmit to the owning node a full snapshot that excludes the target workload (an empty table if no workloads remain), and SHALL NOT perform autonomous removal in the fault callback or node-side removal or table synthesis based on `RecoverySignal`. The CPU list SHALL use the existing configuration. | F-008..F-011 | AC-P0-02, AC-P0-06 |
| **REQ-RTU-P0-F-003** | O SHALL keep the candidate model separate from the confirmed model, derive the minimum LCM of the candidate's node-wide TT periods, and verify that the periods form a harmonic set whose minimum LCM divides the confirmed execution frame `H`. O SHALL retain the baseline `E/H`, expand candidate slots over that same `H` without shifting retained tasks' repeated slots, and perform schedulability judgment, CPU pinning of retained tasks, assignment of new workloads to unused isolated CPUs first and then to the least-loaded CPU, and representation and capacity checks. For an empty candidate, the minimum LCM is undefined and `E/H` SHALL still be retained. When generation is rejected, the confirmed model and the node's active table SHALL NOT be changed. | F-021, F-024, F-050, N-004 | AC-P0-01, AC-P0-02, AC-P0-04, AC-P0-06 |
| **REQ-RTU-P0-F-004** | The transport table SHALL include a non-empty `table_id` that is unique within the test run, the target `node_id`, a positive `hyperperiod_us`, and the reference `epoch_ns`. Results SHALL be linked to the identifiers of the same node and the current request. | F-011, F-012 | AC-P0-04, AC-P0-06, AC-P0-07 |
| **REQ-RTU-P0-F-005** | N SHALL verify, before activation, the target, identifiers, required values, inclusion of the isolated CPU mask, single CPU assignment per task, invariance of retained-task CPUs, task references, representable range, current capacity, and the fixed `E/H` conditions. It SHALL reject CBS/non-TT items, distribution of a single task across multiple CPUs, CPU migration of retained tasks, unsupported changes, and the legacy incremental path. N SHALL NOT recompute slot overlap, placement, or schedulability. | F-011, F-021, F-025, F-050, N-004 | AC-P0-04 |
| **REQ-RTU-P0-F-006** | A pre-activation rejection SHALL record its reason and SHALL discard the preparation state, such as candidate registration, maps, and SHM. Parts of the candidate SHALL NOT remain in the active table, the application publication, or O's confirmed model. | F-026 | AC-P0-04 |
| **REQ-RTU-P0-F-007** | Only one request per node SHALL be accepted. An additional request received during processing SHALL be rejected through the existing failure-response path without any state change, and SHALL NOT be held, merged, replaced, or retransmitted. | F-029 | AC-P0-06 |

### 3.2 Normal Boundary Transition and Execution

| ID | Requirement | Parent REQ | REQ acceptance criteria |
|:--|:--|:--|:--|
| **REQ-RTU-P0-F-008** | The candidate's enforcement representation, task resolution, and required resources SHALL be prepared on the non-RT path. During preparation, the active table SHALL NOT be partially overwritten, and candidate tasks SHALL NOT be dispatched early. | F-013, F-023, F-025 | AC-P0-01, AC-P0-04, AC-P0-08 |
| **REQ-RTU-P0-F-009** | Normal activation SHALL be performed at a future boundary where `T = E + kH`. Started TT slots SHALL be completed, and handover SHALL follow confirmation of the TT idle state of every isolated CPU used by the active and candidate tables. While the handover gate is closed, woken TT tasks SHALL be held in an assigned-CPU queue that no select, enqueue, or dispatch path consumes; they SHALL NOT be inserted into `SCX_DSQ_LOCAL_ON` or `SCX_DSQ_GLOBAL`. After the boundary publication completes and the gate opens, only tasks present in the new table SHALL be released according to their scheduled slots. Tasks absent from the new table SHALL be removed from the holding queue and cleaned up without dispatch. The previous table SHALL run before the boundary, and the new table SHALL run after it. If preparation is late, a later boundary SHALL be selected while the active table is kept. | F-013, F-014, F-016, F-051, N-005 | AC-P0-01, AC-P0-08 |
| **REQ-RTU-P0-F-010** | The four operations SHALL NOT require a restart of O, N, or retained workload processes. After a normal transition, it SHALL be possible to confirm task execution according to the new table. | F-001, N-001 | AC-P0-01, AC-P0-02, AC-P0-03 |
| **REQ-RTU-P0-F-011** | After CLEAR, TT dispatch and related runtime items SHALL be cleaned up, but the `E/H/table_id` of the empty active table and the update-reception capability SHALL be retained. Subsequent ADD/REPLACE operations SHALL be applied without a restart. | F-005, F-032 | AC-P0-03 |
| **REQ-RTU-P0-F-012** | Applications SHALL use only complete previous or new tables and SHALL follow the same effective boundary as kernel execution. While a publication is unreadable, applications SHALL NOT use partial data or execute arbitrary jobs. | F-037, F-038, F-051 | AC-P0-05 |
| **REQ-RTU-P0-F-013** | Publication notifications SHALL be treated as control events. Only actual TT slot wakes SHALL be forwarded as job executions, and retained tasks SHALL interpret new publications without a restart. | F-037, F-051 | AC-P0-05 |
| **REQ-RTU-P0-F-014** | On normal completion of REMOVE/CLEAR, the TT execution and budget-related items, task registrations, application items, and internal references of the removal targets SHALL be cleaned up. Withdrawal SHALL be observable upon lookup, and the OS scheduling policy/affinity SHALL NOT be changed or restored. | F-032, F-040 | AC-P0-02, AC-P0-03, AC-P0-05 |

### 3.3 Results and Test Observation

| ID | Requirement | Parent REQ | REQ acceptance criteria |
|:--|:--|:--|:--|
| **REQ-RTU-P0-F-015** | N SHALL send `TableApplied.success=true` for the corresponding `table_id` only after confirming that the actual boundary application and normal cleanup are complete. Success SHALL NOT be reported on the basis of reception or preparation completion alone. A pre-activation rejection SHALL send `success=false` and the reason. | F-033 | AC-P0-07 |
| **REQ-RTU-P0-F-016** | O SHALL promote the candidate to the confirmed model/table only after a successful result that matches the current request. The response to `RECOVERY_STOP` SHALL also be returned as successful, and the session SHALL be maintained, only after a correlated successful ACK has been confirmed within a finite wait deadline. Upon pre-activation rejection, conflict during processing, or unconfirmed status, a failure SHALL be returned and the existing confirmed state SHALL be maintained. Mismatched results SHALL NOT be used as the basis for confirmation and SHALL be recorded as test errors. | F-012, F-024, F-029, F-033 | AC-P0-04, AC-P0-06, AC-P0-07 |
| **REQ-RTU-P0-F-017** | Test logs SHALL be able to link the node, table identifier, operation, target, rejection reason or planned/actual application time, and result. No persistent audit repository or operational timing SLA is required. | F-042 | AC-P0-07 |
| **REQ-RTU-P0-F-018** | If the test precondition is broken by a failure during activation, connection loss, unconfirmed result, or clock anomaly, the test SHALL NOT be judged successful. The test harness SHALL stop subsequent submissions and record the cause, and SHALL NOT assume automatic rollback, safe stop, or successful resynchronization. Existing safety mechanisms SHALL be retained. | N-008 | AC-P0-08 |

## 4. Non-Functional Requirements

| ID | Requirement | Parent REQ | REQ acceptance criteria |
|:--|:--|:--|:--|
| **REQ-RTU-P0-N-001** | Dynamic allocation, blocking, network I/O, or unbounded search SHALL NOT be added to the modified real-time (RT) path, and locks SHALL NOT be held across a scheduling decision. Preparation, release, log output, and gRPC SHALL be performed on the non-RT path. | F-023, N-006 | AC-P0-08 |
| **REQ-RTU-P0-N-002** | The slot execution order across multiple isolated CPUs SHALL be determined by an explicit sort key and SHALL NOT be changed during the execution of a table, and the next slot time SHALL be computed on the basis of the same CPU. The guarantee of input-order-independent global generation results and per-core timer expansion are deferred to Phase 1. | N-005, N-010 | AC-P0-08 |
| **REQ-RTU-P0-N-003** | The smallest of the BPF, SHM, and task-registration upper limits of the current build that applies to the actual path SHALL be applied in advance. The constants used and the effective capacity at the start of the test SHALL be recorded, and input exceeding these limits SHALL be rejected before activation. Capacity expansion is not required. | N-004, F-050 | AC-P0-04 |
| **REQ-RTU-P0-N-004** | The four normal operations, including multi-isolated-CPU configurations, and the basic rejections SHALL be verified by repeatable automated tests. Actual BPF/TT/app execution evidence on the target Linux hardware and host-level test results SHALL be distinguished. | N-005, N-008 | AC-P0-08 |

## 5. Verification Scenarios

The pre-designated isolated CPU set and the fixed `E/H` grid are used, with the baseline table `A={W1,W2}` generated by O. The assigned CPU and timing parameters of retained workload `W1` are fixed. Each test starts from a known baseline state, and unconfirmed results of a previous test are not used as the baseline for the next test.

| Test ID | Input and operation | Observation and success judgment | REQ criteria |
|:--|:--|:--|:--|
| **TC-P0-01** | Replace `A` with a full snapshot of `{W1,W3}` | `E+kH` transition, in-progress slots completed, no mixing of previous and new tables, `W1` assigned CPU pinned and actual execution of the new task, no restart of daemon or retained app | AC-P0-01 |
| **TC-P0-02** | Apply `A→{W1,W2,W3}→{W1,W3}` one request at a time (including removal by recovery STOP) | W1 parameters, assigned CPU, and PID retained; W3 placed on an available isolated CPU and execution started; W2 dispatch stopped and runtime/publication items cleaned up; STOP returns its completion reply only after the correlated ACK is confirmed | AC-P0-02 |
| **TC-P0-03** | `A→empty table→{W1}` and REPLACE after an empty table | ID/E/H of the empty table retained; no TT dispatch or residual items; subsequent application and execution succeed | AC-P0-03 |
| **TC-P0-04** | Parameterize invalid node or field, non-isolated CPU, single-task distribution across multiple CPUs, CPU migration of retained tasks, and task, representation, or capacity overflow. Also submit inputs that O cannot generate, separately | Pre-activation rejection and reason; O's confirmed model and N's active table unchanged; no candidate residue. Unsupported CBS/time-grid changes are verified in the same way | AC-P0-04 |
| **TC-P0-05** | Observe the lookups and waits of TT applications during an update | Only valid previous/new publications used; no job execution by control wake; removal lookup shows withdrawal; no restart of retained apps | AC-P0-05 |
| **TC-P0-06** | Generate the four results from O inputs (including recovery STOP). While one request is in preparation, submit an additional request; then confirm the preceding result and resubmit | Actual O generation/validation, transmission of the existing full_table, and N execution; the additional request is rejected; the next request succeeds after the preceding result is confirmed | AC-P0-06 |
| **TC-P0-07** | Collect O/N logs and results of normal and rejection tests | Linked by the same node/table ID; planned and actual application times distinguished; successful ACK does not occur before actual application | AC-P0-07 |
| **TC-P0-08** | Repeat the above normal and basic-rejection tests and review the modified RT path | Repeatable judgment, identical slot order, no prohibited operations added. Deviation from a test precondition is recorded as an error or abort, not as PASS | AC-P0-08 |
| **TC-P0-09** | With baseline `A={W1(10 ms),W2(20 ms)}` and `H=20 ms`, remove longest-period `W2` through recovery STOP | Candidate minimum LCM is 10 ms, but transport and activation retain `H=20 ms`; `W1`'s repeated slots are unchanged; `W2` is not dispatched, and STOP completes only after the correlated successful ACK | AC-P0-02, AC-P0-07 |
| **TC-P0-10** | With `A={W1(10 ms),W2(20 ms)}` and `H=20 ms`, CLEAR, then ADD `W3(5 ms)` | CLEAR retains `E/H`; ADD has candidate minimum LCM 5 ms but retains execution frame `H=20 ms`, expands `W3` over that frame, and applies without changing `E/H` | AC-P0-03 |

The test wait limit and the finite ACK wait deadline for the recovery STOP response are used **only as termination conditions for call-result judgment or the test harness**, and are not interpreted as an operational unconfirmed-state transition, an automatic-retransmission recovery contract, or an accept-to-activation delay guarantee. Jitter, worst-case latency, zero job omissions, FFI, and fail-safe proof are not acceptance criteria for this phase.

## 6. Phase 0 Handling of Open Items

| REQ item | Current decision/contract | Detailed basis |
|:--|:--|:--|
| OI-01 | All four operations use only the existing full_table path; no delta is used | DDR D-P0-01 |
| OI-05 | Inactive candidate preparation and boundary handover within a fixed TT-free interval. Partial overwrite during execution SHALL NOT occur | DDR D-P0-03, D-P0-04 |
| OI-06 | Reads that check validity and generation; on invalidity, job execution SHALL NOT occur and non-RT re-verification is performed; if invalidity persists, the test is aborted | DDR D-P0-05 |
| OI-10 | Existing logs and test result files are used; no new persistent audit repository is established | F-017 |
| OI-12 | Only current upper-limit checks and rejection of excess input are performed | N-003 |
| OI-15 | Lossless mapping within the existing proto/BPF/SHM representations; if an ABI change is needed, separate approval is required | F-004, F-005, DDR D-P0-06 |
| OI-17 | Recovery removal is performed through the existing `EnforceRecoveryPolicy(RECOVERY_STOP)`; general removal and empty-workload-set tests are performed through O-internal test inputs. No new RPC for normal undeployment is added | DDR D-P0-02 |

## 7. Traceability and Deferral to Later Phases

Each row in §3 and §4 cites its parent REQ and AC-P0. TC-P0-01..08 cover AC-P0-01..08; TC-P0-09 and TC-P0-10 add coverage for AC-P0-02 and AC-P0-03, respectively. HLD specifies responsibilities and flows, DDR specifies decisions, and LLD specifies internal processing and test touchpoints; each cites the active IDs of this SRS. This document does not mark the full set of existing requirements as satisfied.

| Phase | Deferred scope |
|:--|:--|
| Phase 1 | CBS, MODIFY, hyperperiod and isolated CPU configuration changes, cross-core migration of active tasks, pipelines, per-core timer expansion, normal multi-node operation, request waiting/merging/duplicate handling policy (including harmless convergence of repeated STOP), detailed application contract and handle stability, quantitative timing, FFI, capacity expansion, persistent audit |
| Phase 2 | Activation failure, rollback, and restore-failure stop; unconfirmed ACK, connection loss and restart resynchronization; blocking of redispatch and baseline resynchronization upon recovery STOP failure; clock anomaly; preservation of fault-monitoring state; partial failure across multiple nodes |

Hooks or generalized frameworks for deferred functions are not implemented in advance in Phase 0. Implementation plan documents are not included in this document set.
