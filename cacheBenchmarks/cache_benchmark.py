import argparse
import csv
import itertools
import random
import subprocess
from pathlib import Path


CACHE_STRATEGIES = ("LRU", "LFU", "ARC", "2Q", "LIRS")
LEVEL_CAPACITIES = ((64,), (8, 56), (16, 48), (32, 32), (8, 16, 40))
LEVEL_ACCESS_COSTS = (1.0, 5.0, 20.0)
MEMORY_ACCESS_COST = 100.0

REQUEST_PATTERNS = (
    "small_cycle", "boundary_cycle", "scan", "hot_cold",
    "hot_scan", "phase_change", "bursts", "uniform",
)

CSV_COLUMNS = (
    "pattern", "seed", "configuration", "requests",
    "l1_requests", "l1_hits", "l1_misses",
    "l2_requests", "l2_hits", "l2_misses",
    "l3_requests", "l3_hits", "l3_misses",
    "memory_misses", "hit_rate", "amat",
    "ideal_memory_misses", "ideal_hit_rate", "ideal_amat",
    "extra_memory_misses", "slowdown_vs_ideal_pct",
)


def generate_hot_cold_key(random_generator, hot_page_offset):
    if random_generator.randrange(100) < 90:
        return hot_page_offset + random_generator.randrange(8)

    return random_generator.randint(1024, 2047)


def generate_request_key(pattern_name, request_index, previous_key, random_generator):
    if pattern_name == "small_cycle":
        return request_index % 32
    if pattern_name == "boundary_cycle":
        return request_index % 65
    if pattern_name == "scan":
        return request_index % 256
    if pattern_name == "hot_cold":
        return generate_hot_cold_key(random_generator, 0)
    if pattern_name == "hot_scan":
        if request_index % 512 < 384:
            return random_generator.randrange(8)
        return 1024 + request_index
    if pattern_name == "phase_change":
        return generate_hot_cold_key(random_generator, (request_index // 2000) % 4 * 64)
    if pattern_name == "bursts":
        if request_index % 16 == 0:
            return random_generator.randrange(256)
        return previous_key
    if pattern_name == "uniform":
        return random_generator.randrange(256)

    raise ValueError(f"Unknown pattern: {pattern_name}")


def generate_request_sequence(pattern_name, random_seed, request_count):
    random_generator = random.Random(random_seed)
    request_sequence = []
    previous_key = 0

    for request_index in range(request_count):
        previous_key = generate_request_key(
            pattern_name, request_index, previous_key, random_generator
        )
        request_sequence.append(previous_key)

    return request_sequence


def create_cache_configurations():
    configurations = []

    for capacities in LEVEL_CAPACITIES:
        for strategies in itertools.product(CACHE_STRATEGIES, repeat=len(capacities)):
            strategies = tuple(reversed(strategies))
            configuration_name = "+".join(
                strategy + str(capacity)
                for strategy, capacity in zip(strategies, capacities)
            )
            configurations.append((configuration_name, strategies, capacities))

    return configurations


def get_config_path(root_directory, configuration_name):
    return root_directory / "configs" / f"benchmark_{configuration_name}.txt"


def get_input_path(root_directory, pattern_name, random_seed, capacities):
    capacity_name = "_".join(str(capacity) for capacity in capacities)
    return root_directory / "inputs" / (
        f"benchmark_{pattern_name}_seed{random_seed}_{capacity_name}.txt"
    )


def write_config_file(file_path, strategies):
    file_path.write_text(
        str(len(strategies)) + "\n" + "\n".join(strategies) + "\n",
        encoding="utf-8",
    )


def write_input_file(file_path, capacities, requests):
    capacity_line = " ".join(str(capacity) for capacity in capacities)
    request_line = " ".join(str(request) for request in requests)
    file_path.write_text(
        f"{capacity_line}\n{len(requests)}\n{request_line}\n", encoding="utf-8"
    )


def generate_benchmark_files(root_directory, configurations, random_seeds, request_count):
    (root_directory / "configs").mkdir(parents=True, exist_ok=True)
    (root_directory / "inputs").mkdir(parents=True, exist_ok=True)

    for configuration_name, strategies, capacities in configurations:
        write_config_file(get_config_path(root_directory, configuration_name), strategies)

    write_config_file(get_config_path(root_directory, "REF64"), ("REF",))

    for pattern_name in REQUEST_PATTERNS:
        for random_seed in random_seeds:
            requests = generate_request_sequence(pattern_name, random_seed, request_count)

            for capacities in LEVEL_CAPACITIES:
                input_path = get_input_path(root_directory, pattern_name, random_seed, capacities)
                write_input_file(input_path, capacities, requests)


def check_statistics(statistics, request_count, level_count):
    if len(statistics) != 12 or any(value < 0 for value in statistics):
        raise ValueError("Runner must return 12 nonnegative integer counters")

    total_requests, total_hits, total_misses = statistics[:3]
    expected_requests = request_count
    accumulated_hits = 0

    for level_index in range(3):
        offset = 3 + 3 * level_index
        level_requests, level_hits, level_misses = statistics[offset:offset + 3]

        if level_index >= level_count:
            if level_requests or level_hits or level_misses:
                raise ValueError("Nonzero statistics for an absent level")
            continue

        if level_requests != expected_requests or level_hits + level_misses != level_requests:
            raise ValueError("Incorrect level statistics")

        accumulated_hits += level_hits
        expected_requests = level_misses

    if (total_requests != request_count or total_hits != accumulated_hits
            or total_misses != expected_requests or total_hits + total_misses != total_requests):
        raise ValueError("Incorrect total statistics")


def run_cache_experiment(runner_path, config_path, input_path, request_count, level_count):
    try:
        process_result = subprocess.run(
            [str(runner_path), str(config_path), str(input_path)],
            capture_output=True, text=True, check=True,
        )
        statistics = [int(value) for value in process_result.stdout.split()]
        check_statistics(statistics, request_count, level_count)
        return statistics
    except subprocess.CalledProcessError as error:
        raise RuntimeError(
            f"{config_path.name}, {input_path.name}: "
            f"{error.stderr.strip() or error.stdout.strip() or error}"
        ) from error
    except ValueError as error:
        raise RuntimeError(f"{config_path.name}, {input_path.name}: {error}") from error


def calculate_average_access_cost(statistics):
    total_cost = statistics[2] * MEMORY_ACCESS_COST

    for level_index, access_cost in enumerate(LEVEL_ACCESS_COSTS):
        total_cost += statistics[3 + 3 * level_index] * access_cost

    return total_cost / statistics[0]


def calculate_hit_rate(statistics):
    return statistics[1] / statistics[0]


def calculate_slowdown_percent(access_cost, reference_cost):
    return 100 * (access_cost / reference_cost - 1)


def write_csv_result(csv_writer, pattern_name, random_seed, configuration_name,
                     statistics, ideal_statistics):
    access_cost = calculate_average_access_cost(statistics)
    ideal_cost = calculate_average_access_cost(ideal_statistics)

    csv_writer.writerow([
        pattern_name, random_seed, configuration_name, statistics[0],
        *statistics[3:], statistics[2], calculate_hit_rate(statistics), access_cost,
        ideal_statistics[2], calculate_hit_rate(ideal_statistics), ideal_cost,
        statistics[2] - ideal_statistics[2],
        calculate_slowdown_percent(access_cost, ideal_cost),
    ])


def format_configuration_result(configuration_name, score, metric_name,
                                 hit_rate=None, ideal_cost=None):
    result = f"{configuration_name} {metric_name}={score:.3f}"

    if hit_rate is not None:
        result += f" hit={100 * hit_rate:.3f}%"
    if ideal_cost is not None:
        result += f" vs REF={calculate_slowdown_percent(score, ideal_cost):.3f}%"

    return result


def print_best_configurations(configurations, scores, metric_name, result_count,
                              hit_rates=None, ideal_cost=None):
    ranked_indices = sorted(range(len(configurations)), key=lambda index: scores[index])

    for rank, configuration_index in enumerate(ranked_indices[:result_count], start=1):
        hit_rate = None if hit_rates is None else hit_rates[configuration_index]
        result = format_configuration_result(
            configurations[configuration_index][0], scores[configuration_index],
            metric_name, hit_rate, ideal_cost,
        )
        print(f"  {rank}. {result}")

    for configuration_index in ranked_indices:
        if len(configurations[configuration_index][2]) > 1:
            result = format_configuration_result(
                configurations[configuration_index][0], scores[configuration_index],
                metric_name, ideal_cost=ideal_cost,
            )
            print(f"  Best multi-level: {result}")
            break


def evaluate_request_pattern(pattern_name, configurations, arguments, csv_writer):
    print(f"\n{pattern_name} ...", flush=True)
    mean_costs = [0.0] * len(configurations)
    mean_hit_rates = [0.0] * len(configurations)
    mean_ideal_cost = 0.0
    mean_ideal_hit_rate = 0.0
    mean_ideal_misses = 0.0
    seed_count = len(arguments.seeds)

    for random_seed in arguments.seeds:
        ideal_statistics = run_cache_experiment(
            arguments.runner, get_config_path(arguments.root, "REF64"),
            get_input_path(arguments.root, pattern_name, random_seed, (64,)),
            arguments.requests, 1,
        )
        mean_ideal_cost += calculate_average_access_cost(ideal_statistics) / seed_count
        mean_ideal_hit_rate += calculate_hit_rate(ideal_statistics) / seed_count
        mean_ideal_misses += ideal_statistics[2] / seed_count
        write_csv_result(csv_writer, pattern_name, random_seed, "REF64",
                         ideal_statistics, ideal_statistics)

        for configuration_index, configuration in enumerate(configurations):
            configuration_name, strategies, capacities = configuration
            statistics = run_cache_experiment(
                arguments.runner, get_config_path(arguments.root, configuration_name),
                get_input_path(arguments.root, pattern_name, random_seed, capacities),
                arguments.requests, len(capacities),
            )
            if statistics[2] < ideal_statistics[2]:
                raise RuntimeError(
                    f"{pattern_name}, seed={random_seed}, {configuration_name}: "
                    "fewer misses than REF64; check cache implementations"
                )

            mean_costs[configuration_index] += calculate_average_access_cost(statistics) / seed_count
            mean_hit_rates[configuration_index] += calculate_hit_rate(statistics) / seed_count
            write_csv_result(csv_writer, pattern_name, random_seed, configuration_name,
                             statistics, ideal_statistics)

    print(f"  Ideal REF64 AMAT={mean_ideal_cost:.3f} "
          f"hit={100 * mean_ideal_hit_rate:.3f}% mean misses={mean_ideal_misses:.3f}")
    print_best_configurations(configurations, mean_costs, "AMAT", 3,
                              mean_hit_rates, mean_ideal_cost)
    return mean_costs


def run_cache_benchmark(configurations, arguments):
    mean_slowdown = [0.0] * len(configurations)
    print(f"{len(configurations)} configurations, {arguments.requests} requests, "
          f"{len(arguments.seeds)} seeds; cold start included.")
    print("Reference: offline REF64, one level; same requests and total capacity.")

    with arguments.output.open("w", newline="", encoding="utf-8") as output_file:
        csv_writer = csv.writer(output_file)
        csv_writer.writerow(CSV_COLUMNS)

        for pattern_name in REQUEST_PATTERNS:
            mean_costs = evaluate_request_pattern(pattern_name, configurations, arguments, csv_writer)
            best_pattern_cost = min(mean_costs)

            for configuration_index, mean_cost in enumerate(mean_costs):
                mean_slowdown[configuration_index] += (
                    calculate_slowdown_percent(mean_cost, best_pattern_cost) / len(REQUEST_PATTERNS)
                )

    print("\nTop 5 compromises: mean slowdown (%) vs each pattern's best")
    print_best_configurations(configurations, mean_slowdown, "slowdown(%)", 5)
    print(f"\nCSV: {arguments.output}")
    print("Best among these candidates under this cost model.")


def read_arguments():
    argument_parser = argparse.ArgumentParser(description="Generate cache traces and run benchmarks")
    argument_parser.add_argument("--runner", type=Path, help="Path to cache_benchmark_runner")
    argument_parser.add_argument("--root", type=Path, default=Path("./cacheBenchmarks"), help="Directory for configs/inputs")
    argument_parser.add_argument("--requests", type=int, default=20000)
    argument_parser.add_argument("--seeds", type=int, nargs="+", default=[1, 2, 3])
    argument_parser.add_argument("--output", type=Path, default=Path("./cacheBenchmarks/cache_benchmark_results.csv"))
    argument_parser.add_argument("--generate-only", action="store_true")
    arguments = argument_parser.parse_args()

    if not 1 <= arguments.requests <= 2147482623:
        argument_parser.error("--requests must be positive and fit the parser's integer range")
    if len(arguments.seeds) != len(set(arguments.seeds)):
        argument_parser.error("--seeds must contain distinct values")

    arguments.root = arguments.root.resolve()
    if not arguments.generate_only:
        if arguments.runner is None:
            argument_parser.error("Pass --runner or use --generate-only")
        arguments.runner = arguments.runner.resolve()
        if not arguments.runner.is_file():
            argument_parser.error(f"Runner not found: {arguments.runner}")

    return arguments


def main():
    arguments = read_arguments()
    configurations = create_cache_configurations()
    generate_benchmark_files(arguments.root, configurations, arguments.seeds, arguments.requests)

    if arguments.generate_only:
        input_count = len(REQUEST_PATTERNS) * len(arguments.seeds) * len(LEVEL_CAPACITIES)
        print(f"Generated {len(configurations) + 1} configs and {input_count} inputs in {arguments.root}")
    else:
        run_cache_benchmark(configurations, arguments)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as benchmark_error:
        raise SystemExit(f"Benchmark failed: {benchmark_error}")
