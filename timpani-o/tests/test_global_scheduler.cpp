/*
 * SPDX-FileCopyrightText: Copyright 2026 LG Electronics Inc.
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>
#include <memory>
#include <variant>
#include <vector>

#include "../src/global_scheduler.h"
#include "../src/node_config.h"

class GlobalSchedulerTest : public ::testing::Test {
protected:
    void SetUp() override {
        node_config_manager_ = std::make_shared<NodeConfigManager>();
        scheduler_ = std::make_unique<GlobalScheduler>(node_config_manager_);
    }

    void SetAvailableCpus(const std::string& node_id, const std::vector<int>& cpus) {
        NodeConfig cfg;
        cfg.name = node_id;
        cfg.available_cpus = cpus;
        node_config_manager_->AddNodeConfig(cfg);
    }

    ClassifiedTask MakeTtTask(const std::string& wl_id,
                              const std::string& task_id,
                              uint32_t period_us,
                              uint32_t wcet_us,
                              uint32_t deadline_us = 0) {
        ClassifiedTask t;
        t.workload_id = wl_id;
        t.task_id     = task_id;
        t.mechanism   = Mechanism::TT;
        t.period_us   = period_us;
        t.wcet_us     = wcet_us;
        t.deadline_us = (deadline_us > 0) ? deadline_us : period_us;
        t.assigned_cpu = -1;
        return t;
    }

    ClassifiedTask MakeCbsTask(const std::string& wl_id,
                               const std::string& task_id,
                               uint32_t mit_us,
                               uint32_t wcet_us,
                               uint32_t deadline_us = 0) {
        ClassifiedTask t;
        t.workload_id = wl_id;
        t.task_id     = task_id;
        t.mechanism   = Mechanism::CBS;
        t.period_us   = mit_us;
        t.wcet_us     = wcet_us;
        t.deadline_us = (deadline_us > 0) ? deadline_us : mit_us;
        t.assigned_cpu = -1;
        return t;
    }

    bool IsSuccess(const ScheduleResult& result) {
        return std::holds_alternative<timpani::node::v1::HierarchicalScheduleTable>(result);
    }

    const timpani::node::v1::HierarchicalScheduleTable& GetTable(
        const ScheduleResult& result) {
        return std::get<timpani::node::v1::HierarchicalScheduleTable>(result);
    }

    const InfeasibleError& GetError(const ScheduleResult& result) {
        return std::get<InfeasibleError>(result);
    }

    std::shared_ptr<NodeConfigManager> node_config_manager_;
    std::unique_ptr<GlobalScheduler> scheduler_;
};

// ─── 1. Classification & Basic Schedule Generation ──────────────────────────

TEST_F(GlobalSchedulerTest, SingleTtTaskProducesTable) {
    SetAvailableCpus("node1", {2});
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("brake", "brake_ctrl", 10000, 2000, 5000)
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));

    const auto& table = GetTable(result);
    EXPECT_EQ(table.node_id(), "node1");
    EXPECT_EQ(table.partitions_size(), 1);
}

TEST_F(GlobalSchedulerTest, SingleCbsTaskProducesTable) {
    SetAvailableCpus("node1", {2});
    std::vector<ClassifiedTask> tasks = {
        MakeCbsTask("lidar", "lidar_main", 5000, 2000, 5000)
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));

    const auto& table = GetTable(result);
    EXPECT_EQ(table.partitions_size(), 1);

    bool has_cbs = false;
    for (int p = 0; p < table.partitions_size(); ++p) {
        for (int l = 0; l < table.partitions(p).layers_size(); ++l) {
            if (table.partitions(p).layers(l).cbs_entries_size() > 0) {
                has_cbs = true;
            }
        }
    }
    EXPECT_TRUE(has_cbs);
}

TEST_F(GlobalSchedulerTest, EmptyTasksReturnsEmptyTable) {
    std::vector<ClassifiedTask> tasks;
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));
    const auto& table = GetTable(result);
    EXPECT_EQ(table.partitions_size(), 0);
}

TEST_F(GlobalSchedulerTest, UnconfiguredNodeFallsBackToDefaultCpus) {
    // Without calling SetAvailableCpus(), GlobalScheduler falls back to [2, 3].
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_a", "task_a", 10000, 2000, 5000),
        MakeTtTask("wl_b", "task_b", 10000, 2000, 5000),
    };
    auto result = scheduler_->generate_schedule("unconfigured_node", tasks);
    ASSERT_TRUE(IsSuccess(result));
    EXPECT_EQ(GetTable(result).partitions_size(), 2);
}

// ─── 2. Harmonic Period Validation ──────────────────────────────────────────

TEST_F(GlobalSchedulerTest, HarmonicPeriodsSucceed) {
    SetAvailableCpus("node1", {2});
    // 5ms, 10ms, 20ms are harmonic (5 | 10, 5 | 20, 10 | 20)
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl1", "t1", 5000, 500, 5000),
        MakeTtTask("wl1", "t2", 10000, 1000, 10000),
        MakeTtTask("wl1", "t3", 20000, 1000, 20000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    EXPECT_TRUE(IsSuccess(result));
}

TEST_F(GlobalSchedulerTest, NonHarmonicPeriodsReturnError) {
    SetAvailableCpus("node1", {2});
    // 7ms and 10ms are not harmonic
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl1", "t1", 7000, 500, 7000),
        MakeTtTask("wl1", "t2", 10000, 1000, 10000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::NonHarmonicPeriod);
}

// ─── 3. TT Slot Placement & DM Ordering ─────────────────────────────────────

TEST_F(GlobalSchedulerTest, TtSlotOffsetsMatchPeriod) {
    SetAvailableCpus("node1", {2});
    // Two harmonic tasks on same CPU: fast (period=10ms, wcet=2ms), slow (period=20ms, wcet=2ms)
    // Hyperperiod = 20ms -> fast repeats twice at offsets 0us and 10000us.
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl1", "t_fast", 10000, 2000, 5000),
        MakeTtTask("wl1", "t_slow", 20000, 2000, 10000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));

    const auto& table = GetTable(result);
    EXPECT_EQ(table.hyperperiod_us(), 20000u);
    ASSERT_EQ(table.partitions_size(), 1);

    const auto& layer = table.partitions(0).layers(0);
    ASSERT_EQ(layer.tt_slots_size(), 3);

    std::vector<uint32_t> fast_offsets;
    std::vector<uint32_t> slow_offsets;
    for (int i = 0; i < layer.tt_slots_size(); ++i) {
        const auto& slot = layer.tt_slots(i);
        if (slot.task_id() == "t_fast") {
            fast_offsets.push_back(slot.offset_us());
            EXPECT_EQ(slot.duration_us(), 2000u);
        } else if (slot.task_id() == "t_slow") {
            slow_offsets.push_back(slot.offset_us());
            EXPECT_EQ(slot.duration_us(), 2000u);
        }
    }
    ASSERT_EQ(fast_offsets.size(), 2u);
    EXPECT_EQ(fast_offsets[0], 0u);
    EXPECT_EQ(fast_offsets[1], 10000u);
    ASSERT_EQ(slow_offsets.size(), 1u);
    EXPECT_EQ(slow_offsets[0], 2000u);
}

TEST_F(GlobalSchedulerTest, DmOrderingShorterDeadlineFirst) {
    SetAvailableCpus("node1", {2});
    // Two tasks in same workload: brake(deadline=5ms), steer(deadline=10ms)
    // DM ordering: brake gets placed first at offset 0
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl1", "steer_ctrl", 10000, 2000, 10000),
        MakeTtTask("wl1", "brake_ctrl", 10000, 2000, 5000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));

    const auto& table = GetTable(result);
    ASSERT_EQ(table.partitions_size(), 1);
    const auto& layer = table.partitions(0).layers(0);
    ASSERT_EQ(layer.tt_slots_size(), 2);
    EXPECT_EQ(layer.tt_slots(0).task_id(), "brake_ctrl");
    EXPECT_EQ(layer.tt_slots(0).offset_us(), 0u);
    EXPECT_EQ(layer.tt_slots(1).task_id(), "steer_ctrl");
    EXPECT_EQ(layer.tt_slots(1).offset_us(), 2000u);
}


// TtSlotConflictWhenDeadlineTightRejected will be added after #69 fix 

// MultipleTtWorkloadsWithinDeadlinesNoSlotConflict will be added after #69 fix 

// ─── 4. Gap Extraction & CBS Allocation ─────────────────────────────────────

TEST_F(GlobalSchedulerTest, NoTtSlotsProducesFullGap) {
    SetAvailableCpus("node1", {2});
    // CBS-only workload → entire hyperperiod is a gap
    std::vector<ClassifiedTask> tasks = {
        MakeCbsTask("lidar", "lidar_main", 5000, 1000, 5000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));
}

TEST_F(GlobalSchedulerTest, CbsWithinBudgetSucceeds) {
    SetAvailableCpus("node1", {2});
    // Single CPU: TT 20% + CBS 30% + overhead 2% = 52% <= 80%
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("brake", "brake_ctrl", 10000, 2000, 5000),   // U=0.20
        MakeCbsTask("lidar", "lidar_main", 10000, 3000, 10000), // U=0.30
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    EXPECT_TRUE(IsSuccess(result));
}

TEST_F(GlobalSchedulerTest, CbsExceedingBandwidthFails) {
    SetAvailableCpus("node1", {2, 3});
    // CBS_U = 0.85 > 0.80 - 0.02 = 0.78 → must fail
    std::vector<ClassifiedTask> tasks = {
        MakeCbsTask("wl2", "cbs1", 10000, 8500, 10000),  // U=0.85
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::UtilizationExceeded);
}

// ─── 5. Single-CPU Utilization Admissibility & Edge Cases (Issue #68) ───────

TEST_F(GlobalSchedulerTest, SingleCpuTtMaxUtilization100PercentRejected) {
    SetAvailableCpus("node1", {2});
    // Single TT task with 100% utilization (runtime == period == 20000us)
    // U_tt = 1.0 > 0.78 bound -> Must be rejected with UtilizationExceeded
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_max", "task_100", 20000, 20000, 20000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::UtilizationExceeded);
    EXPECT_EQ(GetError(result).cpu, 2);
}

TEST_F(GlobalSchedulerTest, SingleCpuTtUtilizationAboveBoundRejected) {
    SetAvailableCpus("node1", {2});
    // Single TT task with U_tt = 0.80 (+0.02 overhead = 0.82 > 0.80)
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_over", "task_80", 20000, 16000, 20000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::UtilizationExceeded);
    EXPECT_EQ(GetError(result).cpu, 2);
}

TEST_F(GlobalSchedulerTest, SingleCpuTtUtilizationJustAboveBoundRejected) {
    SetAvailableCpus("node1", {2});
    // Edge case: U_tt = 3901 / 5000 = 0.7802 (+0.02 = 0.8002 > 0.80)
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_edge", "task_just_over", 50000, 39010, 50000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::UtilizationExceeded);
    EXPECT_EQ(GetError(result).cpu, 2);
}

TEST_F(GlobalSchedulerTest, SingleCpuTtAtExactBoundAccepted) {
    SetAvailableCpus("node1", {2});
    // U_tt = 39000 / 50000 = 0.78; with U_OVERHEAD = 0.02 -> sum = 0.80 == U_BOUND
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_exact", "task_78", 50000, 39000, 50000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));
    EXPECT_EQ(GetTable(result).partitions_size(), 1);
}

TEST_F(GlobalSchedulerTest, SingleCpuTtBelowBoundAccepted) {
    SetAvailableCpus("node1", {2});
    // U_tt = 0.50 (wcet 10000, period 20000)
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_ok", "task_50", 20000, 10000, 20000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));
}

TEST_F(GlobalSchedulerTest, SingleCpuMultipleTtTasksSameWorkloadExceedingBoundRejected) {
    SetAvailableCpus("node1", {2});
    // Same workload with two TT tasks: 0.40 + 0.40 = 0.80 > 0.78
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_same", "tt_1", 10000, 4000, 10000),
        MakeTtTask("wl_same", "tt_2", 10000, 4000, 10000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::UtilizationExceeded);
    EXPECT_EQ(GetError(result).cpu, 2);
}

TEST_F(GlobalSchedulerTest, SingleCpuMultipleTtWorkloadsExceedingBoundRejected) {
    SetAvailableCpus("node1", {2});
    // Single CPU [2]: two TT workloads (0.40 + 0.40 = 0.80 > 0.78)
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_1", "tt_1", 10000, 4000, 10000),
        MakeTtTask("wl_2", "tt_2", 10000, 4000, 10000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::UtilizationExceeded);
    EXPECT_EQ(GetError(result).cpu, 2);
}

TEST_F(GlobalSchedulerTest, SingleCpuCombinedTtAndCbsAtExactBoundAccepted) {
    SetAvailableCpus("node1", {2});
    // Single CPU [2]: U_tt=0.50, U_cbs=0.28, U_overhead=0.02 -> sum=0.80 == U_BOUND
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_tt", "tt_50", 20000, 10000, 20000),
        MakeCbsTask("wl_cbs", "cbs_28", 10000, 2800, 10000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));
    EXPECT_EQ(GetTable(result).partitions_size(), 1);
}

TEST_F(GlobalSchedulerTest, SingleCpuCombinedTtAndCbsAboveBoundRejected) {
    SetAvailableCpus("node1", {2});
    // Single CPU [2]: U_tt=0.50, U_cbs=0.29, U_overhead=0.02 -> sum=0.81 > U_BOUND
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_tt", "tt_50", 20000, 10000, 20000),
        MakeCbsTask("wl_cbs", "cbs_29", 10000, 2900, 10000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::UtilizationExceeded);
}

TEST_F(GlobalSchedulerTest, SingleCpuCbsOnlyAtExactBoundAccepted) {
    SetAvailableCpus("node1", {2});
    // Single CPU [2]: U_cbs=0.78, U_overhead=0.02 -> sum=0.80 == U_BOUND
    std::vector<ClassifiedTask> tasks = {
        MakeCbsTask("wl_cbs", "cbs_78", 50000, 39000, 50000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));
}

TEST_F(GlobalSchedulerTest, SingleCpuCbsOnlyAboveBoundRejected) {
    SetAvailableCpus("node1", {2});
    // Single CPU [2]: U_cbs=0.79, U_overhead=0.02 -> sum=0.81 > U_BOUND
    std::vector<ClassifiedTask> tasks = {
        MakeCbsTask("wl_cbs", "cbs_79", 10000, 7900, 10000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::UtilizationExceeded);
}

// ─── 6. Multi-CPU Utilization, Topology & Hyperperiod Tests ─────────────────

TEST_F(GlobalSchedulerTest, MultiCpuTtDifferentPeriodsUnrollAcrossGlobalHyperperiod) {
    SetAvailableCpus("node1", {2, 3});
    // Reproduction case for multi-CPU TT under-scheduling bug (#61):
    // Workload 1: tt_task1 on CPU 2 (period=20ms, wcet=10ms, deadline=18ms) -> U_tt=0.50 <= 0.78
    // Workload 2: tt_task2 on CPU 3 (period=40ms, wcet=2ms,  deadline=4ms)  -> U_tt=0.05 <= 0.78
    // Global hyperperiod = LCM(20ms, 40ms) = 40ms
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("fcw_producer_tt",  "tt_task1", 20000, 10000, 18000),
        MakeTtTask("fcw_producer_tt2", "tt_task2", 40000,  2000,  4000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));

    const auto& table = GetTable(result);
    EXPECT_EQ(table.hyperperiod_us(), 40000u);
    ASSERT_EQ(table.partitions_size(), 2);

    int total_tt = 0;
    std::vector<uint32_t> cpu2_offsets;
    std::vector<uint32_t> cpu3_offsets;

    for (int p = 0; p < table.partitions_size(); ++p) {
        const auto& part = table.partitions(p);
        ASSERT_GT(part.cpuset().cpus_size(), 0);
        uint32_t cpu = part.cpuset().cpus(0);

        for (int l = 0; l < part.layers_size(); ++l) {
            const auto& layer = part.layers(l);
            for (int s = 0; s < layer.tt_slots_size(); ++s) {
                const auto& slot = layer.tt_slots(s);
                total_tt++;
                if (cpu == 2) {
                    cpu2_offsets.push_back(slot.offset_us());
                    EXPECT_EQ(slot.task_id(), "tt_task1");
                    EXPECT_EQ(slot.duration_us(), 10000u);
                } else if (cpu == 3) {
                    cpu3_offsets.push_back(slot.offset_us());
                    EXPECT_EQ(slot.task_id(), "tt_task2");
                    EXPECT_EQ(slot.duration_us(), 2000u);
                }
            }
        }
    }

    EXPECT_EQ(total_tt, 3);
    ASSERT_EQ(cpu2_offsets.size(), 2u);
    EXPECT_EQ(cpu2_offsets[0], 0u);
    EXPECT_EQ(cpu2_offsets[1], 20000u);

    ASSERT_EQ(cpu3_offsets.size(), 1u);
    EXPECT_EQ(cpu3_offsets[0], 0u);
}

TEST_F(GlobalSchedulerTest, MultiCpuMultipleTtWorkloadsGetSeparateCpus) {
    SetAvailableCpus("node1", {2, 3});
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_a", "task_a", 10000, 2000, 5000),
        MakeTtTask("wl_b", "task_b", 10000, 2000, 5000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));
    EXPECT_EQ(GetTable(result).partitions_size(), 2);
}

TEST_F(GlobalSchedulerTest, MultiCpuThreeCpusThreeWorkloadsAtExactBoundAccepted) {
    SetAvailableCpus("node1", {2, 3, 4});
    // 3 CPUs, 3 TT workloads each at exact bound 0.78 -> all accepted on separate CPUs
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_1", "tt_1", 50000, 39000, 50000),
        MakeTtTask("wl_2", "tt_2", 50000, 39000, 50000),
        MakeTtTask("wl_3", "tt_3", 50000, 39000, 50000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));
    EXPECT_EQ(GetTable(result).partitions_size(), 3);
}

TEST_F(GlobalSchedulerTest, MultiCpuOneCpuOverloadedRejectsSchedule) {
    SetAvailableCpus("node1", {2, 3});
    // wl_1 on CPU 2 (U=0.50, valid), wl_2 on CPU 3 (U=0.80 > 0.78, invalid)
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_1", "tt_1", 20000, 10000, 20000),
        MakeTtTask("wl_2", "tt_2", 20000, 16000, 20000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::UtilizationExceeded);
    EXPECT_EQ(GetError(result).cpu, 3);
}

TEST_F(GlobalSchedulerTest, MultiCpuCombinedTtAndCbsAtExactBoundAccepted) {
    SetAvailableCpus("node1", {2, 3});
    // Fill CPU 3 with tt_other (U=0.78) so CBS is forced onto CPU 2:
    // CPU 2: tt_main (U=0.50), cbs_task (U=0.28) -> sum = 0.50 + 0.28 + 0.02 = 0.80 == U_BOUND
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_tt2", "tt_main", 20000, 10000, 20000),   // CPU 2 (U=0.50)
        MakeTtTask("wl_tt3", "tt_other", 50000, 39000, 50000),  // CPU 3 (U=0.78)
        MakeCbsTask("wl_cbs", "cbs_28", 10000, 2800, 10000),    // Must go to CPU 2 (0.28 <= 0.28)
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));
}

TEST_F(GlobalSchedulerTest, MultiCpuCombinedTtAndCbsAboveBoundRejected) {
    SetAvailableCpus("node1", {2, 3});
    // Fill CPU 3 with tt_other (U=0.78) so CBS cannot fit on CPU 3.
    // On CPU 2: tt_main (U=0.50), cbs_task (U=0.29) -> 0.81 > U_BOUND
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_tt2", "tt_main", 20000, 10000, 20000),   // CPU 2 (U=0.50)
        MakeTtTask("wl_tt3", "tt_other", 50000, 39000, 50000),  // CPU 3 (U=0.78)
        MakeCbsTask("wl_cbs", "cbs_29", 10000, 2900, 10000),    // Cannot fit on either CPU
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::UtilizationExceeded);
}

TEST_F(GlobalSchedulerTest, MultiCpuFourCpusMixedTtAndCbsDistributed) {
    SetAvailableCpus("node1", {4, 5, 6, 7});
    // 2 TT workloads on CPUs 4 and 5; 2 CBS tasks placed on empty CPUs 6 and 7 (worst-fit)
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_tt1", "tt_1", 10000, 3000, 5000),
        MakeTtTask("wl_tt2", "tt_2", 20000, 4000, 10000),
        MakeCbsTask("wl_cbs1", "cbs_1", 10000, 4000, 10000),
        MakeCbsTask("wl_cbs2", "cbs_2", 10000, 4000, 10000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));
    EXPECT_EQ(GetTable(result).partitions_size(), 4);
}

// ─── 7. Capacity, Limits & Floating-Point Precision Edge Cases ──────────────

TEST_F(GlobalSchedulerTest, UnrolledSlotsExceedingNodeCapacityFails) {
    SetAvailableCpus("node1", {2, 3});
    // Non-harmonic periods on separate CPUs (1,000 us and 1,000,001 us)
    // produce LCM ≈ 1,000,001,000 us. Unrolled slots = 1,000,001 + 1000 > 2048.
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_a", "task_fast", 1000, 100, 1000),
        MakeTtTask("wl_b", "task_coprime", 1000001, 100, 1000001),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::TtSlotCapacityExceeded);
}

TEST_F(GlobalSchedulerTest, TtSlotCapacityExactBoundary) {
    SetAvailableCpus("node1", {2, 3});
    // Case 1: Exactly 2048 slots (2047 + 1 = 2048) -> SUCCEEDS
    {
        std::vector<ClassifiedTask> tasks = {
            MakeTtTask("wl_a", "t_fast", 1000, 10, 1000),
            MakeTtTask("wl_b", "t_slow", 2047000, 10, 2047000),
        };
        auto result = scheduler_->generate_schedule("node1", tasks);
        ASSERT_TRUE(IsSuccess(result));
        const auto& table = GetTable(result);
        int total_tt = 0;
        for (int p = 0; p < table.partitions_size(); ++p) {
            for (int l = 0; l < table.partitions(p).layers_size(); ++l) {
                total_tt += table.partitions(p).layers(l).tt_slots_size();
            }
        }
        EXPECT_EQ(total_tt, 2048);
    }

    // Case 2: Exactly 2049 slots (2048 + 1 = 2049 > 2048) -> FAILS with TtSlotCapacityExceeded
    {
        std::vector<ClassifiedTask> tasks = {
            MakeTtTask("wl_a", "t_fast", 1000, 10, 1000),
            MakeTtTask("wl_b", "t_slow", 2048000, 10, 2048000),
        };
        auto result = scheduler_->generate_schedule("node1", tasks);
        ASSERT_FALSE(IsSuccess(result));
        EXPECT_EQ(GetError(result).reason, InfeasibleReason::TtSlotCapacityExceeded);
    }
}

TEST_F(GlobalSchedulerTest, HyperperiodExceedingUint32LimitFails) {
    SetAvailableCpus("node1", {2, 3});
    // Large periods (3,000,000,000 us and 4,000,000,000 us) produce LCM = 12×10^9 us > UINT32_MAX.
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("wl_a", "task_3s", 3000000000u, 100, 3000000000u),
        MakeTtTask("wl_b", "task_4s", 4000000000u, 100, 4000000000u),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_FALSE(IsSuccess(result));
    EXPECT_EQ(GetError(result).reason, InfeasibleReason::TtSlotCapacityExceeded);
}

TEST_F(GlobalSchedulerTest, UtilizationHelperBoundaryPrecision) {
    // IEEE 754 double: 0.50 + 0.28 + 0.02 can exceed 0.80 by ~4.4e-17
    EXPECT_TRUE(GlobalScheduler::is_utilization_admissible(0.78, 0.0));
    EXPECT_TRUE(GlobalScheduler::is_utilization_admissible(0.50, 0.28));
    EXPECT_TRUE(GlobalScheduler::is_utilization_admissible(0.0, 0.78));
    EXPECT_FALSE(GlobalScheduler::is_utilization_admissible(0.7801, 0.0));
    EXPECT_FALSE(GlobalScheduler::is_utilization_admissible(0.50, 0.2801));
    EXPECT_FALSE(GlobalScheduler::is_utilization_admissible(1.0, 0.0));

    EXPECT_NEAR(GlobalScheduler::available_cbs_bandwidth(0.50, 0.0), 0.28, 1e-9);
    EXPECT_NEAR(GlobalScheduler::available_cbs_bandwidth(0.78, 0.0), 0.0, 1e-12);
    EXPECT_DOUBLE_EQ(GlobalScheduler::available_cbs_bandwidth(0.85, 0.0), 0.0);
}

// ─── 8. End-to-End, Constants & Table Structure Verification ────────────────

TEST_F(GlobalSchedulerTest, Ddr007SampleWorkloads) {
    SetAvailableCpus("node1", {2, 3});
    // From DDR-007 §5.1:
    //   brake_ctrl    (L1, period=10ms,  wcet=2ms,   deadline=5ms)
    //   steer_ctrl    (L1, period=20ms,  wcet=2ms,   deadline=10ms)
    //   collision_det (L2, MIT=10ms,     wcet=1.5ms, deadline=10ms)
    //   lidar_proc    (L2, MIT=5ms,      wcet=2ms,   deadline=5ms)
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("brake", "brake_ctrl", 10000, 2000, 5000),
        MakeTtTask("steer", "steer_ctrl", 20000, 2000, 10000),
        MakeCbsTask("collision", "col_detect", 10000, 1500, 10000),
        MakeCbsTask("lidar_proc", "lidar_main", 5000, 2000, 5000),
    };

    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));

    const auto& table = GetTable(result);
    EXPECT_EQ(table.node_id(), "node1");
    EXPECT_GT(table.hyperperiod_us(), 0u);
    EXPECT_GT(table.partitions_size(), 0);

    int total_tt = 0;
    int total_cbs = 0;
    for (int p = 0; p < table.partitions_size(); ++p) {
        for (int l = 0; l < table.partitions(p).layers_size(); ++l) {
            total_tt += table.partitions(p).layers(l).tt_slots_size();
            total_cbs += table.partitions(p).layers(l).cbs_entries_size();
        }
        EXPECT_TRUE(table.partitions(p).cpuset().isolated());
    }

    EXPECT_EQ(total_tt, 3);
    EXPECT_GE(total_cbs, 2);
}

TEST_F(GlobalSchedulerTest, FeasibilityConstants) {
    EXPECT_DOUBLE_EQ(GlobalScheduler::U_OVERHEAD, 0.02);
    EXPECT_DOUBLE_EQ(GlobalScheduler::U_BOUND, 0.80);
    EXPECT_EQ(GlobalScheduler::CBS_MIN_EXEC_US, 100u);
    EXPECT_EQ(GlobalScheduler::MAX_NODE_TT_SLOTS, 2048u);
}

TEST_F(GlobalSchedulerTest, TableHasRequiredFields) {
    SetAvailableCpus("node1", {2});
    std::vector<ClassifiedTask> tasks = {
        MakeTtTask("brake", "brake_ctrl", 10000, 2000, 5000),
    };
    auto result = scheduler_->generate_schedule("node1", tasks);
    ASSERT_TRUE(IsSuccess(result));

    const auto& table = GetTable(result);
    EXPECT_FALSE(table.table_id().empty());
    EXPECT_EQ(table.node_id(), "node1");
    EXPECT_GT(table.hyperperiod_us(), 0u);
    EXPECT_EQ(table.epoch_ns(), 0u);

    for (int p = 0; p < table.partitions_size(); ++p) {
        for (int l = 0; l < table.partitions(p).layers_size(); ++l) {
            const auto& layer = table.partitions(p).layers(l);
            for (int s = 0; s < layer.tt_slots_size(); ++s) {
                EXPECT_NE(layer.tt_slots(s).workload_id_hash(), 0u);
                EXPECT_NE(layer.tt_slots(s).task_id_hash(), 0u);
            }
        }
    }
}
