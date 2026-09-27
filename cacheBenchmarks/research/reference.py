"""Offline mandatory-admission MIN and a relaxed multilevel cost lower bound."""

import heapq


def min_misses(requests, capacity):
    if capacity < 1:
        return len(requests)

    # Precompute the next occurrence of every request.
    next_positions = {}
    next_use = [len(requests)] * len(requests)
    for request_index in range(len(requests) - 1, -1, -1):
        key = requests[request_index]
        next_use[request_index] = next_positions.get(key, len(requests))
        next_positions[key] = request_index

    resident_next_use = {}
    eviction_queue = []

    misses = 0
    for request_index, key in enumerate(requests):
        if key not in resident_next_use:
            misses += 1
            if len(resident_next_use) == capacity:
                while True:
                    negative_next_use, evicted_key = heapq.heappop(eviction_queue)
                    if resident_next_use.get(evicted_key) == -negative_next_use:
                        del resident_next_use[evicted_key]
                        break

        resident_next_use[key] = next_use[request_index]
        heapq.heappush(eviction_queue, (-next_use[request_index], key))
        # Discard stale heap entries before they dominate memory usage.
        if len(eviction_queue) > 4 * capacity:
            eviction_queue = [
                (-next_position, resident_key)
                for resident_key, next_position in resident_next_use.items()
            ]
            heapq.heapify(eviction_queue)

    return misses


def hierarchy_lower_bound(capacities, costs, memory_cost, request_count, misses_by_capacity):
    """Relax each prefix to an independent fully associative MIN cache.
    Prefixes need not be jointly realizable: this is a bound, not an oracle.
    Assumes serial lookups, mandatory admission and no prefetching.
    """
    lower_bound_cost = costs[0]
    prefix_capacity = capacities[0]
    for level_index in range(1, len(capacities)):
        lower_bound_cost += costs[level_index] * misses_by_capacity[prefix_capacity] / request_count
        prefix_capacity += capacities[level_index]
    return lower_bound_cost + memory_cost * misses_by_capacity[prefix_capacity] / request_count
