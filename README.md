# Rovelle

> Content-addressed artifact storage and zero-restart delivery platform — an
> early systems-engineering prototype.

Rovelle исследует полный путь программного артефакта: от исходников и сборки до
неизменяемого хранения, доставки, атомарной активации и rollback без
перезапуска узла. Текущий вертикальный срез — локальная библиотека на C17 и CLI
для SHA-256, content-addressed blobs и публикации manifest.

Проект развивается открыто как инженерный case study: архитектурные решения,
учебные задачи и воспроизводимые демонстрации хранятся рядом с кодом.

## Что уже работает

- потоковый SHA-256 через OpenSSL EVP;
- `blob put`: атомарная запись и дедупликация по содержимому;
- `blob get`: восстановление с проверкой hash;
- `manifest publish`: публикация manifest как неизменяемого CAS-объекта;
- единая модель ошибок C API;
- строгая сборка C17 с warnings-as-errors.

Объекты раскладываются по первым двум символам hash:

```text
<store>/objects/ab/cdef...
```

## Быстрый старт

Нужны CMake 3.21+, компилятор C17 и OpenSSL с development headers.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

./build/rovelle --help
./build/rovelle version
./build/rovelle hash README.md
```

Корневая справка доступна через `rovelle --help` и `rovelle help`. Для каждой
команды из блока `Usage:` поддерживается форма `<command> --help`, включая
вложенные команды, например `rovelle blob put --help`. Smoke-тест CTest
проверяет весь список из `Usage:` в изолированном временном каталоге.

Пример локального CAS:

```sh
STORE=/tmp/rovelle-store
HASH=$(./build/rovelle blob put "$STORE" README.md)
./build/rovelle blob get "$STORE" "$HASH" /tmp/rovelle-readme.md
cmp README.md /tmp/rovelle-readme.md
```

Для записи демонстраций есть обёртка, скрывающая локальный checkout path и имя
пользователя в выводе CMake:

```sh
./tools/rbuild
```

Она меняет только отображаемый вывод и сохраняет код завершения CMake.

## Manifest v1

Manifest начинается с заголовка:

```text
ROVELLE-MANIFEST-V1
```

Текущая реализация проверяет заголовок и непустое тело, после чего публикует
файл в CAS и возвращает hash ревизии. Строгая проверка записей вида
`<sha256><TAB><relative-path>`, канонизация и mutable refs — ближайшие этапы;
ограничения намеренно описаны явно, чтобы прототип не выглядел завершённым
production-хранилищем.

## Архитектурные инварианты

1. Blob и опубликованная ревизия неизменяемы.
2. Физический адрес содержимого определяется SHA-256.
3. Логический путь отделён от содержимого.
4. Новый релиз сначала подготавливается, затем активируется.
5. Rollback использует уже опубликованную ревизию и не запускает сборку.

Целевой end-to-end путь:

```text
source -> build -> CAS -> release -> prepare -> activate -> health -> rollback
```

## Репозиторий

- `include/rovelle/` — публичный C API;
- `src/` — реализация библиотеки;
- `cli/` — командная утилита;
- `tools/` — воспроизводимые build helpers;
- `docs/ARCHITECTURE.md` — целевая архитектура;
- `docs/DECISIONS.md` — реестр принятых решений;
- `docs/ROADMAP.md` — последовательность вертикальных срезов;
- `docs/STREAM-*.md` — учебные материалы, runbooks и ретроспективы.

## Статус

Rovelle находится на ранней стадии. Локальный CAS уже можно собрать и
проверить, но сетевой control plane, agents, prepare/activate и production
security ещё не реализованы. План и границы продукта зафиксированы в
[`docs/ROADMAP.md`](docs/ROADMAP.md), а архитектурная модель — в
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

Функции C API возвращают `rovelle_status_t` из
`include/rovelle/status.h`. `ROVELLE_OK` означает успех, а
`rovelle_status_name()` предоставляет стабильное диагностическое имя.

## Лицензия

Rovelle распространяется под [Apache License 2.0](LICENSE).
