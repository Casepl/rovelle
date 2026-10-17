# RVL-004: сохранить manifest как ревизию

## Размер задачи

Это задача на один стрим продолжительностью около 89 минут.

На стриме мы **не пишем полноценный парсер manifest**. К концу эфира нужен
один законченный переход:

```text
manifest-файл → blob put → SHA-256 ревизии
```

## Контекст

Rovelle уже умеет:

```text
файл → SHA-256 → blob put → objects/ab/cdef...
hash → blob get → проверенная копия файла
```

Но релиз состоит из нескольких файлов. Нужен документ, который связывает
логический путь файла с хешем его blob:

```text
ROVELLE-MANIFEST-V1
<hash><TAB>public/style.css
<hash><TAB>server/index.js
```

Если сохранить этот документ через существующий `rovelle_blob_put()`, хеш
manifest станет идентификатором всей ревизии.

## Результат

Должна появиться команда:

```sh
./build/rovelle manifest publish <store> <manifest-file>
```

Она должна:

1. Проверить аргументы.
2. Открыть manifest.
3. Проверить только заголовок `ROVELLE-MANIFEST-V1`.
4. Убедиться, что после заголовка есть хотя бы один байт.
5. Закрыть файл.
6. Сохранить manifest через существующий `rovelle_blob_put()`.
7. Напечатать SHA-256 manifest как revision ID.

## Что не входит в задачу

На этом стриме не нужно:

- разбирать строки `hash<TAB>path`;
- проверять формат каждого хеша;
- проверять безопасность логического пути;
- проверять существование указанных blobs;
- проверять сортировку и повторяющиеся пути;
- создавать manifest автоматически;
- делать `refs/current`;
- реализовывать сеть или доставку на поды.

Это следующие отдельные задачи. Сейчас важно доказать, что manifest может быть
неизменяемым объектом, адресованным собственным содержимым.

## Файлы

Создать:

```text
include/rovelle/manifest.h
src/manifest.c
```

Изменить:

```text
cli/main.c
CMakeLists.txt
```

## Публичный контракт

В `include/rovelle/manifest.h` объявить:

```c
#ifndef ROVELLE_MANIFEST_H
#define ROVELLE_MANIFEST_H

#include "rovelle/hash.h"
#include "rovelle/status.h"

rovelle_status_t rovelle_manifest_publish(
    const char *store_path,
    const char *manifest_path,
    char out_revision[ROVELLE_SHA256_HEX_SIZE]
);

#endif
```

Память для `out_revision` создаёт вызывающий код. Функция записывает туда
64 hex-символа и завершающий `\0` только при успехе.

## Реализация маленькими шагами

### Шаг 1. Создать заголовочный файл

Создать `include/rovelle/manifest.h` и объявить функцию.

Контрольная точка:

```sh
rbuild
```

### Шаг 2. Создать `manifest.c`

Подключить:

```c
#include "rovelle/manifest.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "rovelle/blob.h"
```

Определить ожидаемый заголовок:

```c
#define ROVELLE_MANIFEST_HEADER "ROVELLE-MANIFEST-V1\n"
```

### Шаг 3. Проверить аргументы

Если любой аргумент равен `NULL`, вернуть:

```c
ROVELLE_INVALID_ARGUMENT
```

### Шаг 4. Прочитать заголовок

Открыть файл:

```c
FILE *manifest = fopen(manifest_path, "rb");
```

Создать буфер, способный вместить заголовок вместе с `\0`:

```c
char header[sizeof(ROVELLE_MANIFEST_HEADER)];
```

Прочитать первую строку через `fgets()` и сравнить с константой через
`strcmp()`.

Если файл не открылся, вернуть `ROVELLE_NOT_FOUND` для `ENOENT` или
`ROVELLE_IO_ERROR` для другой ошибки. Если заголовок отличается, вернуть
`ROVELLE_INVALID_ARGUMENT`.

### Шаг 5. Проверить, что manifest не пустой

После заголовка прочитать один символ через `fgetc()`.

- `EOF` вместе с `ferror()` означает `ROVELLE_IO_ERROR`;
- обычный `EOF` означает пустой manifest и `ROVELLE_INVALID_ARGUMENT`;
- любой другой символ означает, что содержимое присутствует.

Разбирать этот символ и остальные строки сейчас не нужно.

### Шаг 6. Закрыть файл

Закрыть manifest через `fclose()`. Если закрытие завершилось ошибкой, вернуть
`ROVELLE_IO_ERROR`.

### Шаг 7. Сохранить manifest

Использовать уже работающую функцию:

```c
return rovelle_blob_put(
    store_path,
    manifest_path,
    out_revision
);
```

Не писать второй алгоритм копирования файла.

### Шаг 8. Добавить CLI

Аргументы команды:

```text
argv[0] rovelle
argv[1] manifest
argv[2] publish
argv[3] store
argv[4] manifest-file
```

Значит, ожидается `argc == 5`.

При успехе напечатать revision ID. При ошибке вывести:

```text
manifest publish failed: ROVELLE_INVALID_ARGUMENT
```

Добавить команду в `print_help()`.

### Шаг 9. Подключить файл в CMake

Добавить `src/manifest.c` к исходникам `rovelle_core`, затем выполнить:

```sh
rbuild
```

## Ручная демонстрация

### Подготовить два файла

```sh
rm -rf /tmp/rovelle-demo
mkdir -p /tmp/rovelle-demo/input

printf 'console.log("v1");\n' > /tmp/rovelle-demo/input/index.js
printf 'body { color: black; }\n' > /tmp/rovelle-demo/input/style.css
```

### Сохранить два blob

```sh
JS_V1=$(./build/rovelle blob put \
    /tmp/rovelle-demo/store \
    /tmp/rovelle-demo/input/index.js)

CSS=$(./build/rovelle blob put \
    /tmp/rovelle-demo/store \
    /tmp/rovelle-demo/input/style.css)
```

### Создать первую ревизию

```sh
printf 'ROVELLE-MANIFEST-V1\n%s\tpublic/style.css\n%s\tserver/index.js\n' \
    "$CSS" "$JS_V1" \
    > /tmp/rovelle-demo/release-v1.rvl

REVISION_V1=$(./build/rovelle manifest publish \
    /tmp/rovelle-demo/store \
    /tmp/rovelle-demo/release-v1.rvl)

printf 'revision v1: %s\n' "$REVISION_V1"
find /tmp/rovelle-demo/store/objects -type f | wc -l
```

Ожидается три объекта: JavaScript, CSS и manifest.

### Изменить один файл

```sh
printf 'console.log("v2");\n' > /tmp/rovelle-demo/input/index.js

JS_V2=$(./build/rovelle blob put \
    /tmp/rovelle-demo/store \
    /tmp/rovelle-demo/input/index.js)

printf 'ROVELLE-MANIFEST-V1\n%s\tpublic/style.css\n%s\tserver/index.js\n' \
    "$CSS" "$JS_V2" \
    > /tmp/rovelle-demo/release-v2.rvl

REVISION_V2=$(./build/rovelle manifest publish \
    /tmp/rovelle-demo/store \
    /tmp/rovelle-demo/release-v2.rvl)

printf 'revision v1: %s\nrevision v2: %s\n' \
    "$REVISION_V1" "$REVISION_V2"

find /tmp/rovelle-demo/store/objects -type f | wc -l
```

Теперь объектов пять. Добавились новый JavaScript и новый manifest. CSS
остался прежним объектом.

## Критерии готовности

Задача завершена, если:

- проект собирается;
- команда `manifest publish` появилась в `--help`;
- manifest с правильным заголовком получает revision ID;
- повторная публикация того же manifest возвращает тот же revision ID;
- manifest с неправильным заголовком отклоняется;
- файл без записей после заголовка отклоняется;
- изменение одного файла создаёт новый blob и новую ревизию;
- неизменившийся CSS не сохраняется повторно.

---

# Контент-план на 89 минут

## 00:00–00:05 — хук

> У меня уже есть файлы по хешам, но набор случайных хешей ещё не является
> релизом. За этот стрим я превращу manifest в неизменяемую ревизию. Потом
> изменю один файл из двух и проверю, что второй не придётся сохранять заново.

Сразу показать ожидаемую команду `manifest publish`.

## 00:05–00:12 — напомнить текущий результат

Показать рабочие `blob put` и `blob get`. Не разбирать их реализацию заново.

## 00:12–00:20 — нарисовать модель

```text
index.js  → hash A ┐
                   ├→ manifest → revision X
style.css → hash B ┘
```

После изменения:

```text
index.js  → hash C ┐
                   ├→ manifest → revision Y
style.css → hash B ┘
```

## 00:20–00:28 — определить границу задачи

Показать формат manifest и явно сказать:

> Сегодня я проверяю только версию формата и сохраняю manifest. Проверку всех
> ссылок и путей сделаю отдельным слоем на следующем шаге.

## 00:28–00:50 — библиотечная функция

Написать `manifest.h` и `manifest.c`: аргументы, `fopen()`, `fgets()`,
`strcmp()`, проверку непустого содержимого, `fclose()` и вызов
`rovelle_blob_put()`.

После законченного шага проговаривать:

```text
Сейчас проверяю ...
Ожидаю ...
Получилось ...
Следующий шаг ...
```

## 00:50–01:05 — CLI и CMake

Добавить `manifest publish`, обновить `--help`, подключить `manifest.c` и
выполнить `rbuild`.

К отметке **65 минут** команда должна принять manifest и напечатать revision
ID.

## 01:05–01:20 — главная демонстрация

1. Создать JavaScript и CSS.
2. Сохранить оба blob.
3. Опубликовать manifest v1.
4. Изменить только JavaScript.
5. Опубликовать manifest v2.
6. Показать разные revision ID.
7. Показать рост числа объектов с трёх до пяти.

Перед запуском сказать фразу для будущего Shorts:

> Я изменил один файл из двух. Если модель работает, в хранилище появятся
> только новый JavaScript и новый manifest. Старый CSS останется тем же blob.

## 01:20–01:29 — завершение

Проговорить:

1. Manifest теперь получает собственный адрес.
2. Этот адрес фиксирует конкретное содержимое релиза.
3. Неизменившийся файл переиспользуется между ревизиями.
4. Следующая задача — разобрать записи manifest и проверить все ссылки.

## Если застряли

Минимальный результат стрима:

```text
manifest publish
проверка заголовка
вызов blob put
revision ID в stdout
```

Проверку пустого manifest можно закончить после эфира. Не начинать парсер строк,
проверку путей или `refs/current`.
