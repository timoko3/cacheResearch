# Cache Research

**[Результаты исследования и отчёты](cacheBenchmarks/reports/README.md)** —
описание экспериментов, основные выводы и подробные результаты.

Этот README описывает сборку и запуск. Все команды выполняются **из корня
репозитория**.

## 1. Подготовка

Нужны компилятор C++17, CMake 3.24 или новее, Ninja и Python 3.
В примерах Python запускается командой `python`; если в системе используется
`python3`, замените её во всех командах.

Инициализируйте подмодули и установите библиотеку графиков:

```sh
git submodule update --init --recursive
python -m pip install matplotlib
```

## 2. Сборка бенчмарков

```sh
cmake --preset release
cmake --build --preset release --target cache_benchmark_runner cache_exact_search cache_tests --parallel
```

Эта команда собирает два исполнителя для экспериментов и C++ тесты.
Пути зависят от операционной системы. Задайте переменные для последующих
команд **в той же сессии терминала**.

**Windows, PowerShell:**

```powershell
$buildDir = "build/Windows/release"
$runner = "$buildDir/cache_benchmark_runner.exe"
$exact = "$buildDir/cache_exact_search.exe"
```

**Linux, Bash:**

```bash
buildDir="build/Linux/release"
runner="$buildDir/cache_benchmark_runner"
exact="$buildDir/cache_exact_search"
```

Дальнейшие команды с `"$runner"`, `"$exact"` и `"$buildDir"` работают
с этими переменными. Если сборка уже находится в другом каталоге, например
`build/Windows/research`, укажите его в переменных.

В Linux без Ninja можно использовать пресет `release-make` в обеих командах
сборки и каталог `build/Linux/release-make`.

## 3. Короткая проверка запуска

Небольшой одноуровневый эксперимент:

```sh
python cacheBenchmarks/cache_research.py --mode software --runner "$runner" --root build/bench-smoke --requests 200 --seeds 1 --sizes 8 16
```

В `build/bench-smoke/software/` появятся CSV с измерениями и сводками,
графики и параметры запуска.

## 4. Запуск экспериментов

Примеры сохраняют новые результаты в `build/bench-results/`.
Так они не заменяют опубликованные таблицы и рисунки в `cacheBenchmarks/reports/`.

### Влияние размера кеша на выбор алгоритма

```sh
python cacheBenchmarks/cache_research.py --mode software --runner "$runner" --root build/bench-results
```

По умолчанию: 30 вместимостей, 10 видов запросов, 20 000 обращений
и пять начальных значений генератора. Результаты — в `build/bench-results/software/`.

### Иерархия, приближённая к процессорной

```sh
python cacheBenchmarks/cache_research.py --mode hierarchy --runner "$runner" --root build/bench-results
```

По умолчанию: три уровня, общий объём 290 страниц, пределы вместимости
4/64/256, восемь видов запросов, 20 000 обращений и пять начальных
значений генератора. Результаты — в `build/bench-results/hierarchy/`.

Параметр `--mode all` запускает оба этих эксперимента.

| Параметр `cache_research.py` | Назначение |
|---|---|
| `--root PATH` | Общая папка результатов; без указания — `cacheBenchmarks/reports` |
| `--requests N` | Число запросов; по умолчанию 20 000 |
| `--seeds N ...` | Начальные значения генератора; по умолчанию 1 2 3 4 5 |
| `--sizes N ...` | Вместимости одноуровневого кеша |
| `--total N` | Общая вместимость иерархии; по умолчанию 290 |
| `--level-limits N N N` | Верхние границы размеров L1, L2 и L3 |
| `--l1-sizes N ...`, `--l2-sizes N ...` | Проверяемые размеры первых двух уровней; L3 получает остаток |
| `--workload-scale N` | Масштаб генерации запросов; по умолчанию 290, независимо от вместимости кеша |
| `--trace PATH` | Собственная последовательность для режима `software` |
| `--generate-only` | Сохранить план опыта без запуска симулятора |

### Иерархия с бесплатным обращением к уровням

Сначала можно выполнить небольшой поиск:

```sh
python cacheBenchmarks/cache_unrestricted.py --runner "$exact" --output build/bench-smoke/unrestricted --patterns uniform --totals 4 8 --requests 200
```

Полная серия запускается отдельно:

```sh
python cacheBenchmarks/cache_unrestricted.py --runner "$exact" --output build/bench-results/unrestricted
```

По умолчанию проверяются общие объёмы 8, 16, 36, 64, 145 и 290,
от одного до трёх уровней, 10 видов запросов и последовательности
из 6000 обращений с начальным значением 1.
Точный поиск на больших объёмах может занимать много времени.
Команда запрашивает все 60 сочетаний, включая две точки,
для которых в опубликованном исследовании результатов нет.

| Параметр `cache_unrestricted.py` | Назначение |
|---|---|
| `--output PATH` | Папка этой серии; без указания — `cacheBenchmarks/reports/unrestricted` |
| `--patterns NAME ...` | Виды запросов, например `uniform popular navigation`; полный список — в `--help` |
| `--totals N ...` | Общие вместимости |
| `--max-levels 1\|2\|3` | Максимальное число уровней; по умолчанию 3 |
| `--requests N`, `--seeds N ...` | Длина и начальные значения последовательностей для подбора |
| `--workload-scale N` | Масштаб генерации запросов; по умолчанию 290 |
| `--workers N` | Число одновременно рассчитываемых видов запросов; по умолчанию 2 |

### Проверка выбранных конфигураций и пересчёт графиков

После завершённого поиска можно сравнить выбранные конфигурации на новых
последовательностях без повторного подбора:

```sh
python cacheBenchmarks/cache_unrestricted.py --validate-with "$runner" --validation-seeds 2 3 --output build/bench-results/unrestricted
```

Начальные значения проверки должны отличаться от использованных при подборе.
Результат сохраняется в `validation.csv`.

Пересчитать сводную таблицу и рисунки по сохранённым результатам:

```sh
python cacheBenchmarks/cache_unrestricted.py --render-only --output build/bench-results/unrestricted
```

## 5. Собственная последовательность запросов

Создайте текстовый файл, например `requests.txt`:

```text
a b a c b d a
```

Каждое слово обозначает объект; одинаковые слова — один и тот же объект.
Количество запросов в начале файла указывать не нужно.

```sh
python cacheBenchmarks/cache_research.py --mode software --runner "$runner" --trace requests.txt --sizes 2 4 8 --root build/custom-trace
```

Последовательность используется целиком. Параметры `--requests` и `--seeds`
не меняют запросы из файла.

## 6. Тесты

C++ тесты:

```sh
ctest --test-dir "$buildDir" --output-on-failure
```

Python тесты запускаются из корня проекта. Для интеграционных проверок
укажите оба исполнителя.

**Windows, PowerShell:**

```powershell
$env:CACHE_RESEARCH_RUNNER = (Resolve-Path $runner).Path
$env:CACHE_EXACT_SEARCH = (Resolve-Path $exact).Path
python -m unittest discover -s cacheBenchmarks -p "test_*.py" -v
```

**Linux, Bash:**

```bash
CACHE_RESEARCH_RUNNER="$runner" CACHE_EXACT_SEARCH="$exact" python -m unittest discover -s cacheBenchmarks -p "test_*.py" -v
```

Без этих переменных модульные проверки выполняются, а интеграционные
явно пропускаются. Обнаружение тестов начинается с `cacheBenchmarks`,
хотя сами файлы находятся в `cacheBenchmarks/tests/`.

## 7. Структура кода и результатов

```text
cacheBenchmarks/
  cache_benchmark.py       исходный эксперимент на 64 страницы
  cache_research.py        одноуровневый и процессорный эксперименты
  cache_unrestricted.py    точный поиск и проверка выбранных конфигураций
  research/
    model.py              ограничения и метрики
    reference.py          эталон REF
    workloads.py          последовательности запросов
    runner.py             взаимодействие с C++ симулятором
    results.py            сводные таблицы и сохранение результатов
    plots.py              графики
  tests/                  Python тесты
  reports/                отчёты и результаты
```

Одноуровневый и процессорный эксперименты сохраняют `raw.csv`,
`summary.csv`, рисунки.
Точный поиск сохраняет `results.json`, `summary.csv`, рисунки.
проверка на новых запросах дополнительно создаёт `validation.csv`.

Папки `work/` и `inputs/` содержат служебные файлы запуска.

## 8. Обычный запуск

Перед сборкой создайте в корне папки `configs/` и `inputs/`:
CMake копирует их в каталог приложения. Сохраните в них следующие файлы.

`configs/config_1.txt` — число уровней и алгоритмы сверху вниз:

```text
4
LFU
LRU
LIRS
2Q
```

`inputs/input_1.txt` — вместимости уровней, число запросов и идентификаторы:

```text
3 4 5 7
5
1 2 3 2 1
```

```sh
cmake --build --preset release --target cache_research --parallel
```

**Windows, PowerShell:**

```powershell
& "$buildDir/bin/Release/cache_research.exe" configs/config_1.txt inputs/input_1.txt
```

**Linux, Bash:**

```bash
"$buildDir/bin/Release/cache_research" configs/config_1.txt inputs/input_1.txt
```

Количество вместимостей должно совпадать с количеством уровней, а число
идентификаторов — с заявленным количеством запросов.
