## Генерация запросов и запуск отдельных экспериментов

Скрипт `cacheBenchmarks/benchmark.py` позволяет создать последовательность
запросов, сохранить её вместе с конфигурацией кеша, запустить симуляцию
и сравнить алгоритмы.

Все команды выполняются **из корня репозитория**.
В примерах используется `python`; на Linux при необходимости замените его
на `python3`.

| Команда | Назначение |
|---|---|
| `patterns` | Показать доступные виды последовательностей запросов |
| `generate` | Создать файлы запросов и конфигурации кеша |
| `run CASE` | Запустить симуляцию с файлами из папки `CASE` |
| `compare CASE` | Сравнить алгоритмы на запросах из папки `CASE` |
| `research` | Сравнить алгоритмы при выбранных вместимостях и сохранить результаты в CSV |
| `search` | Подобрать размеры уровней и алгоритмы для заданной общей вместимости |
| `doctor` | Проверить Python и наличие программы симуляции |

У каждой команды есть `--help`, например:

```sh
python cacheBenchmarks/benchmark.py generate --help
```

### 1. Подготовка

Нужен **Python 3.10 или новее**.
Генерация запросов и сохранение плана исследования не требуют сборки C++
и сторонних Python-пакетов.

Для симуляции нужна программа `cache_benchmark_runner`,
для точного подбора системы — `cache_exact_search`:

```sh
cmake --preset benchmarks
cmake --build --preset benchmarks --parallel
```

Графики создаются только при указании `--plots`. Для них нужен matplotlib:

```sh
python -m pip install matplotlib
```

Проверить Python и наличие программы симуляции:

```sh
python cacheBenchmarks/benchmark.py doctor
```

Команда только выполняет проверку, ничего не устанавливая.

### 2. Генерация запросов и конфигурации кеша

Посмотреть доступные виды запросов:

```sh
python cacheBenchmarks/benchmark.py patterns
```

Создать 20 000 запросов для паттерна `hot_cold`:

```sh
python cacheBenchmarks/benchmark.py generate --pattern hot_cold --seed 3 --requests 20000 --workload-scale 290 --output cacheBenchmarks/local_data/my_case
```

По умолчанию создаётся конфигурация одноуровневого кеша LRU вместимостью 64.

| Параметр | Назначение |
|---|---|
| `--pattern NAME` | Вид последовательности запросов |
| `--seed N` | Начальное значение генератора случайных чисел |
| `--requests N` | Число запросов |
| `--workload-scale N` | Управляет размерами наборов страниц при генерации; не задаёт вместимость кеша |
| `--capacities N ...` | Вместимости уровней кеша от первого к последнему |
| `--strategies NAME ...` | Алгоритмы уровней в том же порядке |
| `--output PATH` | Папка для созданных файлов |

Например, создать конфигурацию из трёх уровней:
LRU вместимостью 8, 2Q вместимостью 16 и ARC вместимостью 40:

```sh
python cacheBenchmarks/benchmark.py generate --pattern hot_scan --seed 1 --capacities 8 16 40 --strategies LRU 2Q ARC --output cacheBenchmarks/local_data/three_levels
```

В указанной папке сохраняются:

| Файл | Содержимое |
|---|---|
| `requests.txt` | Ключи запросов, разделённые пробелами |
| `input.txt` | Вместимости уровней, число запросов и ключи для C++ программы |
| `config.txt` | Алгоритмы уровней |
| `manifest.json` | Параметры генерации и контрольные суммы файлов |

**Повторение эксперимента.** Сохраните всю папку с этими четырьмя файлами.
Команда `run` использует готовые файлы и проверяет по контрольным суммам,
что их содержимое не изменилось.

При одинаковых `suite`, `pattern`, `seed`, `requests` и `workload-scale`
генератор создаёт одинаковые запросы, если версии генератора и Python
также совпадают. Вместимости кешей на генерацию не влияют.

Существующие файлы не перезаписываются без `--force`.

**Наборы генераторов.** В `cache_research.py` и `cache_unrestricted.py`
по умолчанию используется набор `research`, а в `cache_benchmark.py` —
`legacy`. Для повторения сохранённого исследования выбирайте набор,
указанный в его `manifest.json`.

Посмотреть паттерны набора `legacy` и создать запросы:

```sh
python cacheBenchmarks/benchmark.py patterns --suite legacy
python cacheBenchmarks/benchmark.py generate --suite legacy --pattern boundary_cycle --seed 1 --requests 20000 --output cacheBenchmarks/local_data/legacy_cycle
```

У `legacy` размеры наборов страниц фиксированы, поэтому `--workload-scale`
для него запрещён. Паттерны с одинаковыми именами в `research` и `legacy`
могут создавать разные последовательности.

### 3. Запуск сохранённого эксперимента

Запустить симуляцию с созданными файлами:

```sh
python cacheBenchmarks/benchmark.py run cacheBenchmarks/local_data/my_case
```

Скрипт автоматически ищет `cache_benchmark_runner` сначала в Release,
затем в Debug в стандартных каталогах сборки текущей ОС.

Если программа находится в другом месте, укажите путь через `--runner`.

**Linux:**

```sh
python cacheBenchmarks/benchmark.py run cacheBenchmarks/local_data/my_case --runner build/release/cache_benchmark_runner
```

**Windows:**

```sh
python cacheBenchmarks/benchmark.py run cacheBenchmarks/local_data/my_case --runner build/release/cache_benchmark_runner.exe
```

Результат содержит число запросов, попаданий и промахов, а также статистику
каждого уровня. Для вывода в формате JSON добавьте `--json`.

Относительные пути `CASE` и `--output` отсчитываются от текущей рабочей папки.

Программу симуляции можно запустить напрямую, передав конфигурацию и запросы:

```sh
./build/benchmarks/cache_benchmark_runner cacheBenchmarks/local_data/my_case/config.txt cacheBenchmarks/local_data/my_case/input.txt
```

### 4. Сравнение алгоритмов на одной последовательности

Сравнить поддерживаемые алгоритмы при вместимости 64:

```sh
python cacheBenchmarks/benchmark.py compare cacheBenchmarks/local_data/my_case --capacity 64
```

Сравнить только LRU, ARC и REF при вместимости 16 и вывести результат в JSON:

```sh
python cacheBenchmarks/benchmark.py compare cacheBenchmarks/local_data/my_case --capacity 16 --strategies LRU ARC REF --json
```

Каждый алгоритм получает одинаковые запросы и вместимость.
Кеши в начале пусты; сравнение одноуровневое. REF используется как эталон.

Файлы исходного эксперимента сохраняются без изменений.
Временные файлы конфигурации и запросов удаляются после сравнения.

При вместимости 1 укажите алгоритмы без 2Q и LIRS:
этим реализациям нужна вместимость не менее 2.

### 5. Исследование нескольких вместимостей и видов запросов

По умолчанию команда `research` проверяет паттерн `hot_cold`
при вместимости 64 и начальном значении генератора 1. Графики не создаются.

Проверить два паттерна, три вместимости и два начальных значения генератора:

```sh
python cacheBenchmarks/benchmark.py research --patterns hot_cold bursts --sizes 16 32 64 --seeds 1 2 --requests 5000 --output cacheBenchmarks/local_data/sweep
```

В папке `sweep/software` сохраняются:

- `raw.csv` — результаты отдельных запусков;
- `summary.csv` — сводная таблица;
- `comparison_with_lru.csv` — сравнение с LRU;
- `manifest.json` — параметры исследования.

Для создания графиков добавьте `--plots`.
Для записи в папку с существующими результатами нужен `--force`;
также можно выбрать другую папку.

**Многоуровневые системы.** Для исследования с моделью стоимости
и ограничениями уровней из исследования №2 укажите `--mode hierarchy`:

```sh
python cacheBenchmarks/benchmark.py research --mode hierarchy --patterns hot_scan --requests 1000 --output cacheBenchmarks/local_data/hierarchy
```

Сохраняются `raw.csv`, `summary.csv` и `manifest.json`.
Для задания проверяемых размеров уровней и пределов их вместимости
используйте `cache_research.py`. В нём также доступны
`--patterns NAME ...` и `--no-plots`.

**План без запуска симуляции.** Добавьте `--plan`:

```sh
python cacheBenchmarks/benchmark.py research --patterns uniform --sizes 16 64 --plan --output cacheBenchmarks/local_data/plan
```

Для этой команды C++ программа не требуется.

### 6. Точный подбор системы: исследование №3

Команда `search` подбирает вместимости уровней и алгоритмы
при заданной общей вместимости:

```sh
python cacheBenchmarks/benchmark.py search --patterns hot_cold --totals 8 --requests 1000 --output cacheBenchmarks/local_data/exact
```

По умолчанию проверяется паттерн `hot_cold`, начальное значение генератора 1,
1000 запросов и общая вместимость 8. Используется один рабочий процесс.

| Параметр | Назначение |
|---|---|
| `--patterns NAME ...` | Проверяемые виды запросов |
| `--totals N ...` | Общие вместимости кеша |
| `--max-levels N` | Максимальное число уровней |
| `--requests N` | Число запросов |
| `--seeds N ...` | Начальные значения генератора |
| `--workload-scale N` | Управляет размерами наборов страниц при генерации |
| `--workers N` | Число рабочих процессов |
| `--output PATH` | Папка результатов |
| `--plots` | Создать графики; требуется matplotlib |

Начинайте с небольших вместимостей: при их увеличении точный перебор
может занимать много времени.

Числовые результаты сохраняются без matplotlib.
Эта команда использует `cache_exact_search`, а команды `run`, `compare`
и симуляции в `research` — `cache_benchmark_runner`.

Скрипты `cache_benchmark.py`, `cache_research.py` и `cache_unrestricted.py`
остаются доступны. Их параметр `--generate-only` не заменяет команду
`benchmark.py generate`: например, у `cache_research.py` он сохраняет
только план исследования.
