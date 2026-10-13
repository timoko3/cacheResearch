# Быстрый запуск одного случая

Нужен Python 3.10+; генерация не требует сборки C++, matplotlib или сторонних
Python-пакетов. Команды ниже выполняются из корня проекта (`python3` на Linux,
`python` на Windows).

| Команда | Что делает |
|---|---|
| `patterns` | Показывает паттерны выбранного набора |
| `generate` | Сохраняет один воспроизводимый случай, без запуска C++ |
| `run CASE` | Запускает сохранённый случай |
| `compare CASE` | Сравнивает стратегии на одной последовательности |
| `research` | Проверяет выбранные паттерны и размеры, сохраняет CSV |
| `search` | Точный подбор системы из исследования №3 |
| `doctor` | Проверяет Python и наличие runner, ничего не устанавливает |

У каждой команды есть `--help`. Runner ищется сначала в Release, затем в Debug
в стандартных CMake-каталогах текущей ОС. Можно указать `--runner PATH` для
любого другого расположения. Относительные пути `--output` и `CASE` считаются
от текущей рабочей папки; сама команда работает и из другой папки.

Список паттернов нового исследования:

```sh
python3 cacheBenchmarks/benchmark.py patterns
```

Сгенерировать только один паттерн с заданными параметрами:

```sh
python3 cacheBenchmarks/benchmark.py generate --pattern hot_cold --seed 3 --requests 20000 --workload-scale 290 --output cacheBenchmarks/local_data/my_case
```

Будут сохранены:

- `requests.txt` — только ключи запросов, разделённые пробелами;
- `input.txt` — ёмкости, число запросов и запросы для C++ runner;
- `config.txt` — стратегии (по умолчанию один LRU ёмкостью 64);
- `manifest.json` — набор генераторов, паттерн, seed, параметры и SHA-256.

Одинаковые suite, pattern, seed, requests и workload-scale дают одинаковую
последовательность для той же версии генератора/Python. Размеры кешей не влияют
на генерацию. Для гарантированного повтора сохраните весь каталог случая:
команда `run` читает уже сохранённые файлы и проверяет их SHA-256.
Существующие файлы случая не перезаписываются без `--force`.

Для воспроизведения исследования смотрите параметры в его manifest: у
`cache_research.py` и `cache_unrestricted.py` используется `--suite research`
(это значение по умолчанию), у исходного `cache_benchmark.py` — `--suite legacy`.
У legacy фиксированные размеры паттернов; `--workload-scale` для него запрещён.
Имена могут совпадать, но последовательности этих двух наборов различаются.

```sh
python3 cacheBenchmarks/benchmark.py patterns --suite legacy
python3 cacheBenchmarks/benchmark.py generate --suite legacy --pattern boundary_cycle --seed 1 --requests 20000 --output cacheBenchmarks/local_data/legacy_cycle
```

Пример трёхуровневой системы (порядок от первого уровня к последнему):

```sh
python3 cacheBenchmarks/benchmark.py generate --pattern hot_scan --seed 1 --capacities 8 16 40 --strategies LRU 2Q ARC --output cacheBenchmarks/local_data/three_levels
```

Собрать runner и запустить сохранённый случай на Linux:

```sh
cmake --preset release
cmake --build --preset release --target cache_benchmark_runner --parallel
python3 cacheBenchmarks/benchmark.py run cacheBenchmarks/local_data/my_case --runner build/release/cache_benchmark_runner
```

На Windows с Ninja путь к runner:
`build/release/cache_benchmark_runner.exe`. Для другого генератора
укажите фактический путь через `--runner`.

Результат — читаемая сводка с запросами, попаданиями, промахами и статистикой
каждого уровня. Добавьте `--json` для обработки результата программой.
Можно запустить C++ runner напрямую:

```sh
./build/release/cache_benchmark_runner cacheBenchmarks/local_data/my_case/config.txt cacheBenchmarks/local_data/my_case/input.txt
```

Старые команды `cache_benchmark.py`, `cache_research.py` и
`cache_unrestricted.py` сохранены. Их `--generate-only` не заменяет эту команду:
например, у `cache_research.py` этот флаг записывает только план исследования.

## Сравнить алгоритмы на одинаковых запросах

```sh
python3 cacheBenchmarks/benchmark.py compare cacheBenchmarks/local_data/my_case --capacity 64
python3 cacheBenchmarks/benchmark.py compare cacheBenchmarks/local_data/my_case --capacity 16 --strategies LRU ARC REF --json
```

Сравнение одноуровневое: каждый алгоритм получает одинаковые ёмкость и запросы,
с пустым кешем в начале. REF включён как эталон. Исходный случай не меняется;
временные config/input-файлы удаляются по завершении. При ёмкости 1 укажите
стратегии без 2Q и LIRS, которым нужны хотя бы два слота.

## Запустить небольшое исследование

По умолчанию `research` запускает только `hot_cold`, seed 1 и ёмкость 64,
без графиков. Это существенно меньше полной серии старых скриптов.

```sh
python3 cacheBenchmarks/benchmark.py research --patterns hot_cold bursts --sizes 16 32 64 --seeds 1 2 --requests 5000 --output cacheBenchmarks/local_data/sweep
```

В `sweep/software` появятся `raw.csv`, `summary.csv`, `comparison_with_lru.csv`
и `manifest.json`. Для графиков добавьте `--plots` (нужен matplotlib).
Существующие результаты защищены; для повторного использования папки нужно
явно добавить `--force` или выбрать другую папку.

Для иерархий:

```sh
python3 cacheBenchmarks/benchmark.py research --mode hierarchy --patterns hot_scan --requests 1000 --output cacheBenchmarks/local_data/hierarchy
```

Используется исходная модель стоимости и ограничения иерархии из исследования
№2. Без графиков сохраняются `raw.csv`, `summary.csv` и `manifest.json`.
Полный контроль кандидатов размеров и лимитов остаётся в `cache_research.py`;
теперь там тоже доступны `--patterns NAME ...` и `--no-plots`.

До запуска исследования можно сохранить только план (runner не требуется):

```sh
python3 cacheBenchmarks/benchmark.py research --patterns uniform --sizes 16 64 --plan --output cacheBenchmarks/local_data/plan
```

## Исследование №3: точный подбор

Точный подбор подключён к обычной CMake-сборке:

```sh
cmake --preset release
cmake --build --preset release --target cache_exact_search --parallel
python3 cacheBenchmarks/benchmark.py search --patterns hot_cold --totals 8 --requests 1000 --output cacheBenchmarks/local_data/exact
```

По умолчанию выбран один паттерн, seed 1, 1000 запросов, общий объём 8 и один
worker. Начинайте с небольших объёмов: точный перебор сильно дорожает с ростом
ёмкости. Доступны `--max-levels`, `--workers`, `--seeds` и `--workload-scale`.
Числовые результаты сохраняются без matplotlib, графики включаются `--plots`.
Этот режим использует `cache_exact_search`, а остальные — `cache_benchmark_runner`.

## Частые вопросы

- Runner не найден: выполните две команды сборки выше или укажите `--runner`.
- Один и тот же seed даёт разные запросы: проверьте `suite`, число запросов,
  workload-scale, версию Python и `generator_sha256` в manifest.
- Файлы случая изменены: `run` проверяет их контрольные суммы. Чтобы поменять
  стратегии или ёмкости, сгенерируйте новый случай соответствующими флагами.
- Нужно только содержимое последовательности: используйте `requests.txt`, а
  не `input.txt`, который содержит также ёмкости и число запросов.
- Все новые команды, кроме явного `--plots`, используют только стандартную
  библиотеку Python. Генерация и подготовка плана не запускают C++.
