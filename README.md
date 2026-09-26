# Cache Research

**[Результаты исследования и отчёты](cacheBenchmarks/reports/README.md)** — описание экспериментов, основные выводы и ссылки на подробные результаты.

## Настройка, сборка и запуск

Все команды выполняются из корня проекта.

```sh
cmake --preset debug
cmake --build --preset debug --parallel
```

#### Сборка тестов:
```
cmake --preset debug
cmake --build --preset debug
```
#### Запуск тестов:
```
ctest --test-dir build/Linux/debug --output-on-failure
```
Для запуска конкретной группы (к примеру, CacheREF):
```
./build/Linux/debug/cache_tests --gtest_filter='CacheREF*'
```

#### Сборка бенчмарков кешей:
```
cmake -S . -B build-benchmark -DCMAKE_BUILD_TYPE=Release
cmake --build build-benchmark --target cache_benchmark_runner -j
```
#### Запуск исходного бенчмарка (64 страницы):
```
python3 ./cacheBenchmarks/cache_benchmark.py --runner ./build-benchmark/cache_benchmark_runner
```

##### Доступные параметры:

- `--runner PATH` — путь к исполняемому файлу `cache_benchmark_runner`;
- `--root PATH` — директория для сгенерированных конфигураций и входных данных. По умолчанию `./cacheBenchmarks`;
- `--requests N` — количество запросов в одном benchmark-запуске. По умолчанию `20000`;
- `--seeds N [N ...]` — набор seed'ов, для которых выполняются тесты. По умолчанию `1 2 3`;
- `--output PATH` — путь к итоговому CSV-файлу. По умолчанию `./cacheBenchmarks/cache_benchmark_results.csv`.

## Формат входных файлов

Программа принимает два файла: конфигурацию кеш-системы и входные данные теста.

### Файл конфигурации

Первое значение — количество уровней кеша. После него должны идти стратегии вытеснения для каждого уровня в порядке от `L1` к последнему уровню.

Пример `configs/config_1.txt`:

```text
4
LFU
LRU
LIRS
2Q
```

Для этого примера создаётся кеш-система из 4 уровней:

```text
L1 -> LFU
L2 -> LRU
L3 -> LIRS
L4 -> 2Q
```

Разделителями между значениями могут быть пробелы, табы и переводы строк.

### Файл входных данных

Сначала указываются размеры всех уровней кеша. Количество размеров должно совпадать с количеством уровней из файла конфигурации.

После размеров указывается количество запросов, а затем — сами запросы.

Пример `inputs/input_1.txt`:

```text
3 4 5 7
5
1 2 3 2 1
```

Для конфигурации из 4 уровней это означает:

```text
Размеры уровней: 3 4 5 7
Количество запросов: 5
Запросы: 1 2 3 2 1
```

Количество запросов в файле должно совпадать с указанным значением.

## Запуск

Все команды выполняются из корня проекта.

Сначала настроить и собрать проект:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
```

Затем запустить программу, передав первым аргументом файл конфигурации, а вторым — файл входных данных:

```sh
./build/Linux/debug/bin/Debug/cache_research configs/config_1.txt inputs/input_1.txt
```

Общий вид запуска:

```sh
./build/Linux/debug/bin/Debug/cache_research <config_file> <input_file>
```
## Исследования кешей

Описание экспериментов, методика и результаты собраны [тут](cacheBenchmarks/reports/README.md). Ниже приведены команды для запуска исследований.

Пример сборки в Windows:

```sh
cmake -S . -B build/Windows/research -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/Windows/research --target cache_benchmark_runner cache_exact_search
```

Запуск новых опытов отдельно:

```sh
python cacheBenchmarks/cache_research.py --mode software --runner build/Windows/research/cache_benchmark_runner.exe
python cacheBenchmarks/cache_research.py --mode hierarchy --runner build/Windows/research/cache_benchmark_runner.exe
```

В Linux замените путь runner на путь к Linux-сборке без .exe.

| Параметр нового скрипта | Назначение |
|---|---|
| --mode software/hierarchy/all | Какой опыт выполнить |
| --root PATH | Каталог результатов; по умолчанию cacheBenchmarks/reports |
| --requests N | Длина искусственной последовательности; по умолчанию 20000 |
| --seeds N ... | Начальные значения генератора; по умолчанию 1 2 3 4 5 |
| --sizes N ... | Размеры одного программного кеша |
| --trace PATH | Свои запросы в режиме software, по одному идентификатору на слово |
| --total N | Сумма размеров трёх уровней; по умолчанию 290 |
| --level-limits N N N | Верхние границы L1, L2, L3; по умолчанию 4 64 256 |
| --l1-sizes N ... | Проверяемые размеры L1; по умолчанию 1 2 4 |
| --l2-sizes N ... | Проверяемые размеры L2; по умолчанию 16 32 48 64 |
| --workload-scale N | Масштаб наборов объектов в искусственных запросах; по умолчанию 290 |
| --generate-only | Сохранить plan.json без запуска экспериментов |

Одинаковые запросы используются для всех размеров и алгоритмов.
Результаты программного кеша и иерархии находятся в разных подкаталогах.

Исходный `cache_benchmark.py` остаётся отдельным опытом на 64 страницы.
Он сохраняет параметры `--output`, `--root` и `--generate-only`.
В сравнении с REF теперь выводится процент от его стоимости:
100% означает равенство, а 150% — стоимость в полтора раза больше.

### Проверки новых модулей

```sh
python -m unittest discover -s cacheBenchmarks -p "test_*.py" -v
```

Для интеграционных проверок задайте переменную окружения CACHE_RESEARCH_RUNNER
с путём к исполняемому runner. Без неё эта проверка будет явно пропущена.
