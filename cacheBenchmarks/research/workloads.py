"""Deterministic request sequences, independent of the tested cache capacity."""

import random
from pathlib import Path

PATTERN_LABELS = {
    "small_cycle": "Цикл 145 объектов",
    "boundary_cycle": "Цикл 291 объекта",
    "scan": "Длинный последовательный проход",
    "hot_cold": "Постоянное горячее множество",
    "hot_scan": "Горячие объекты и сканирование",
    "phase_change": "Смена горячего множества",
    "bursts": "Серии повторных запросов",
    "uniform": "Равномерные случайные запросы",
    "popular": "Неравномерная популярность",
    "navigation": "Переходы между страницами",
    "user_trace": "Последовательность из файла",
}

HIERARCHY_PATTERNS = tuple(list(PATTERN_LABELS)[:8])
SOFTWARE_PATTERNS = HIERARCHY_PATTERNS + ("popular", "navigation")


def pattern_label(pattern, scale):
    if pattern == "small_cycle":
        return f"Цикл {max(1, scale // 2)} объектов"
    if pattern == "boundary_cycle":
        return f"Цикл {scale + 1} объекта"
    return PATTERN_LABELS[pattern]


def generate_requests(pattern, seed, count, scale):
    """Scale describes the workload; it never changes during a capacity sweep."""
    generator = random.Random(seed)
    if pattern == "popular":
        population = range(4 * scale)
        weights = [1.0 / (rank + 1) for rank in population]
        return generator.choices(population, weights=weights, k=count)

    hot_size = max(1, scale // 8)
    requests = []
    previous_key = 0
    for index in range(count):
        if pattern == "small_cycle":
            key = index % max(1, scale // 2)
        elif pattern == "boundary_cycle":
            key = index % (scale + 1)
        elif pattern == "scan":
            key = index % (4 * scale)
        elif pattern in ("hot_cold", "phase_change"):
            phase_index = (index // 2000) % 4 if pattern == "phase_change" else 0
            if generator.randrange(100) < 90:
                key = phase_index * scale + generator.randrange(hot_size)
            else:
                key = 8 * scale + generator.randrange(4 * scale)
        elif pattern == "hot_scan":
            if index % 512 < 384:
                key = generator.randrange(hot_size)
            else:
                key = 8 * scale + index
        elif pattern == "bursts":
            key = generator.randrange(4 * scale) if index % 16 == 0 else previous_key
        elif pattern == "uniform":
            key = generator.randrange(4 * scale)
        elif pattern == "navigation":
            page_visit, resource_index = divmod(index, 16)
            site_index = (page_visit // 12) % 8
            if resource_index < 4:
                key = resource_index
            elif resource_index < 8:
                key = 4 + site_index * 4 + resource_index - 4
            else:
                key = 1000 + site_index * 48 + (page_visit % 6) * 8 + resource_index - 8
        else:
            raise ValueError(f"Unknown pattern: {pattern}")
        requests.append(key)
        previous_key = key
    return requests


def read_requests(path: Path):
    """Read whitespace-separated object IDs; preserve equality and request order."""
    identifiers = {}
    requests = []
    for token in path.read_text(encoding="utf-8").split():
        if token not in identifiers:
            identifiers[token] = len(identifiers)
        requests.append(identifiers[token])
    if not requests:
        raise ValueError("The request file is empty")
    return requests
