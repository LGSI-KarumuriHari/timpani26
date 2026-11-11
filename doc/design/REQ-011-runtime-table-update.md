<!--
SPDX-FileCopyrightText: Copyright 2026 LG Electronics Inc.
SPDX-License-Identifier: MIT
-->

# REQ-011: Runtime Schedule Table Update — Requirements Definition

**Date:** 2026-09-29  
**Last modified:** 2026-10-28 (Phase 0/1/2 introduced; separation of the Phase 0 basic operation scope, acceptance criteria, and follow-on document authoring criteria)  
**Status:** Draft  
**Current design baseline:** Phase 0 — Runtime Table Update basic operation check  
**Author:** Human (Lead Architect) + AI  
**Parent requirement:** SRS-000 `REQ-FUN-012`  
**Related documents:** DDR-002(scheduling architecture), DDR-003(interface/protocol), DDR-005(BPF scheduler), DDR-006(communication), DDR-007(TT/CBS), DDR-011(runtime table update design), DDR-012(fault management)  
**Follow-on documents:** SRS-011, HLD-011, DDR-011, LLD-011, implementation_plan. All documents in the current revision target are written on a **Phase 0 basis**, and the full functionality of the existing document set is not automatically carried over into the current implementation scope (§8.2).

> **Role of this document.** This document is a **requirements definition**. It specifies, at the stakeholder and system level, *what* TIMPANI SHALL do when the schedule table changes at runtime, and *why* that behavior is required. The protocol, data structures, and algorithms are intentionally left unfixed; they are specified in SRS-011 and decided in DDR-011.
>
> **Keyword convention:** **SHALL** = mandatory, **SHOULD** = recommended, **MAY** = optional.

---

## 1. Purpose and Scope

### 1.1 Purpose

**While the system is operating**, this document defines the requirements for implementing, in stages, the capability to change TIMPANI's schedule table. At present, **confirmation of the Phase 0 basic update operation** is the design and implementation target, while detailed management, quality/accuracy improvements, and feature extensions are separated into Phase 1 and fail-safe functions into Phase 2.

Existing requirements are retained to preserve the final target and traceability. **The current scope follows §1.5, and the current acceptance criteria follow §7.1.** Unmet requirements of later phases do not make Phase 0 incomplete, and completing Phase 0 does not imply fulfillment at the level of full functionality or functional safety.

### 1.2 Original Requirement Text

#### 1.2.1 Original Text (Preserved Verbatim)

> "While timpani is operating, new work is created for the schedule table, and there are cases in which, based on this schedule table, the existing schedule table is **replaced**, new work is **added** to the existing schedule table, part of the work is **removed** from the existing schedule table, or the schedule table for all of the work is **deleted**. timpani SHALL respond to these cases."
> — Original requirement (Project Leader, 2026-09-29)

The original text is preserved verbatim for traceability. It states *what situations occur*, but does not specify the unit of change, the scope of the table, the quality attributes implied by "respond", or the behavior on failure. §1.2.2 records these gaps, and §1.2.3 restates the requirement in a form that can be decomposed into §5.

#### 1.2.2 Ambiguities and Gaps in the Original Text

| # | Original wording | Ambiguity / gap | Interpretation decision in this document |
|:-:|:--|:--|:--|
| 1 | "new work", "part of the work" | Is the unit a workload, a task, or a process? | The unit of runtime change is the **workload** defined in DDR-001. A workload can contain multiple tasks, and a change always applies to the entire workload. |
| 2 | "work" | Which scheduling layer is the target? | Only **L1(TT)** and **L2(CBS)** workloads are represented in the schedule table. L3/L4 are delegated to the default Linux scheduler and remain outside the table (DDR-002). |
| 3 | "schedule table" | Is it a single global table or one per node? | The schedule table is a **per-node** artifact. A deployment-level change is decomposed into one update per target node. |
| 4 | "replace / add / remove / delete" | Is it a wire-level operation or an observable result? | It is a **semantic** operation. The transport method that carries it (full snapshot or incremental delta) is a design choice and SHALL NOT change the result. |
| 5 | *(not in original)* | The case in which only the **parameters** of a running workload change (period, budget, deadline, CPU) is not covered by any of the four operations | Added as a fifth derived operation, **UPD-MODIFY** (REQ-011-F-046). Whether it is a separate wire-level operation or is realized through UPD-REPLACE is left open in OI-13. |
| 6 | "SHALL respond" | No quality attributes: zero downtime? atomic? when is it applied? | "Respond" is defined as **zero downtime, atomic application at the hyperperiod boundary, a bounded activation latency, and non-interference for unaffected workloads** (§5.3, §5.4). |
| 7 | *(not in original)* | Who triggers a change, and who generates the table? | The authorized triggers and the division of roles (Pullpiri decides deployments, Timpani-O generates the table, and Timpani-N executes it) are specified in §5.2. |
| 8 | *(not in original)* | Behavior and responsible parties when the change is infeasible, received information is erroneous, or activation fails midway | Schedulability judgment is assigned to Timpani-O, and node-side applicability checking of the received table to Timpani-N. Rejection and restore behavior are specified in §5.5 and §5.6. |
| 9 | *(not in original)* | What running applications observe during a table change | The application-side behavior is specified in §5.7. |
| 10 | *(not in original)* | When changes are requested concurrently or out of order | The outcomes for ordering, replacement, and conflicts are specified in REQ-011-F-027 and REQ-011-F-029. |
| 11 | *(not in original)* | One logical change spans several nodes and only some of them accept it | Added as scenario E-06 and REQ-011-F-047. The atomicity boundary itself is left open in OI-14. |
| 12 | *(not in original)* | How change results are observed and recorded as evidence | Reporting and recording are specified in §5.6 and §5.8. |
| 13 | *(not in original)* | Is there a limit on what a single update can carry? | The capacity limit and rejection at acceptance time are specified in REQ-011-N-004. |

#### 1.2.3 Reinforced Requirement Statements (Normalized)

The restatements below are the **target at completion of all phases** and form the basis for §5. Phase 0 does not require all of the properties at once; the per-phase scope follows §1.5 and the phase labels in §5.

| ID | Reinforced statement |
|:--|:--|
| **SR-01** | The unit of runtime change is the **workload**, and only the L1(TT) and L2(CBS) workloads represented in the schedule table are within the scope of this requirement. |
| **SR-02** | The schedule table is a **per-node** artifact, and a deployment-level change is applied as one update per target node, each addressed to that node. |
| **SR-03** | While TIMPANI is operating, a node's schedule table MAY be subject to a **replacement**, an **addition**, a **removal**, an **in-place change**, or a **clear**, and TIMPANI SHALL support all five of these. |
| **SR-04** | In any of these cases, a restart of Timpani-O, Timpani-N, or an unaffected workload SHALL NOT be required. |
| **SR-05** | A change is applied **atomically at a hyperperiod boundary** of the common epoch grid, and the latency from acceptance to activation is bounded, with the bound stated numerically. |
| **SR-06** | Unaffected workloads SHALL continue to operate without deadline misses, job omissions (missed periodic executions), or budget resets. |
| **SR-07** | A change occurs only from an authorized trigger, and the resulting schedule table is generated solely by Timpani-O from the L1/L2 workload information provided by Pullpiri and the per-node isolated CPU list; each update is addressed to a specific node. |
| **SR-08** | Timpani-O judges the feasibility and schedulability of the schedule table. Timpani-N checks, before applying it, only that the received table targets itself and satisfies the representation and node-local execution conditions, and it does not recompute schedulability. A rejection at any stage does not change the active table. A change that fails during activation ends either in restoration of the previous table or in the defined restore-failed stop state, and a partially applied table SHALL NOT be permitted. After a connection loss or restart, the node converges to the intended state, including the empty table. |
| **SR-09** | The outcome of every change is reported to Timpani-O and recorded in a form suitable for post-hoc analysis. |

#### 1.2.4 Derived Update Operations

| ID | Operation | Meaning |
|:--|:--|:--|
| **UPD-REPLACE** | full replacement | The newly generated schedule table replaces the node's active schedule table in its entirety. |
| **UPD-ADD** | incremental addition | One or more new workloads are added to the active schedule table, and existing entries are retained. |
| **UPD-REMOVE** | incremental removal | Some workloads are removed from the active schedule table, and the remaining entries are retained. |
| **UPD-MODIFY** | in-place change *(derived, §1.2.2 #5)* | The workload set stays the same, while the timing parameters of workloads that already exist are changed. |
| **UPD-CLEAR** | clear | All workloads are removed, so the node's schedule table becomes empty, and the node returns to an idle state (no TT/CBS). |

### 1.3 In-Scope Items

The following is the functional scope of all phases. The portion included in the current design and implementation is limited in §1.5.

- Runtime changes to the L1 TT slot table and the L2 CBS budget set of a running node
- Change propagation: Pullpiri (decides deployments; supplies L1/L2 workload information and the per-node isolated CPU list) → Timpani-O (generates the schedule table) → Timpani-N (executes it)
- Activation semantics, atomicity, isolation, verification, failure handling, and observability of changes
- The impact of changes on TT/CBS applications that are already running
- Multi-node consistency for a single logical change

### 1.4 Out-of-Scope Items

- The schedule table generation policy itself (placement, CPU assignment, feasibility algorithms) — DDR-004, DDR-007
- L3/L4 (NonSafety) workloads that are delegated to the default Linux scheduler and are not represented in the schedule table — DDR-002
- The fault detection and recovery *decision* logic — DDR-012. This document covers only the *application result* on the schedule table.
- Node anomalies **not caused by an update** — loss of the BPF scheduler, timer thread failure, clock discontinuity, corruption of the partition configuration, etc. These belong to the fault management area (DDR-012).

  > **Phase 2 scope decision.** A general node health-state model is not introduced. If a failure occurs for which the previous table cannot be restored, the node enters the **restore-failed stop state** (§2) scoped to the update, and the only way out of this state is a full table re-application by Timpani-O. This recovery state machine is not an implementation target of Phase 0.
- Initial node startup and first table loading. Only the parts that share mechanisms with runtime updates are referenced.
- Persistence of the schedule table across system power cycles

### 1.5 Per-Phase Implementation Scope and Current Baseline

#### 1.5.1 Phase Definitions

| Phase | Purpose | Implementation scope | Completion criteria |
|:--|:--|:--|:--|
| **Phase 0 — Basic Operation** | Confirm that Runtime Table Update actually operates | Normal-path verification of table generation, transmission, activation-boundary application, and execution results (including removal due to recovery STOP), and minimal input rejection, under restricted test conditions (single node, one or more isolated CPUs, core pinning of retained tasks) | Phase 0 acceptance criteria in §7.1 are met |
| **Phase 1 — Management, Quality, and Extension** | Extend to an operable feature set and raise quality and accuracy | Request management, quantitative timing and FFI verification, detailed application contracts, CBS, changes to parameters, hyperperiod, and isolated CPU configuration, cross-core migration of active tasks, per-core timer extension and normal multi-node operation, interfaces and audit records | Meet the SRS and acceptance criteria elaborated when the phase starts |
| **Phase 2 — Fail-Safe** | Guarantee safe results and recovery in case of failure | Activation failure, rollback, and restore-failed stop; unconfirmed ACK, reconnection, and restart resynchronization; recovery STOP failure blocking and resynchronization; clock anomalies; preservation of fault-monitoring state; partial multi-node failure handling | Meet acceptance criteria that include fault injection and recovery scenarios |

Phases are cumulative. Later phases reinforce functionality while maintaining the normal operation of earlier phases. **Deferral is a postponement of new implementation and verification, not permission to remove existing behavior or safety mechanisms.** Phase 0 also maintains the separation of roles, table consistency in normal transitions, RT path discipline, and the constraint that interface changes require approval.

#### 1.5.2 Phase 0 Test Boundary

- **Target:** L1 TT workloads on a single node. On one or more pre-designated isolated CPUs, the test uses a test table with node-wide harmonic periods that the current representation can express exactly and with sufficient slack. CBS and composite pipelines are deferred to Phase 1.
- **Time and resources:** The epoch, global hyperperiod, and isolated CPU set configuration of the node are fixed before and after the update. Each TT task is assigned to a single isolated CPU, and active tasks that remain after the update are pinned to their existing assigned CPU (cross-core migration of active tasks is deferred to Phase 1). Existing TT slots are configured so that they do not cross the boundary. An empty table SHALL also keep this time reference so that it can accept subsequent tables.
- **Updates:** The four outcomes UPD-REPLACE, UPD-ADD, UPD-REMOVE, and UPD-CLEAR are verified through **the single full-snapshot path for the entire node table**. A separate incremental wire-level operation is not required. The parameters of retained workloads are not changed, and UPD-MODIFY is deferred to Phase 1.
- **Inputs and roles:** The set of workloads to change is provided through Timpani-O's existing inputs (workload addition and recovery STOP) or through the test path, and the table that O generates and validates is sent over the existing O→N full-snapshot path. End-to-end success is not determined merely by injecting a test table directly into N. CPU information uses the existing configuration, and the new interfaces for supplying Pullpiri's CPU list and for normal deployment release are not prerequisites for Phase 0 (OI-16, OI-17).
- **Requests and environment:** Only one request per node is processed at a time, and the next request is sent after the result is confirmed. A controlled test environment with a stable connection and clock is assumed. Queuing, merging, retransmission, and automatic recovery are not implemented.
- **Assurance level:** The test confirms that, without restarting the daemons and retained workloads, execution uses the intended table after the boundary transition. Quantitative guarantees for jitter, worst-case latency, zero job omissions, and preservation of budget and fault-monitoring state are separate verification targets for Phase 1/2.

Input and range errors that can be checked before activation are rejected in Phase 0 as well. A run that departs from the preconditions above, such as a failure during activation or a connection loss, is **not judged as a Phase 0 success; the test is stopped and the cause is recorded**. Automatic rollback, safe stop, and recovery guarantees are covered in Phase 2, and Phase 0 is not used as grounds for in-vehicle or safety-operation approval.

#### 1.5.3 Minimum Operation Required in Phase 0

For items in §5 whose phase label includes `0`, only the content below applies to Phase 0. For items also marked for other phases, the remaining detailed obligations are deferred to later phases. This is a per-phase elaboration of the same ID and does not mean that the entire original requirement is met in Phase 0.

| Target requirement | Phase 0 applied content | Follow-on enhancements |
|:--|:--|:--|
| F-001..F-007, N-001 | Realize the four update outcomes as full snapshots and execute them without restarting the daemon or retained workloads | Hyperperiod change and expansion of the operation scope: Phase 1 |
| F-008..F-012 | Only O generates and validates, and N executes on the designated node/CPU. Requests and results are linked through the existing inputs (addition and recovery STOP), the test path, the configuration, and the table identifier; a recovery STOP is also delivered only as a full snapshot to the owning node | Expansion of Pullpiri CPU supply and of normal deployment release/mode triggers: Phase 1; multi-node recovery routing: Phase 1/2 |
| F-013, F-014, F-016, F-051 | Normal transitions are performed consistently at the hyperperiod boundary of the fixed time grid for all isolated CPUs in use, and do not truncate TT slots that have already started. The enforcement representation and the application-visible representation use the same table and effective boundary, and a control wake is not executed as a job | Extended execution conditions: Phase 1; consistency recovery on transition failure: Phase 2 |
| F-021 | Pin active TT tasks retained during an update to their assigned CPU, and preserve the slot, timing, and CPU parameters of retained workloads during addition and removal | Cross-core migration of active tasks (MODIFY), CBS, and quantitative FFI verification: Phase 1 |
| F-023, N-006 | No dynamic allocation, blocking, network I/O, or unbounded work is added to the real-time (RT) path, and no lock is held across scheduling decisions | Detailed analysis of lock ordering and priority inversion: Phase 1 |
| F-024..F-026, F-050, N-004 | O's existing generation checks (node-wide harmonic hyperperiod and pinning of retained tasks to CPUs), and N's checks of the target, required fields, inclusion in the isolated CPU mask, one CPU per task, unchanged CPU for retained tasks, task references, representable range, and current capacity. Unsupported inputs are rejected before activation, the active table and confirmed model are kept, and the candidate is discarded. N does not recompute schedulability | Verification of generalized conversion, precision, and capacity contracts, and of parameter-change rejection: Phase 1 |
| F-029 | Accept only one request per node. If an additional request arrives while one is in progress, reject it through the existing failure-response path without holding or merging it | Queuing, merging, and replacement: Phase 1; baseline recovery after unconfirmed ACK: Phase 2 |
| F-032, F-040 | After a normal removal or clear, including recovery STOP, stop dispatching the target TT tasks and clean up the enforcement and application entries and related runtime resources. Removed entries are observed as withdrawn on lookup, and the OS policy/affinity is not changed | Long-duration repetition, resource reuse, immediate-withdrawal wake contract, and convergence of repeated STOP: Phase 1; blocking re-dispatch on STOP failure: Phase 2 |
| F-033, F-042 | After actual boundary application, report the table identifier and result to O. A recovery STOP returns completion to the caller only after confirmation of a correlated successful application. Record the operation, target, and rejection reason, or the application time and result, in the test log | Complete audit items and persistent retention: Phase 1; distinguished recovery result codes: Phase 2 |
| F-037, F-038 | TT applications interpret only valid previous and new tables, and retained tasks operate without restart. An unreadable state is not used as a normal table | Parameter reinterpretation, first-job losslessness, and quantitative contract for the transitional read interval: Phase 1 |
| N-005, N-008, N-010 | Verify the four normal updates, including multiple isolated CPUs, and the minimal rejections through repeatable automated tests, and keep the slot execution order deterministic, including for slots scheduled at the same time | Per-core timer extension, input-order independence, and extended operations/exceptions: Phase 1; fault injection and recovery: Phase 2 |

The current follow-on documents elaborate only the content of this section. Later phases remain as scope and dependency lists, and their internal state machines, algorithms, and detailed tests are not pulled into Phase 0 implementation work.

---

## 2. Term Definitions

The terms below are used for the full target. The numeric upper bound of the commit window is elaborated in Phase 1, and rollback and the restore-failed stop state are elaborated in Phase 2; the mere presence of these terms does not require implementation in Phase 0.

| Term | Definition |
|:--|:--|
| **Schedule table** | The per-node set of L1 TT slot and L2 CBS budget entries executed by Timpani-N, together with its hyperperiod and epoch |
| **Active table** | The schedule table that the node is currently executing |
| **Candidate table** | A table that has been received or derived but not yet activated |
| **Hyperperiod** | The repetition period of the TT slot pattern (the LCM of the TT task periods) |
| **Epoch** | The absolute time reference point of the hyperperiod grid shared by all nodes of a deployment unit |
| **Activation instant** | The hyperperiod boundary at which a candidate table becomes the active table |
| **Commit window** | A bounded interval immediately preceding the activation instant. No TT slot is scheduled to run in this interval, and the node switches from the previous table to the new table during it. |
| **Job** | One execution of a task: one TT slot instance, or one release handled under a CBS budget. *Job omission* refers to an instance that was supposed to execute but did not. |
| **Update transaction** | The complete sequence from acceptance of an update request to completion of activation of the new table or of rollback |
| **Rollback** | Processing that reverts a failed update within the node so that the previous table continues to execute. A failure before switching to the new table is handled as an **abort**, which discards the prepared candidate; a failure after switching is handled as a **restore**, which returns to the previous table. |
| **Revert** | Timpani-O redeploys a table with the previous contents under a new table identifier. For the node, this is a new update, not a rollback. |
| **Affected workload** | A workload whose slots, budget, CPU assignment, or existence changes as a result of an update |
| **Unaffected workload** | A workload that exists unchanged before and after an update |
| **Restore-failed stop state (`RESTORE_FAILED`)** | The state a node enters when an update fails and the previous table also cannot be restored. Because it cannot be guaranteed that the running table matches the intended table, dispatching of affected workloads is stopped. The only way out is a successful full table re-application by Timpani-O. It is a state of the update transaction, not a general node health state (§1.4). In the safety domain, "degraded" denotes a state in which operation continues with reduced functionality, so this name is not used. The DDR-011 v2.x document set refers to a similar state as `DEGRADED`. |
| **Result code** | A machine-readable value that informs Timpani-O of the result of an update attempt. It distinguishes at least applied, no change, rejected, rollback completed, and restore failed. |
| **Node-side applicability check** | Timpani-N's verification, before activation, of the received table's target node, request identifier, and authorized communication connection; the format and representable range of fields and values; the validity of node-local references (CPUs, tasks, etc.); the required local resources; and compatibility with the current clock, the active time grid, the activation boundary, and the handover preparation conditions. It does not repeat the placement, slot overlap, CPU assignment, CBS budget allocation, or whole-schedule schedulability analysis performed by Timpani-O. |
| **Table identifier** | A value that uniquely identifies a table version so that requests and results can be linked to each other |
| **Generated schedule** | A schedule table generated and held by Timpani-O |
| **Transport representation** | The form in which a table is delivered from Timpani-O to Timpani-N |
| **Enforcement representation** | The form that Timpani-N maintains so that the kernel can execute the table and applications can read their own parameters |

---

## 3. Stakeholders and Needs

The following are the stakeholder needs for all phases. In Phase 0, the test boundary and minimum operation in §1.5 apply.

| Stakeholder | Need |
|:--|:--|
| **Pullpiri (deployment orchestrator)** | Deploys or releases L1/L2 workloads at any time without taking the vehicle out of service, supplies the per-node isolated CPU list that establishes FFI, and receives a definitive response on acceptance or rejection. |
| **Timpani-O (schedule table generator)** | Remains the sole entity that generates schedule tables from the received workload information and isolated CPU list and judges their schedulability, and tracks the actual application status of all nodes. |
| **Timpani-N (node executor)** | Checks only the target, representation, and node-local execution conditions of the received table. It does not recompute schedulability (placement, slot overlap, CPU assignment, CBS budget allocation, etc.), and executes the table on the designated CPUs in the designated order. |
| **Safety (L1/L2) applications** | Are not disturbed by changes that do not concern them, and are notified in a defined manner when their own timing parameters change. |
| **Functional safety engineer** | Evidence (FFI) that unaffected safety workloads cannot miss their deadlines because other workloads are added or removed |
| **Integration/operations engineer** | Observes what was requested, what was applied, when it was applied, and why it failed. |

---

## 4. Operational Scenarios

Each scenario describes the **behavior at completion of all phases**. In Phase 0, the normal paths of UC-01..UC-04 (including removal by recovery STOP under UC-03) are verified under the restrictions of §1.5, and UC-05, hyperperiod changes, and CBS are excluded. Among the exceptions below, basic input rejection applies to Phase 0, duplicate and conflict management to Phase 1, and recovery from activation failure, connection loss, and partial multi-node failure to Phase 2.

### UC-01 — Schedule table full replacement (UPD-REPLACE)

- **Trigger:** When a new workload set becomes valid (e.g., mode transition Parking → Highway) and Pullpiri provides new workload information, Timpani-O generates a new schedule table for the node.
- **Preconditions:** The node holds an active table and is running.
- **Main flow:** Timpani-O checks schedulability and generates the new schedule table → Timpani-N checks the received target, representation, and node-local execution conditions (schedulability is not recomputed) → if applicable, executes at the next hyperperiod boundary → reports the result
- **Postconditions:** The node executes only the new table. The hyperperiod, slot placement, and CBS budget set MAY all change.
- **Special property:** The hyperperiod MAY change, and the node SHALL realign its timing grid without losing common epoch alignment.
- **Exceptions:** E-01 (infeasible), E-02 (commit failure), E-03 (connection loss)

### UC-02 — Workload addition to a running table (UPD-ADD)

- **Trigger:** Pullpiri deploys an additional workload to a node on which workloads are already running.
- **Preconditions:** The node has a non-empty active table, and there is capacity to accommodate the new workload.
- **Main flow:** Timpani-O determines placement and schedulability and generates the resulting table → Timpani-N checks the received target, representation, and node-local execution conditions (placement and schedulability are not recomputed) → if applicable, executes at the next hyperperiod boundary → reports the result
- **Postconditions:** The new workload is dispatched, and existing workloads continue without interruption, retaining their timing parameters.
- **Exceptions:** E-01 (placement not possible → request rejected, active table unchanged), E-04 (workload already exists)

### UC-03 — Partial workload removal from a running table (UPD-REMOVE)

- **Trigger:** Pullpiri releases the deployment of a workload (the deployment release interface is OI-17), or a STOP is applied to a workload according to a recovery decision (DDR-012). In the case of STOP, Pullpiri proceeds with cleanup of the workload (container) after confirming, through Timpani-O, that the node's runtime cleanup succeeded.
- **Preconditions:** The workload to be removed exists in the active table, and at least one workload remains after removal.
- **Main flow:** Timpani-O generates the resulting table with the workload removed while preserving the removal intent → Timpani-N checks the received target, representation, and node-local execution conditions (schedulability is not recomputed) → if applicable, executes at the next hyperperiod boundary and cleans up the Timpani runtime state of the removed workload (OS CPU scheduling policy/affinity is neither changed nor restored) → reports the result to O (and to Pullpiri in the case of STOP)
- **Postconditions:** The removed workload is no longer dispatched and retains no residual runtime state, and the remaining workloads continue without interruption.
- **Exceptions:** E-04 (nonexistent workload), E-02 (commit failure), E-05 (removal request reaches a node that does not own the workload)

### UC-04 — Schedule table clear (UPD-CLEAR)

- **Trigger:** The deployment of the last workload is released (OI-17), the node is drained, or the entire workload set is torn down.
- **Preconditions:** The node holds an active table.
- **Main flow:** Timpani-O generates and validates an empty table for the node → Timpani-N performs the node-side applicability check on the empty table as well (including the current clock, active time grid, activation boundary, and handover preparation conditions; schedulability is not recomputed) → if applicable, executes at the next hyperperiod boundary, stops all TT slot triggers and CBS budget control, and returns the workload-related runtime state → reports the result
- **Postconditions:** The node holds an **empty but valid** table, is idle from the TT/CBS perspective, and can immediately accept a subsequent UPD-REPLACE or UPD-ADD.
- **Special property:** An empty table is **a legitimate table**, not "no update." It SHALL NOT be silently ignored, and it SHALL NOT be left so that the previous table keeps running.

### UC-05 — In-place parameter change of a running workload (UPD-MODIFY)

- **Trigger:** The timing parameters of an already running workload change — period, CBS budget, and deadline through Pullpiri, and CPU assignment through relocation by Timpani-O — with no workload added or removed.
- **Preconditions:** The workload exists in the active table, and the changed parameters are feasible.
- **Main flow:** Timpani-O reflects the changed parameters, judges schedulability, and generates the resulting table → Timpani-N checks the received target, representation, and node-local execution conditions (schedulability is not recomputed) → if applicable, executes at the next hyperperiod boundary → the workload continues operating with the new parameters, and the result is reported
- **Postconditions:** The changed workload operates with the new parameters without a restart, the workload set does not change, and other workloads are not affected.
- **Special property:** Because the workload keeps running across the change interval, application-visible parameters SHALL be reinterpreted rather than re-registered.
- **Exceptions:** E-01 (new parameters not feasible), E-04 (nonexistent workload)

### Exception Scenarios

| ID | Scenario | Required outcome (summary) |
|:--|:--|:--|
| **E-01** | No feasible schedule exists for the requested change | Rejected with the reason, active table unchanged, no node-side side effects |
| **E-02** | Activation fails midway on the node | Restore the previous table, or enter the restore-failed stop state and stop dispatching the affected workloads. A partially applied table SHALL NOT occur. |
| **E-03** | Connection loss between Timpani-O and Timpani-N during an update | During the loss, the node keeps executing the last applied table; on reconnection, the node's running table is brought into agreement with the intended table (including when the intended table is empty) |
| **E-04** | Duplicate or conflicting requests (adding an existing workload, removing a nonexistent workload, two updates in progress concurrently) | A documented deterministic outcome, with no ambiguous state |
| **E-05** | Update delivered to the wrong node | The node rejects it with no state change and reports it as an error |
| **E-06** | One logical change spans multiple nodes, and at least one node rejects or fails | The outcome follows the documented multi-node atomicity boundary. No node is left in an undocumented state or in an undetected divergent state |

---

## 5. Requirements

Priority: **M** = required for acceptance in the applicable phase, **S** = recommended, **C** = conditional/future

**Phase notation:** `0`, `1`, and `2` are the applicable phases; where several phases are listed, such as `0/1`, the ID is satisfied progressively across those phases. **Phase 0 applies only the content of §1.5.3.** Items `1`, `2`, and `1/2` are not prerequisites for current implementation or acceptance. The original requirement text below preserves the goals through the later phases and is not used in bulk as detailed input to the current design.

> Once assigned, a requirement ID is never renumbered or reused. Therefore, IDs within a section may not be consecutive, and this is not an error.
>
> **ID prefix change (2026-10-01).** The DDR-011 v2.3 SRS uses `REQ-RTU-F/N-###` with a different meaning, so the prefix in this document was changed from `REQ-RTU-` to `REQ-011-`. The numbers were not changed. The `F-###` and `N-###` in the body are abbreviations of `REQ-011-F-###` and `REQ-011-N-###`, respectively.

### 5.1 Operation Scope

| ID | Requirement | Priority | Phase | Rationale / Source |
|:--|:--|:-:|:--|:--|
| **REQ-011-F-001** | TIMPANI SHALL be able to change the schedule table while the system is operating, and during such a change SHALL NOT restart Timpani-O, Timpani-N, or unaffected workload processes. | M | 0 | Original requirement, SRS-000 REQ-FUN-012 |
| **REQ-011-F-002** | The system SHALL support **UPD-REPLACE**: full replacement of the node's active schedule table with a new table, including a change of hyperperiod. | M | 0/1 | UC-01 |
| **REQ-011-F-003** | The system SHALL support **UPD-ADD**: adding one or more workloads to the active schedule table while retaining all existing entries. | M | 0 | UC-02 |
| **REQ-011-F-004** | The system SHALL support **UPD-REMOVE**: removing some workloads from the active schedule table while retaining the remaining entries. | M | 0 | UC-03 |
| **REQ-011-F-005** | The system SHALL support **UPD-CLEAR**: deleting the node's entire schedule table, leaving an empty but valid table and an idle node state, and accepting subsequent updates immediately. | M | 0 | UC-04 |
| **REQ-011-F-006** | Update operations SHALL be specified as **semantic** operations. The choice of transport method (full snapshot vs. incremental delta) SHALL NOT change the observable result of an operation. | M | 0 | Prevents transport-dependent semantics; allows the snapshot path and the future delta path to coexist |
| **REQ-011-F-007** | Each update SHALL be applied as exactly one of the defined operations, and SHALL NOT be a partially ordered mixture whose result varies with the internal evaluation order. | M | 0 | Determinism, E-04 |
| **REQ-011-F-046** | The system SHALL support **UPD-MODIFY**: changing the timing parameters (period, budget, deadline, offset, CPU assignment) of workloads that already exist in the active table, without adding or removing workloads and without restarting them. | M | 1 | UC-05, §1.2.2 #5. Whether this is a separate wire-level operation or is realized through UPD-REPLACE is OI-13 |

### 5.2 Roles, Triggers, and Addressing

| ID | Requirement | Priority | Phase | Rationale / Source |
|:--|:--|:-:|:--|:--|
| **REQ-011-F-008** | Timpani-O SHALL be the only entity that generates a node's schedule table, and SHALL derive it from the L1/L2 workload information supplied by Pullpiri and the per-node isolated CPU list. Timpani-N SHALL only execute the table it receives and SHALL NOT recompute placement, offsets, or budgets. | M | 0/1 | DDR-002, single source of truth. No interface yet exists through which Pullpiri supplies the isolated CPU list (OI-16) |
| **REQ-011-F-009** | Updates SHALL occur only from authorized triggers: (a) a deployment change from Pullpiri, (b) a decision to apply recovery, (c) an explicit operations or mode-transition command, (d) a change to the node's isolated CPU list. | M | 0/1/2 | DDR-003, DDR-012. Undeployment under (a) is OI-17; (d) is OI-16 |
| **REQ-011-F-010** | Timpani-N SHALL NOT initiate schedule table changes on its own. In particular, a workload SHALL NOT be removed from the table by fault detection alone. | M | 0 | Fault ≠ eviction, DDR-012 |
| **REQ-011-F-011** | Every update SHALL explicitly designate a specific node, and a node SHALL reject, without changing its state, any update that is not addressed to it. | M | 0 | E-05, prevents inter-node side effects of broadcast application |
| **REQ-011-F-012** | Every update SHALL include a table identifier, and SHALL allow its result report to be clearly linked to the originating request, even when several updates occur in short succession. | M | 0/1 | E-04, prevents delayed stale ACKs |

### 5.3 Determinism and Atomicity

| ID | Requirement | Priority | Phase | Rationale / Source |
|:--|:--|:-:|:--|:--|
| **REQ-011-F-013** | An accepted update SHALL be applied **atomically**. At any point in time, the node SHALL execute either the entire previous table or the entire new table, and SHALL NOT execute a mixture of the two. | M | 0/2 | Determinism, safety |
| **REQ-011-F-014** | Activation of the new table SHALL occur at a hyperperiod boundary of the common epoch grid. | M | 0 | DDR-011, prevents partial slots, deadline mismatches, and budget mismatches |
| **REQ-011-F-015** | The delay from "node activation preparation complete" to "new table active" SHALL have an upper bound, and that bound SHALL be stated **numerically** in SRS-011. A commit window that is not quantified is not permitted. | M | 1 | Review finding: a barrier without a number cannot be verified |
| **REQ-011-F-016** | A TT slot that has already started SHALL NOT be truncated by an update and SHALL be allowed to run to completion. | M | 0 | UC-01/02/03, time determinism |
| **REQ-011-F-017** | If an update changes the hyperperiod, the node SHALL realign the timing grid to the new hyperperiod while maintaining common epoch alignment, and phase error SHALL NOT accumulate. | M | 1 | UC-01 |
| **REQ-011-F-018** | When one logical update affects multiple nodes, all target nodes SHALL activate on the common epoch grid and preserve the inter-node phase relationships defined by Timpani-O. | M | 1 | SRS-000 REQ-FUN-016 |
| **REQ-011-F-019** | The system SHOULD define the update behavior when the system clock used for the epoch grid is stepped or slewed. | S | 2 | Review finding: clock-basis contract not defined |
| **REQ-011-F-047** | The atomicity boundary for a change spanning multiple nodes SHALL be defined — whether results are processed independently with per-node atomicity or the entire deployment is processed all-or-nothing — and SHALL also specify the obligations of nodes that have already committed when another node rejects or fails. | M | 1/2 | E-06, §1.2.2 #11 |
| **REQ-011-F-051** | When a node maintains two or more enforcement representations for the same table, it SHALL be guaranteed that those representations describe the same table and take effect with reference to the same activation boundary. What the kernel executes and what is advertised and fired to applications SHALL NOT diverge during or after an update, and a publication notification (control wake) SHALL NOT be consumed by early job execution before the activation boundary. | M | 0/2 | Otherwise the scheduler and the applications operate on different tables, or phantom jobs are executed (R24-02) |

### 5.4 Unaffected Workload Isolation (FFI)

| ID | Requirement | Priority | Phase | Rationale / Source |
|:--|:--|:-:|:--|:--|
| **REQ-011-F-020** | Due to the addition, removal, replacement, or deletion of other workloads, an unaffected workload SHALL NOT suffer a deadline miss, suffer job omission, or have its CBS budget reset. | M | 1 | FFI, SRS-000 REQ-NFR-002 |
| **REQ-011-F-021** | Unless Timpani-O intentionally changes them, the TT offsets, intervals, deadlines, CPU assignments, and CBS budgets of unaffected workloads SHALL be preserved identically before and after an update. | M | 0/1 | UC-02, UC-03 |
| **REQ-011-F-022** | The application-visible identifiers and lookup handles of unaffected workloads SHALL remain stable across an update, and SHALL NOT cause unnecessary wake-ups or forced resynchronization during publication within the commit window. | M | 1 | Prevents distortion of unaffected task wake-ups (R24-02) |
| **REQ-011-F-023** | Any part of the update procedure SHALL NOT introduce blocking, dynamic allocation, network I/O, or unbounded work into the real-time critical path. | M | 0 | Repository engineering constraint (no gRPC or asynchronous logic on the RT path) |
| **REQ-011-F-052** | An update SHALL NOT reset the fault-monitoring state of unaffected workloads (such as the accumulated deadline-miss value). The fault-monitoring thresholds of workloads that are newly added or whose parameters have changed SHALL be ready before those workloads execute under the new table. | M | 2 | Review finding: the current implementation resets fault-monitoring state each time a table is applied, so faults approaching their thresholds are obscured by unrelated updates. The fault-monitoring logic itself is within the scope of DDR-012 |

### 5.5 Generation Check and Node-Side Applicability Check

> **Responsibility boundary.** Schedulability is determined by Timpani-O. Timpani-N's checks are limited to filtering out, before activation, reception and representation errors and node-local conditions that prevent application. Timpani-N does not re-perform slot placement and overlap analysis, CPU reassignment, CBS budget allocation, or the overall schedulability check. The node-side checks below are requirements, and do not mean that the current implementation already performs all of them.

| ID | Requirement | Priority | Phase | Rationale / Source |
|:--|:--|:-:|:--|:--|
| **REQ-011-F-024** | Timpani-O SHALL verify the content and validity of all resulting tables, and their feasibility and schedulability, before deployment. Not only ordinary changes but also removals, empty tables, retransmissions, and reverts follow the same verification responsibility. An infeasible change SHALL be rejected with a machine-readable reason, and Timpani-O's workload model and the active tables of all nodes SHALL remain in their pre-change state. If a parameter change to an existing workload is rejected, that workload remains with its previous parameters. | M | 0/1 | E-01, DDR-004, DDR-007. Review finding: the current implementation removes an existing workload from the model when a change is rejected |
| **REQ-011-F-025** | Timpani-N SHALL verify the **node applicability** of the candidate table before activation. The verification scope covers whether the target node, the request identifier, and the authorized communication connection are valid; whether required fields and value formats are correct; whether values can be represented without loss in the node's enforcement representation; whether the designated CPU and task references are valid on that node; whether local application resources are ready; and whether the activation boundary and handover preparation conditions are satisfied and compatible with the current clock and the active time grid. If the table is not applicable, the node rejects it with a reason and does not automatically relocate or correct the table. This verification does not recompute placement, slot overlap, CPU assignment, CBS budget allocation, or overall schedulability. | M | 0/1 | E-02, responsibility boundary and defense in depth |
| **REQ-011-F-026** | A candidate table that fails the node applicability check in Timpani-N SHALL be rejected with a reason, without changing the active table and without leaving any partial state of the candidate. | M | 0 | E-02 |
| **REQ-011-F-027** | The results of duplicate or contradictory requests SHALL be deterministic and documented. At a minimum, they include: adding a workload that already exists, removing a workload that does not exist, modifying a workload that does not exist, a change that does not alter any parameter, deleting a table that is already empty, and two updates to the same node proceeding concurrently. | M | 1 | E-04 |
| **REQ-011-F-028** | Incremental updates (UPD-ADD / UPD-REMOVE) SHALL be anchored to the table state on which they were computed, and SHALL be rejected if the node's active state differs from that basis. | M | 1 | Prevents an undetected divergence between O's model and N's running table |
| **REQ-011-F-029** | Updates to the same node SHALL be serialized and processed in a defined order. Follow-up requests that arrive while a preceding update is in progress are held or merged only as intent for unsent requests, and after the result of the preceding attempt is processed (or, if it is unconfirmed, after baseline resynchronization), the next candidate table SHALL be generated and verified from the latest confirmed input. A candidate that has been superseded or whose baseline revision does not match SHALL be discarded or rejected according to the documented policy, and SHALL NOT be applied opportunistically. | M | 0/1/2 | E-04, R24-03 |
| **REQ-011-F-050** | The meaning of the table SHALL be preserved across the generation, transport, and enforcement representations. Timpani-O verifies convertibility and schedulability before generation and transmission, and Timpani-N verifies only lossless convertibility to the enforcement representation. Conditions under which the representation cannot be preserved, such as values exceeding the representable range, loss of time precision, exceeding capacity, and identifier collisions, SHALL be detected and rejected before activation, and this does not imply an obligation for Timpani-N to redo schedulability analysis. | M | 0/1 | Units, integer widths, and identifier formats differ between representations, and conversion loss can make the executed table differ from the intended table |

### 5.6 Failure Handling and Recovery

| ID | Requirement | Priority | Phase | Rationale / Source |
|:--|:--|:-:|:--|:--|
| **REQ-011-F-030** | If activation fails at any stage, the node SHALL perform a rollback so that the previous active table continues to execute. If restoration is not possible, the node SHALL enter the **restore-failed stop state** and abort dispatch of the affected workloads. However, workloads being removed by a recovery STOP SHALL NOT have the previous table restored and dispatched again on the grounds of a cleanup or transition failure. | M | 2 | E-02, R24-04 |
| **REQ-011-F-031** | A failed update SHALL NOT leave residual state belonging to the candidate table (table entries, timers, budgets, task registrations, application-visible entries). | M | 2 | E-02 |
| **REQ-011-F-032** | When a workload is removed (UPD-REMOVE or UPD-CLEAR), all Timpani runtime state held for that workload (table entries, budget state, task registrations, application-visible entries, internal references) SHALL be released. STOP cleanup does not change or restore the OS CPU scheduling policy or CPU affinity of the target task. | M | 0/1/2 | UC-03, UC-04, prevents resource leaks during long-running operation |
| **REQ-011-F-033** | The node SHALL report the result of every update (success or failure, reason, table identifier) to Timpani-O. | M | 0/2 | UC-01..04, observation of actual state |
| **REQ-011-F-034** | The time limit after which a missing result report is treated as unconfirmed, the waiting deadline and substitute result for pending unsent requests, and the corresponding state transitions and resynchronization procedure SHALL be defined. | M | 2 | Review finding: the owner and value of the ACK timeout and the handling of pending requests are undefined (R24-03) |
| **REQ-011-F-035** | After an unconfirmed result, a connection loss, or a node restart, the node's running table SHALL be coordinated so that it is first resynchronized, without operator intervention, to the baseline confirmed by Timpani-O (including reflection of the pending STOP-removal intent and adjustment of empty tables), and only then are pending follow-on ordinary requests generated and applied. | M | 2 | E-03, R24-03. Review defect: re-application of empty tables was missing, and follow-on generation could be contaminated in the unconfirmed state |
| **REQ-011-F-036** | Entry into and exit from the restore-failed stop state SHALL be observable by Timpani-O, and the exit conditions SHALL be documented. | M | 2 | Review finding: the corresponding v2.x state (`DEGRADED`) exists only locally on the node |
| **REQ-011-F-048** | A restore failure, meaning a failure to restore the previous table, SHALL be reported with a **result code that is distinct from** ordinary rejection and rollback completion. | M | 2 | Otherwise Timpani-O cannot distinguish "the previous table is still running" from "what is running is unknown" |
| **REQ-011-F-049** | The only path out of the restore-failed stop state SHALL be a **successful full-table reapplication** by Timpani-O. While in this state, the node SHALL reject incremental updates but SHALL accept full-table reapplication. Uniform rejection of all updates SHALL NOT be required — doing so would also block the path out of this state. | M | 2 | E-02. Confines the recovery path within the update function, without a separate health-state model |
| **REQ-011-F-053** | While the connection to Timpani-O is lost, the node SHALL continue to execute the last table it applied. A partially received update is not treated as applied. | M | 2 | E-03, DDR-006 (keep the last schedule during connection loss) |

### 5.7 Application-Perspective Behavior

| ID | Requirement | Priority | Phase | Rationale / Source |
|:--|:--|:-:|:--|:--|
| **REQ-011-F-037** | TT/CBS applications SHALL be able to detect table changes and reinterpret their own timing parameters, and SHALL distinguish the control wake (publication notification) from the job wake (job-execution notification), execute the new schedule from the first job wake after the activation boundary, and continue operating without restart and without missing a single period. | M | 0/1 | Zero downtime from the application perspective, not only for the daemon (R24-02) |
| **REQ-011-F-038** | Applications SHALL observe only a complete previous table or a complete new table, and SHALL be unable to read a torn, partially published state. | M | 0 | Consistency across the userspace ↔ kernel ↔ application boundary |
| **REQ-011-F-039** | If a transient interval exists during which the published table cannot be read, its maximum duration SHALL be stated numerically, and the behavior that applications SHALL exhibit during that interval SHALL be specified. | M | 1 | Review finding: client contract for the invalid interval is undefined |
| **REQ-011-F-040** | A workload removed by UPD-REMOVE or UPD-CLEAR SHALL no longer be dispatched by TT/CBS. Its entry is also withdrawn from the application-visible area, and a removal task that was waiting SHALL wake on publication and immediately observe the entry withdrawal (`-1`). | M | 0/1 | UC-03, UC-04, R24-02 |
| **REQ-011-F-041** | Changes to the application-visible layout SHALL be version-controlled, and an application built against an older version SHALL be able to detect incompatible changes. | M | 1 | Prevents undetected misinterpretation after interface changes |

### 5.8 Observability and Evidence

| ID | Requirement | Priority | Phase | Rationale / Source |
|:--|:--|:-:|:--|:--|
| **REQ-011-F-042** | Every update attempt SHALL be recorded together with its table identifier, operation type, affected workloads, acceptance decision and reason, activation time, and final result. | M | 0/1 | Integration and incident analysis |
| **REQ-011-F-043** | Records SHALL outlive the process that created them and SHALL be retained in a location suitable for post-hoc analysis, and SHALL NOT use a volatile location, a location without rotation, or a globally writable location. | M | 1 | Review finding: temporary-directory audit sink |
| **REQ-011-F-044** | The node's update state SHALL be observable as a state machine whose transitions and side effects are documented. | M | 1/2 | Review finding: transitions not enforced, side effects not described |
| **REQ-011-F-045** | The timing of updates (acceptance, commit window, activation) SHALL be measurable at runtime so that REQ-011-N-002 and REQ-011-N-003 can be verified on the target hardware. | M | 1 | Verification |

### 5.9 Non-Functional Requirements

| ID | Requirement | Category | Priority | Phase | Rationale / Source |
|:--|:--|:--|:-:|:--|:--|
| **REQ-011-N-001** | **Zero downtime.** None of the five operations SHALL require a daemon restart or a restart of unaffected workload processes. | Availability | M | 0/1 | Original requirement |
| **REQ-011-N-002** | **Timing determinism.** During an update, the trigger jitter of unaffected L1 TT workloads SHALL remain within the system budget of SRS-000 REQ-NFR-001 (tens of μs). | Performance | M | 1 | SRS-000 REQ-NFR-001 |
| **REQ-011-N-003** | **Latency bound.** The worst-case latency from acceptance to activation SHALL have an upper bound, that bound SHALL be stated numerically, and it SHALL be verified on the target hardware. | Performance | M | 1 | REQ-011-F-015 |
| **REQ-011-N-004** | **Capacity.** The maximum number of workloads, tasks, slots, and CBS entries supported by updates SHALL be stated, and an update that exceeds capacity SHALL be rejected at acceptance time, not at activation time. | Scalability | M | 0/1 | REQ-011-F-024·F-025 |
| **REQ-011-N-005** | **Multi-core scalability.** Update processing SHALL scale across the isolated CPUs, including slots scheduled at the same time on different CPUs. | Performance | M | 0/1 | SRS-000 REQ-NFR-005 |
| **REQ-011-N-006** | **RT path discipline.** On the real-time (RT) path, dynamic allocation, blocking calls, and holding a lock across a scheduling decision SHALL be prohibited; lock order SHALL be documented; and there SHALL be no priority inversion. | Safety | M | 0/1 | Repository engineering constraint |
| **REQ-011-N-007** | **Interface compatibility.** Application-visible interfaces SHALL be version-controlled; breaking changes SHALL be announced, and the need for consumer recompilation SHALL be detectable (breaking changes SHALL NOT be accepted silently). | Maintainability | M | 1 | Past breaking change to the layout |
| **REQ-011-N-008** | **Testability.** Each of the five operations and each exception scenario E-01..E-06 SHALL be verifiable by automated, deterministic tests. | Verifiability | M | 0/1/2 | Acceptance criteria |
| **REQ-011-N-009** | **Portability.** Requirements are expressed independently of Linux-specific mechanisms and SHALL be implementable on alternative RTOS targets as well. | Portability | S | 1 | `TIMPANI-on-QNX.md`, SRS-000 REQ-ENV-004 |
| **REQ-011-N-010** | **Deterministic ordering.** A table generated from the same workload set and parameters SHALL be identical regardless of input order, and the execution order of slots that share the same start time SHALL also be determined deterministically. | Determinism | M | 0/1 | Repository engineering constraint (preserve order of data on which scheduling depends) |

---

## 6. Constraints and Assumptions

### 6.1 Constraints

| ID | Constraint | Source |
|:--|:--|:--|
| **CON-01** | Linux 6.12+ with `sched_ext` support, `PREEMPT_RT`, cgroup v2 | SRS-000 REQ-ENV-001..003 |
| **CON-02** | TT task periods are restricted to a harmonic set, and the hyperperiod is their LCM. Quantization when periods are expressed in integer time units can break harmonic relationships (e.g., 1/30 s and 1/15 s are harmonic, but expressed as 33 333 µs and 66 667 µs they are not harmonic). The normalization rule is tracked in OI-18. | DDR-007 |
| **CON-03** | Pullpiri owns deployment decisions, Timpani-O is the only entity that generates the schedule table, and Timpani-N is the executor that runs workloads accordingly. | DDR-002 |
| **CON-04** | In multi-node deployments, the common epoch is derived from a synchronized time source. | SRS-000 REQ-FUN-016 |
| **CON-05** | Interface (protobuf) definitions are not changed without explicit approval. Wire-level needs derived from this requirement are not assumed; they are recorded as approval items in §9. | Repository engineering constraint |
| **CON-06** | External dependencies or new public APIs are not added without explicit approval. | Repository engineering constraint |
| **CON-07** | Existing public methods of `TimerMaster`, `BpfLoader`, `TaskRegistry`, `FaultMonitor`, and `GlobalScheduler` are not deleted or renamed. | Repository engineering constraint |

### 6.2 Assumptions

In Phase 0, the existing configuration is used to supply CPU information for ASM-01. It does not presuppose a new Pullpiri interface, verification of multi-node clock synchronization, or normalization of unrepresentable periods. The approval, public API, and real-time (RT) path constraints in §6.1 apply in all phases.

| ID | Assumption |
|:--|:--|
| **ASM-01** | Pullpiri provides complete and accurate L1/L2 workload metadata for every workload involved in an update, along with a correct per-node isolated CPU list. No interface yet supplies the isolated CPU list (OI-16). |
| **ASM-02** | Workload identifiers remain stable across process restarts and are resolved from deployment metadata rather than thread names (SRS-000 REQ-NFR-004). |
| **ASM-03** | Runtime update frequency is low compared with the hyperperiod frequency. Updates are operational events, not a per-period mechanism. |
| **ASM-04** | Applications participating in TT scheduling do not set kernel scheduling attributes themselves; they cooperate with the periodic wait interface (SRS-000 REQ-NFR-003). |

---

## 7. Acceptance Criteria (Requirement Level)

### 7.1 Current Acceptance Criteria — Phase 0

If all items below are met under the test conditions in §1.5, **only Phase 0** is judged complete. SRS-011 makes this concrete as test inputs, observation points, and success/failure judgments. Actual execution is confirmed on the target hardware, and the criteria are not to be interpreted as proof of a performance upper bound or a fail-safe guarantee.

| ID | Phase 0 acceptance criteria | Verification target |
|:--|:--|:--|
| **AC-P0-01** | Replace a non-empty TT table with a new full table. The switch occurs at a fixed epoch / node-wide hyperperiod boundary, with no truncation of slots that started at that boundary and no mixed-table execution, and tasks of the new table run without a daemon restart while the assigned CPUs of retained tasks remain pinned. | F-001, F-002, F-013, F-014, F-016, F-021, F-051, N-001 |
| **AC-P0-02** | Apply workload addition and partial workload removal, including recovery STOP, each as a full snapshot. For retained TT workloads, parameters, assigned CPUs, and processes remain unchanged; new tasks are placed on available isolated CPUs and run; and removed tasks are no longer dispatched as TT. | F-003, F-004, F-021, F-032, F-037, F-040, N-001 |
| **AC-P0-03** | After an empty table is applied, all TT dispatches and related runtime entries are cleared. If a subsequent ADD or REPLACE is applied on the same time base, execution resumes without a restart. | F-005, F-032, F-040 |
| **AC-P0-04** | Before activation, reject with a reason any candidate that has an invalid node, a required-field error, an invalid CPU/task reference, a single task distributed across multiple CPUs, or a retained task moved to a different CPU, or that exceeds the representable range or current capacity. The active table and O's confirmed model remain unchanged, and no partial state of a rejected candidate is left behind. | F-011, F-024, F-025, F-026, F-050, N-004 |
| **AC-P0-05** | During a normal update, TT applications use only a complete previous or new table and do not run a publication notification as an early job. Removed items are observed as withdrawn at query time, and retained tasks continue to operate without a restart. | F-037, F-038, F-040, F-051 |
| **AC-P0-06** | Generate and validate the results of the four operations on O's existing input path (addition and recovery STOP) and test path, and send them over the existing O→N path. N does not recompute the schedule or change it autonomously. Requests are distinguished by identifier; a follow-on request that arrives while one request is being processed is rejected, and the next request is processed only after the result is confirmed. | F-006, F-007, F-008, F-009, F-010, F-012, F-029 |
| **AC-P0-07** | For each test, the table identifier, operation, target, rejection reason or actual activation instant, and result can be confirmed in the test log. Success is confirmed by the N→O result after actual application at the activation boundary, not by receipt, and the recovery STOP response is also returned as successful only after this confirmation. | F-033, F-042 |
| **AC-P0-08** | The four normal updates, including configurations with multiple isolated CPUs, and the basic rejection tests are automated and repeatable, and slot order remains deterministic. Confirm that the changed real-time (RT) path has not added dynamic allocation, blocking, network I/O, unbounded work, or lock holding that spans a scheduling decision. | F-023, N-005, N-006, N-008, N-010 |

### 7.2 Full Target Acceptance Criteria — for Tracking in Later Phases

The existing AC-01..AC-17 are not deleted or renumbered. The items below are targets for the full functionality and are **not a Phase 0 completion gate**. The basic operation check for AC-04 and AC-09 is performed first under §7.1, and the remaining detailed verification is made concrete in Phase 1/2. In particular, verification of rollback, connection-loss/unconfirmed recovery, restore failure, and multi-node partial failure belongs to Phase 2.

| ID | Acceptance criteria | Verification target |
|:--|:--|:--|
| **AC-01** | With a multi-workload table in operation, each of UPD-REPLACE, UPD-ADD, UPD-REMOVE, UPD-MODIFY, and UPD-CLEAR succeeds, and the table that executes as a result exactly matches the intended table. | F-001..F-007, F-046 |
| **AC-02** | For each of the five operations, all unaffected workloads have zero deadline misses and zero job omissions, with jitter within the REQ-NFR-001 budget, and the cumulative fault-monitoring values are maintained across the update. | F-020..F-022, F-052, N-002 |
| **AC-03** | An UPD-REPLACE whose hyperperiod differs is activated on the common epoch grid without accumulated phase error and without truncated slots. | F-014, F-016, F-017 |
| **AC-04** | UPD-CLEAR leaves an empty, valid, and idle table, and subsequent UPD-ADD/UPD-REPLACE succeed immediately without a restart. | F-005, F-035 |
| **AC-05** | Infeasible requests (including in-place parameter changes to existing workloads) are rejected with a reason, and it is demonstrated that Timpani-O's workload model and the active tables of all nodes remain unchanged. | F-024, F-026 |
| **AC-06** | When an activation failure is injected, one of the following occurs: rollback to the previous table completes, or a restore-failed stop state is reported. In neither case is there mixed-table execution or residual state. | F-030, F-031, F-036 |
| **AC-07** | During a connection loss or unconfirmed ACK that spans the update interval (including an update to an empty table), the node continues to execute the last applied table. On reconnection or recovery, the node matches the intended table solely by processing pending requests after first recovering the confirmed baseline (`M0 − STOP`). | F-034, F-035, F-053 |
| **AC-08** | For each contradictory or duplicate request in REQ-011-F-027, and for generation after holding, merging, or ACK processing of follow-on requests that arrive in-flight, the documented result is reproduced repeatedly. | F-027, F-029 |
| **AC-09** | A removal request delivered to a node that does not own the workload is rejected without any state change and without a transition to the restore-failed stop state. | F-011 |
| **AC-10** | A running TT application observes the change, does not mistake the control wake from publication for job execution, continues to operate with the new parameters from the first job wake after the activation boundary without missing a single period, and does not read a torn table. | F-037..F-040, F-051 |
| **AC-11** | The measured worst-case acceptance-to-activation latency and commit window duration are reported numerically and are within the specified bound. | F-015, F-045, N-003 |
| **AC-12** | For every update attempt covered by AC-01..AC-11, a record containing the items of REQ-011-F-042 can be retrieved. | F-042, F-043 |
| **AC-13** | When only the parameters of a running workload are changed, the change is applied at the next hyperperiod boundary; that workload operates with the new parameters from its first job after the boundary, without a restart; and other workloads are not disturbed. | F-046, F-020, F-037 |
| **AC-14** | When one logical change spans multiple nodes and one node rejects or fails, the resulting state of every node matches the documented multi-node atomicity boundary, and the outcome is reported to Timpani-O. | F-018, F-047, F-033 |
| **AC-15** | When a non-restorable activation failure is injected, the node reports a distinct result code, stops dispatching the workloads targeted by the change, rejects incremental updates, and returns, through full-table reapplication, to a state matching the intended table. | F-030, F-048, F-049 |
| **AC-16** | A table that cannot be represented without loss — a hyperperiod outside the representable range, a slot count exceeding capacity, or identifier collisions — is rejected before activation with a reason, either at Timpani-O's pre-generation or pre-transmission check or at Timpani-N's node-side applicability check. Timpani-N does not recompute schedulability on that basis. | F-025, F-050, N-004 |
| **AC-17** | When the same workload set is submitted several times with only the input order changed, the same table is generated each time, and the slot execution order for slots with the same start time is also the same. | N-010 |

---

## 8. Traceability

### 8.1 Parent Trace

| REQ-011 group | SRS-000 | DDR |
|:--|:--|:--|
| §5.1 Operation scope | REQ-FUN-012 | DDR-011 |
| §5.2 Roles and addressing | REQ-FUN-010, REQ-FUN-011 | DDR-003, DDR-006 |
| §5.3 Determinism and atomicity | REQ-NFR-001, REQ-FUN-016 | DDR-002, DDR-007, DDR-011 |
| §5.4 Isolation | REQ-NFR-002 | DDR-002, DDR-004, DDR-012 |
| §5.5 Verification | REQ-FUN-013 | DDR-004, DDR-007 |
| §5.6 Failure and recovery | REQ-FUN-015, REQ-NFR-004 | DDR-012, DDR-003 |
| §5.7 Application behavior | REQ-FUN-007, REQ-NFR-003 | DDR-005 |
| §5.8 Observability | REQ-FUN-008, REQ-FUN-014 | DDR-005, DDR-012 |
| §5.9 Non-functional | REQ-NFR-001, REQ-NFR-002, REQ-NFR-005 | DDR-002, DDR-011 |

### 8.2 Child Trace

The trace targets of the current revision are **the Phase 0 applicable content in §1.5.3 and AC-P0-01..AC-P0-08**. The 63 requirement IDs in §5 are preserved as the full target list, but Phase 1/2 items remain only in the list of items deferred to Phase 1/2 and are not expanded into currently active requirements or implementation work. IDs that span several phases distinguish the part currently satisfied from the remaining part.

| Document | Phase 0 revision basis |
|:--|:--|
| **SRS-011** | Activates only the Phase 0 applicable requirements and acceptance criteria, and makes concrete the test conditions, minimum rejection cases, and application/result judgments. Does not require simultaneous concretization of all 63 items |
| **HLD-011** | Limited to the normal-update path (input → O generation/validation → transmission → N preparation → boundary application → result report) and the RT/non-RT responsibility boundary |
| **DDR-011** | Records only the decisions needed for Phase 0. Existing advanced design decisions are classified as currently adopted, deferred to later phases, or not applicable, and their existence alone does not create an implementation obligation |
| **LLD-011** | Details only the data, interface usage, synchronization, and basic error handling of the normal path above. Excludes the recovery state machine, CBS, and pending/merge algorithms |
| **implementation_plan** | Decomposes only the Phase 0 implementation and test work into an execution plan, and links each task to the active SRS and AC-P0 items. Phase 1/2 is separated into a follow-on list and is not linked as a prerequisite completion condition |

The trace direction is **REQ-011 (Phase 0 applicable portion) → SRS-011 → HLD/DDR/LLD → implementation_plan and tests**. Each follow-on document SHALL state its applicable phase. Until the current document revision is complete, the blanket SHALL statements, full acceptance criteria, and interpretations of previous versions of existing documents are not used as grounds for expanding the Phase 0 scope.

Active SRS requirements cite the parent REQ-011 IDs and AC-P0 items, and design decisions and implementation work SHALL maintain this linkage. Retired or deferred IDs are not reused, and their prefixes are kept distinct (REQ-011 uses `REQ-011-`, SRS-011 uses `REQ-RTU-`). When Phase 1/2 starts, the corresponding deferred items are activated and the trace matrix is extended. **This revision is limited to REQ and does not imply that revision of the follow-on documents themselves is complete.**

---

## 9. Open Items to Resolve in SRS-011

The items below are decision items to be resolved across all phases. **The Phase 0 SRS resolves only the current portion of the following table**, and it does not treat closing all decisions of later phases as a condition for starting. The **(approval)** marker means that explicit approval from the maintainer or Project Leader is required before the change is implemented. Phase separation does not cancel existing approvals or approval constraints.

| Resolution phase | Items | Current handling |
|:--|:--|:--|
| **Phase 0** | OI-01, OI-05 | Decides only the method for achieving consistent normal boundary transitions, based on the single full-snapshot path |
| **Phase 0 basic / Phase 1 detailed** | OI-06, OI-10, OI-12, OI-15 | Defines basic behavior when unreadable, the test log, and the current capacity/representation checks. The transition-period bound, persistent audit, and extended capacity/representation contract are deferred to Phase 1 |
| **Phase 0 recovery/test path / Phase 1 interface** | OI-17 | Recovery removal uses the existing `EnforceRecoveryPolicy(RECOVERY_STOP)`; general removal and empty-workload-set tests run through O's internal test path; and the new interface for normal deployment release is deferred to Phase 1 |
| **Phase 1** | OI-02, OI-03, OI-04, OI-13, OI-16, OI-18 | Incremental/request management, quantitative bounds, MODIFY, Pullpiri CPU supply, and period normalization are deferred to Phase 1 |
| **Phase 1 normal / Phase 2 failure handling** | OI-14 | The boundary for normal multi-node activation is defined in Phase 1, and the obligations under partial failure are defined in Phase 2 |
| **Phase 2** | OI-07, OI-08, OI-09, OI-11 | Restore-failure reporting, resynchronization after unconfirmed operational ACK, clock anomalies, and node addressing for multi-node recovery removal are deferred to Phase 2 |

The existing item descriptions and approval history below are preserved for follow-on tracking and are not automatically adopted as the detailed design of the current scope.

| ID | Open item | Impact |
|:--|:--|:--|
| **OI-01** | Transmission method for UPD-ADD / UPD-REMOVE: a single full-snapshot path, or a snapshot plus incremental-delta path in parallel? **(requires approval when the interface changes)** | F-006, CON-05 |
| **OI-02** | Merge algebra of the incremental path: conflict rules, replace-versus-merge semantics for changed items, and supersession policy | F-006, F-027, F-029 |
| **OI-03** | Anchor mechanism that binds incremental updates to the active table state **(requires approval when the interface changes)** | F-028, CON-05 |
| **OI-04** | Numerical upper bounds for the commit window and the acceptance-to-activation latency | F-015, N-003 |
| **OI-05** | Whether atomicity is achieved by in-place update of a live structure or by a shadow/double-buffer structure, and the effect of each choice on F-013 | F-013, DDR-011 §2 open item B-2.1 |
| **OI-06** | Application contract during the transition interval in which the table is unreadable: retry, backoff, or failure? | F-039 |
| **OI-07** | How the restore-failure result code is delivered to Timpani-O (is the existing result report sufficient?), and how it is confirmed that the node has exited the restore-failed state | F-036, F-044, F-048, F-049 |
| **OI-08** | Owner, value, and state transition of the timeout for missing result reports | F-034 |
| **OI-09** | Adopt the §7 P8 recommendation as the document design, following [SRS §6.2](SRS-011-runtime-table-update.md#62-clock-discontinuity-reporting-and-recovery-contract-p8): paired clock, freshness/rate, reporting, blocking, and full recovery under a new external ID. Replaces option C of approval item 5; numeric values, target, and DDR-012 integration are verification gates | F-019 |
| **OI-10** | Location, format, and retention period of the record/audit sink, and whether a recording failure blocks activation | F-042, F-043 |
| **OI-11** | Node addressing for recovery-based removal, so that the change reaches only the owning node | F-011 |
| **OI-12** | Capacity figures for workload, task, slot, and CBS entries | N-004 |
| **OI-13** | Whether UPD-MODIFY is a separate operation or is realized as UPD-REPLACE of the target node's table, and which parameters are allowed as in-place change targets **(requires approval when the interface changes)** | F-046, F-006, CON-05 |
| **OI-14** | Atomicity boundary of multi-node changes: per-node atomicity (independent results) vs. all-or-nothing for the whole deployment, and the obligation to roll back nodes that have already committed | F-047, F-018 |
| **OI-15** | Schema of each representation and mapping rules between representations: time unit and precision, integer width (including the maximum representable hyperperiod), identifier generation and collision handling, and capacity limits **(requires approval when the interface changes)** | F-050, F-051, N-004, CON-05 |
| **OI-16** | Pullpiri supply and required interface changes are approved as document design under §7 P9: the node/revision/full list, candidate input/ACK, and unsafe shrink rejection in [SRS §3.3](SRS-011-runtime-table-update.md#33-pullpiri-node-cpu-list-contract-p9). The review criterion is the absence of YAML/field; the actual schema/proto implementation and deployment transition require a separate request and verification | F-008, F-009, ASM-01, CON-05 |
| **OI-17** | Normal deployment-release interface. Currently, the only requests from Pullpiri → Timpani-O are `AddSchedInfo` (addition/update) and `EnforceRecoveryPolicy` (recovery), so deployment release (UC-03, UC-04) is possible only through recovery STOP. **(requires approval when the interface changes)** | F-004, F-005, F-009, CON-05 |
| **OI-18** | Period normalization and quantization rules: the representation method for periods that cannot be exactly expressed in integer time units (e.g., 1/30 s), and the acceptance decision when quantization breaks harmonic relationships or the hyperperiod exceeds its bound | CON-02, F-024, F-050, N-004 |

---

## 10. References

- `SRS-000-timpani-system.md` — System requirements baseline (`REQ-FUN-012`)
- `DDR-001-workload-model.md` — Workload classification (L1–L4)
- `DDR-002-scheduling-architecture.md` — HSF, isolation
- `DDR-003-interface-protocol.md` — Timpani-O ↔ Timpani-N and recovery interface
- `DDR-004-resource-allocation.md` — CPU/partition allocation
- `DDR-005-sched-ext-bpf-scheduler.md` — Node-side execution and application interface
- `DDR-006-communication-architecture.md` — Communication architecture
- `DDR-007-tt-cbs-integrated-scheduling.md` — TT/CBS integration, hyperperiod
- `DDR-011-runtime-table-update.md` — Runtime table update design
- `DDR-012-fault-management.md` — Fault handling and recovery
- `TIMPANI-on-QNX.md` — RTOS portability considerations
