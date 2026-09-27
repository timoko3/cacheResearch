"""Capacity constraints and directly observable benchmark metrics."""

import math

BASE_CAPACITIES = (2, 32, 256)
DEFAULT_LIMITS = (4, 64, 256)
LEVEL_COSTS = (1.0, 3.0, 10.0)
MEMORY_COST = 50.0


def valid_strategies(strategies, capacities):
    """The current 2Q and LIRS implementations require two resident slots."""
    return len(strategies) == len(capacities) and all(
        capacity >= (2 if strategy in ("2Q", "LIRS") else 1)
        for strategy, capacity in zip(strategies, capacities)
    )


def hierarchy_layouts(total_capacity, l1_sizes, l2_sizes, limits):
    """Keep the total fixed; reject layouts outside explicit per-level limits."""
    if total_capacity < 3 or len(limits) != 3 or min(limits) < 1:
        raise ValueError("Use a positive total and three positive level limits")
    if not l1_sizes or not l2_sizes or min((*l1_sizes, *l2_sizes)) < 1:
        raise ValueError("Candidate capacities must be positive")

    layouts = set()
    for l1_capacity in l1_sizes:
        for l2_capacity in l2_sizes:
            capacities = (l1_capacity, l2_capacity, total_capacity - l1_capacity - l2_capacity)
            if all(0 < size <= limit for size, limit in zip(capacities, limits)):
                if capacities[0] <= capacities[1] <= capacities[2]:
                    layouts.add(capacities)

    if not layouts:
        raise ValueError("No layout satisfies the total and level limits")
    return sorted(layouts)


def software_sizes(workload_scale):
    """Include dense points around the hot set and the two cycle lengths."""
    sizes = {1, 2, 4, 8, 16, 24, 32, 48, 64, 96, 192, 256, 384, 512, 768, 1024}
    for threshold in (
        max(1, workload_scale // 8),
        workload_scale // 2,
        workload_scale,
        4 * workload_scale,
    ):
        sizes.update((max(1, threshold - 1), threshold, threshold + 1))
    sizes.update((max(1, workload_scale // 4), max(1, 3 * workload_scale // 4)))
    return sorted(size for size in sizes if size > 0)


def average_access_cost(counters):
    """Serial probe costs: L3 hit costs 1+3+10, memory miss costs 1+3+10+50."""
    request_count = counters[0]
    memory_loads = counters[2]
    total_cost = memory_loads * MEMORY_COST
    total_cost += sum(counters[3 + 3 * level] * cost for level, cost in enumerate(LEVEL_COSTS))
    return total_cost / request_count


def percent_of_reference(value, reference):
    if reference <= 0:
        raise ValueError("Reference must be positive")
    return 100.0 * value / reference


def best_rows(rows, metric):
    """Return all numerical ties without making policy-name order a winner."""
    minimum_value = min(row[metric] for row in rows)
    return [
        row
        for row in rows
        if math.isclose(row[metric], minimum_value, rel_tol=1e-10, abs_tol=1e-12)
    ]
