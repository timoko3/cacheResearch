# Cache parser grammar

Парсер использует два входных файла:

- `config` — описание уровней кэша и стратегий вытеснения;
- `input` — размеры уровней и последовательность запросов.

## Лексемы

```text
INT        ::= DIGIT+
IDENTIFIER ::= (LETTER | DIGIT)+
DIGIT      ::= '0' ... '9'
LETTER     ::= 'A' ... 'Z' | 'a' ... 'z'
```

Последовательность только из цифр распознаётся как `INT`.
Если при попытке считать `INT` после цифр встречается непробельный символ,
лексер откатывается и считывает всю последовательность как `IDENTIFIER`.

Примеры:

```text
123   -> INT
LFU   -> IDENTIFIER
2Q    -> IDENTIFIER
123Q  -> IDENTIFIER
```

Пробельные символы между лексемами игнорируются.

## Config grammar

```text
CONFIG ::= NUM_LEVELS STRATEGY{NUM_LEVELS} EOF

NUM_LEVELS ::= INT

STRATEGY ::= "LFU"
           | "LRU"
           | "LIRS"
           | "2Q"
           | "ARC"
           | "REF"
```

`NUM_LEVELS` задаёт количество уровней кэша и не должно превышать
`MAX_CACHE_LEVELS`.

Пример:

```text
3
LRU
2Q
LFU
```

## Input grammar

```text
INPUT ::= LEVEL_SIZE{NUM_LEVELS}
          NUM_REQUESTS
          REQUEST{NUM_REQUESTS}
          EOF

LEVEL_SIZE   ::= INT
NUM_REQUESTS ::= INT
REQUEST      ::= INT
```

Количество `LEVEL_SIZE` должно совпадать с `NUM_LEVELS` из config-файла.
После `NUM_REQUESTS` должно находиться ровно указанное количество запросов.

Пример для `NUM_LEVELS = 3`:

```text
64
128
256
5
10 20 30 20 10
```

## Ошибки

Парсер считает вход некорректным, если:

- число стратегий не совпадает с `NUM_LEVELS`;
- указана неизвестная стратегия;
- число размеров уровней не совпадает с `NUM_LEVELS`;
- число запросов не совпадает с `NUM_REQUESTS`;
- вместо ожидаемого `INT` или `IDENTIFIER` встречается другая лексема;
- после корректно разобранных данных остаются лишние лексемы.