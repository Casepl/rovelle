# Стрим 2: шпаргалка ведущего

## Результат эфира

```sh
./build/rovelle blob put /tmp/rovelle-store README.md
```

Файл появляется здесь:

```text
<store>/objects/<2 символа хеша>/<62 символа хеша>
```

Повторный `put` печатает тот же хеш и не создаёт второй объект.

## Сегодня пишем

```text
include/rovelle/blob.h
src/blob.c
cli/main.c
CMakeLists.txt
```

Только `blob put`. `blob get` сегодня не пишем.

## Сигнатура

```c
rovelle_status_t rovelle_blob_put(
    const char *store_path,
    const char *source_path,
    char out_hash[ROVELLE_SHA256_HEX_SIZE]
);
```

`out_hash` передаёт вызывающий код, потому что локальный массив функции нельзя
вернуть после выхода. Нужно 65 байт: 64 символа и `\0`.

## Порядок работы

```text
validate arguments
    ↓
create store/objects and store/tmp
    ↓
mkstemp(store/tmp/blob-XXXXXX)
    ↓
copy source to temp in 64 KiB blocks
    ↓
fflush + fsync + fclose
    ↓
hash the temp file
    ↓
create objects/<first 2 chars>
    ↓
if final object exists: unlink temp, success
    ↓
rename temp to final path
```

Хешируем временную копию. Иначе исходный файл может измениться между хешированием
и копированием, и адрес перестанет соответствовать содержимому.

## Helpers

```c
static rovelle_status_t ensure_directory(const char *path);
static rovelle_status_t copy_stream(FILE *source, FILE *destination);
static rovelle_status_t format_blob_path(
    const char *store_path,
    const char hash[ROVELLE_SHA256_HEX_SIZE],
    char *out_path,
    size_t out_path_size
);
```

## Разрешено использовать

```text
fopen fread fwrite ferror fflush fclose
snprintf
mkdir stat
mkstemp fdopen fileno fsync
rename unlink close
rovelle_hash_file
```

SHA-256 и hex заново не писать.

Вручную пишем только цикл `fread` → `fwrite`. Не используем `copyfile`,
`sendfile` или shell-команду `cp`. Других скрытых ограничений нет.

## Заголовки `blob.c`

```c
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rovelle/blob.h"
```

## Четыре массива

```c
char out_hash[65];                 /* результат для вызывающего кода */
unsigned char buffer[64 * 1024];  /* блок копирования */
char tmp_path[4096];              /* изменяет mkstemp */
char final_path[4096];            /* собирает snprintf */
```

У `snprintf()` всегда проверять переполнение.

## Cleanup

- До `fdopen`: дескриптор закрывает `close()`.
- После успешного `fdopen`: дескриптор закрывает `fclose()`.
- При ошибке удалить временный файл через `unlink()`.
- После успешного `rename()` временного пути уже нет.

## Проверка

```sh
rm -rf /tmp/rovelle-store

./tools/stream-cmake -S . -B build
./tools/stream-cmake --build build

./build/rovelle blob put /tmp/rovelle-store README.md
./build/rovelle blob put /tmp/rovelle-store README.md

cp README.md /tmp/same-content.txt
./build/rovelle blob put /tmp/rovelle-store /tmp/same-content.txt

find /tmp/rovelle-store/objects -type f
find /tmp/rovelle-store/tmp -type f
```

Должен быть один объект и ни одного временного файла.

## Если застрял

Сначала довести путь без дедупликации:

```text
source -> temp -> hash(temp) -> rename(final)
```

Потом добавить проверку существующего `final_path`.

## Финал стрима

1. Один и тот же контент получил один адрес?
2. Был ли итоговый путь когда-либо частично записан?
3. Остались ли файлы в `tmp` после ошибок?
4. Какой следующий шаг? `blob get` с проверкой хеша.
