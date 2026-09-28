"""Static figures for cache experiments; no report text generation."""

import math

from cache_benchmark import CACHE_STRATEGIES

from .model import best_rows
from .workloads import pattern_label


def plot_software(folder, summary, patterns, scale):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    filenames = []
    for page, start in enumerate(range(0, len(patterns), 6), start=1):
        selected = patterns[start : start + 6]
        row_count = math.ceil(len(selected) / 2)
        figure, axes = plt.subplots(row_count, 2, figsize=(13, 3.7 * row_count), squeeze=False)
        for axis, pattern in zip(axes.flat, selected):
            for strategy in CACHE_STRATEGIES:
                rows = sorted(
                    (
                        row
                        for row in summary
                        if row["pattern"] == pattern and row["strategies"] == strategy
                    ),
                    key=lambda row: int(row["capacities"]),
                )
                if rows:
                    axis.plot(
                        [int(row["capacities"]) for row in rows],
                        [row["misses_per_1000"] for row in rows],
                        marker=".",
                        linewidth=1.3,
                        label=strategy,
                    )
            reference = sorted(
                (
                    row
                    for row in summary
                    if row["pattern"] == pattern and row["strategies"] == "LRU"
                ),
                key=lambda row: int(row["capacities"]),
            )
            axis.plot(
                [int(row["capacities"]) for row in reference],
                [row["reference_misses_per_1000"] for row in reference],
                color="black",
                linestyle="--",
                linewidth=1.2,
                label="REF",
            )
            axis.set_title(pattern_label(pattern, scale), fontsize=11)
            axis.set_xlabel("Вместимость кеша, страниц")
            axis.set_ylabel("Загрузок на 1000 запросов")
            axis.set_xscale("log", base=2)
            shown_sizes = [int(row["capacities"]) for row in reference]
            ticks = [
                tick
                for tick in (1, 4, 16, 64, 256, 1024)
                if min(shown_sizes) <= tick <= max(shown_sizes)
            ]
            if len(ticks) < 2:
                ticks = shown_sizes
            axis.set_xticks(ticks, labels=[str(tick) for tick in ticks])
            axis.set_ylim(bottom=0)
            axis.grid(alpha=0.25)
        for axis in list(axes.flat)[len(selected) :]:
            axis.set_visible(False)
        handles, labels = axes.flat[0].get_legend_handles_labels()
        figure.legend(handles, labels, loc="upper center", ncol=6)
        figure.tight_layout(rect=(0, 0, 1, 0.96))
        filename = f"size_curves_{page}.png"
        figure.savefig(folder / filename, dpi=140)
        plt.close(figure)
        filenames.append(filename)
    focused = plot_size_details(folder, summary, patterns, scale)
    return ([focused] if focused else []) + filenames


def plot_size_details(folder, summary, patterns, scale):
    """Show policy crossings and the working-set boundary without a wide log axis."""
    import matplotlib.pyplot as plt

    selected = ("phase_change", "boundary_cycle")
    if not all(pattern in patterns for pattern in selected):
        return None
    ranges = ((max(1, scale // 8 - 4), scale + 1), (scale - 1, scale + 2))
    for pattern, (low, high) in zip(selected, ranges):
        sizes = {
            int(row["capacities"])
            for row in summary
            if row["pattern"] == pattern and low <= int(row["capacities"]) <= high
        }
        if len(sizes) < 2:
            return None
    figure, axes = plt.subplots(1, 2, figsize=(13, 4.5))
    for axis, pattern, (low, high) in zip(axes, selected, ranges):
        for strategy in CACHE_STRATEGIES:
            rows = sorted(
                (
                    row
                    for row in summary
                    if row["pattern"] == pattern
                    and row["strategies"] == strategy
                    and low <= int(row["capacities"]) <= high
                ),
                key=lambda row: int(row["capacities"]),
            )
            axis.plot(
                [int(row["capacities"]) for row in rows],
                [row["misses_per_1000"] for row in rows],
                marker=".",
                label=strategy,
            )
        reference = sorted(
            (
                row
                for row in summary
                if row["pattern"] == pattern
                and row["strategies"] == "LRU"
                and low <= int(row["capacities"]) <= high
            ),
            key=lambda row: int(row["capacities"]),
        )
        axis.plot(
            [int(row["capacities"]) for row in reference],
            [row["reference_misses_per_1000"] for row in reference],
            color="black",
            linestyle="--",
            label="REF",
        )
        axis.set_title(pattern_label(pattern, scale), fontsize=11)
        axis.set_xlabel("Вместимость кеша, страниц")
        axis.set_ylabel("Загрузок на 1000 запросов")
        axis.set_ylim(bottom=0)
        axis.grid(alpha=0.25)
    axes[1].set_xticks(sorted({int(row["capacities"]) for row in reference}))
    figure.legend(*axes[0].get_legend_handles_labels(), loc="upper center", ncol=6)
    figure.tight_layout(rect=(0, 0, 1, 0.91))
    filename = "size_details.png"
    figure.savefig(folder / filename, dpi=140)
    plt.close(figure)
    return filename


def plot_hierarchy(folder, summary, patterns, scale):
    """Show complete winning tuples at the same capacities for every pattern."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.colors import ListedColormap

    layouts = sorted({row["capacities"] for row in summary})
    capacities = "2+32+256" if "2+32+256" in layouts else layouts[0]
    winners = []
    tied_rows = []
    for pattern in patterns:
        rows = [
            row for row in summary if row["pattern"] == pattern and row["capacities"] == capacities
        ]
        tied = sorted(best_rows(rows, "mean_access_cost"), key=lambda row: row["strategies"])
        winners.append(tied[0])
        tied_rows.extend(tied)
    colors = ["#cfe2f3", "#fce5cd", "#d9ead3", "#ead1dc", "#fff2cc"]
    values = [
        [CACHE_STRATEGIES.index(policy) for policy in row["strategies"].split("+")]
        for row in winners
    ]
    figure, axis = plt.subplots(figsize=(11, 0.64 * len(patterns) + 1.8))
    axis.imshow(values, cmap=ListedColormap(colors), vmin=-0.5, vmax=4.5, aspect="auto")
    sizes = capacities.split("+")
    axis.set_xticks(
        range(len(sizes)), [f"L{index + 1}: {size} стр." for index, size in enumerate(sizes)]
    )
    axis.set_yticks(range(len(patterns)), [pattern_label(pattern, scale) for pattern in patterns])
    for row_index, row in enumerate(winners):
        for level, policy in enumerate(row["strategies"].split("+")):
            axis.text(level, row_index, policy, ha="center", va="center", fontsize=12)
    axis.set_title(
        "Лучшее сочетание алгоритмов при одинаковых размерах уровней\n"
        "Минимум средней стоимости; при равенстве показан один вариант",
        fontsize=12,
        pad=16,
    )
    axis.tick_params(length=0)
    for spine in axis.spines.values():
        spine.set_visible(False)
    figure.tight_layout()
    figure.savefig(folder / "strategies_by_level.png", dpi=150)
    plt.close(figure)
    return tied_rows


def plot_unrestricted(folder, results, metadata):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    patterns = metadata["patterns"]
    figure, axes = plt.subplots(
        (len(patterns) + 1) // 2, 2, figsize=(13, 3.4 * ((len(patterns) + 1) // 2)), squeeze=False
    )
    requests_in_thousands = metadata["requests"] * len(metadata["seeds"]) / 1000
    for axis, pattern in zip(axes.flat, patterns):
        rows = sorted(
            (row for row in results if row["pattern"] == pattern), key=lambda row: row["total"]
        )
        total_capacities = metadata["totals"]
        by_total = {row["total"]: row for row in rows}

        def misses_per_thousand(field):
            values = []
            for total in total_capacities:
                if total not in by_total:
                    values.append(float("nan"))
                    continue

                result = by_total[total]
                misses = (
                    result["reference_misses"] if field == "reference" else result[field]["misses"]
                )
                values.append(misses / requests_in_thousands)
            return values

        axis.plot(
            total_capacities,
            misses_per_thousand("single"),
            "o-",
            color="#c15b13",
            label="Лучший один уровень",
            markersize=5,
        )
        axis.plot(
            total_capacities,
            misses_per_thousand("best"),
            ".-",
            color="#1766a3",
            label=f'Лучший из 1–{metadata["max_levels"]} уровней',
        )
        axis.plot(
            total_capacities, misses_per_thousand("reference"), "--", color="#333333", label="REF"
        )
        missing = [str(total) for total in total_capacities if total not in by_total]
        if missing:
            axis.text(
                0.98,
                0.06,
                "Не рассчитано: " + ", ".join(missing),
                transform=axis.transAxes,
                ha="right",
                fontsize=8,
                color="#555555",
            )
        axis.set_title(pattern_label(pattern, metadata["workload_scale"]), fontsize=10)
        axis.set_xlabel("Суммарная вместимость, страниц")
        axis.set_ylabel("Загрузок на 1000 запросов")
        axis.set_xscale("log", base=2)
        axis.set_xticks(total_capacities, [str(value) for value in total_capacities], fontsize=8)
        axis.set_ylim(bottom=0)
        axis.grid(alpha=0.2)
    for axis in list(axes.flat)[len(patterns) :]:
        axis.set_visible(False)
    figure.legend(*axes.flat[0].get_legend_handles_labels(), loc="upper center", ncol=3)
    figure.tight_layout(rect=(0, 0, 1, 0.97))
    figure.savefig(folder / "best_by_total.png", dpi=140)
    plt.close(figure)


def plot_configurations(folder, results, metadata):
    """Show the chosen joint configuration, not independent winners per level."""
    import matplotlib.pyplot as plt
    from matplotlib.colors import ListedColormap

    lookup = {(item["pattern"], item["total"]): item["best"] for item in results}
    patterns, totals = metadata["patterns"], metadata["totals"]
    values = [
        [
            (
                len(lookup[pattern, total]["capacities"])
                if (pattern, total) in lookup
                else float("nan")
            )
            for total in totals
        ]
        for pattern in patterns
    ]
    figure, axis = plt.subplots(figsize=(max(10, 2 * len(totals) + 3), 0.82 * len(patterns) + 1.6))
    axis.set_facecolor("#eeeeee")
    axis.imshow(
        values,
        cmap=ListedColormap(["#e4edf4", "#fce3c3", "#cce7d7"]),
        vmin=0.5,
        vmax=3.5,
        aspect="auto",
    )
    axis.set_xticks(range(len(totals)), [str(total) for total in totals])
    axis.set_yticks(
        range(len(patterns)),
        [pattern_label(pattern, metadata["workload_scale"]) for pattern in patterns],
    )
    axis.set_xlabel("Суммарная вместимость, страниц")
    axis.set_title(
        "Выбранные алгоритмы и размеры: сверху вниз L1 → L2 → L3\n"
        "Синий: 1 уровень; оранжевый: 2; зелёный: 3. При равенстве показан один вариант.",
        fontsize=11,
        pad=15,
    )
    for row, pattern in enumerate(patterns):
        for column, total in enumerate(totals):
            configuration = lookup.get((pattern, total))
            label = (
                "\n".join(
                    f"{policy}({capacity})"
                    for policy, capacity in zip(
                        configuration["policies"], configuration["capacities"]
                    )
                )
                if configuration
                else "Не рассчитано"
            )
            axis.text(column, row, label, ha="center", va="center", fontsize=9)
    axis.tick_params(length=0)
    for spine in axis.spines.values():
        spine.set_visible(False)
    figure.tight_layout()
    figure.savefig(folder / "configurations_by_total.png", dpi=150)
    plt.close(figure)
