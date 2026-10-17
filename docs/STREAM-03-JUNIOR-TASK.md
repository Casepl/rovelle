# RVL-003: реализовать `blob get`

## Контекст для разработчика

В Rovelle уже есть команда:

```sh
./build/rovelle blob put <store> <file>
```

Она копирует файл в content-addressed storage и печатает SHA-256. Объект лежит
по пути:

```text
<store>/objects/<первые 2 символа хеша>/<остальные 62 символа>
```

Например, хеш:

```text
9be43cdff7b69dff1ac9f13d864a4a635fcf149b03b2cb89a1c179db7899267d
```

соответствует пути:

```text
<store>/objects/9b/e43cdff7b69dff1ac9f13d864a4a635fcf149b03b2cb89a1c179db7899267d
```

Нужно реализовать обратную операцию: найти объект по хешу, проверить его и
атомарно записать по указанному пути.

## Что должно получиться

Новая команда:

```sh
./build/rovelle blob get <store> <hash> <destination>
```

Пример полного сценария:

```sh
printf 'hello rovelle\n' > /tmp/source.txt

HASH=$(./build/rovelle blob put \
    /tmp/rovelle-store \
    /tmp/source.txt)

./build/rovelle blob get \
    /tmp/rovelle-store \
    "$HASH" \
    /tmp/restored.txt

cmp /tmp/source.txt /tmp/restored.txt
```

Последняя команда не должна печатать ошибок.

## Файлы, которые нужно изменить

```text
include/rovelle/status.h
include/rovelle/blob.h
src/status.c
src/blob.c
cli/main.c
```

`CMakeLists.txt` менять не нужно. `src/blob.c` уже входит в библиотеку.

## Что не нужно делать

- Не писать SHA-256 самостоятельно.
- Не писать новый цикл `fread()` → `fwrite()`.
- Не использовать сеть.
- Не создавать manifest.
- Не создавать отсутствующие каталоги для `destination`.
- Не поддерживать Windows.
- Не писать всю задачу одной большой функцией сразу.

Используй уже существующие функции:

```c
copy_stream()
format_blob_path()
rovelle_hash_file()
```

`copy_stream()` и `format_blob_path()` находятся в `src/blob.c`, поэтому их
можно вызывать из новой функции в этом же файле, несмотря на `static`.

---

# Этап 1. Добавить статус повреждённого объекта

## Что изменить

В `include/rovelle/status.h` добавить в enum:

```c
ROVELLE_CORRUPT
```

Поставить его перед `ROVELLE_INVALID_ARGUMENT` или после него. Важно, чтобы в
`src/status.c` появился соответствующий `case`:

```c
case ROVELLE_CORRUPT:
    return "ROVELLE_CORRUPT";
```

## Зачем

`ROVELLE_NOT_FOUND` означает, что объекта нет.

`ROVELLE_IO_ERROR` означает ошибку файловой системы.

`ROVELLE_CORRUPT` означает, что объект найден и прочитан, но его содержимое не
соответствует хешу в адресе.

## Проверка этапа

Собрать проект. На этом этапе новая команда ещё не нужна.

---

# Этап 2. Объявить публичную функцию

В `include/rovelle/blob.h` после `rovelle_blob_put()` добавить:

```c
rovelle_status_t rovelle_blob_get(
    const char *store_path,
    const char hash[ROVELLE_SHA256_HEX_SIZE],
    const char *destination_path
);
```

## Что означают аргументы

- `store_path`: корень хранилища, например `/tmp/rovelle-store`.
- `hash`: строка из 64 символов `0-9a-f` и завершающего `\0`.
- `destination_path`: куда положить проверенный файл.

Функция не владеет этими строками. Не нужно вызывать для них `free()` и нельзя
их изменять.

Запись `char hash[ROVELLE_SHA256_HEX_SIZE]` в параметрах функции фактически
является указателем. Размер здесь документирует контракт, но не проверяет длину
во время выполнения. Поэтому формат хеша нужно проверить отдельно.

## Проверка этапа

В `src/blob.c` временно добавить функцию, которая только проверяет `NULL` и
возвращает `ROVELLE_IO_ERROR`. Сигнатура в `.h` и `.c` должна совпадать
полностью, иначе строгие предупреждения остановят сборку.

---

# Этап 3. Написать проверку хеша

В `src/blob.c` добавить helper перед публичными функциями:

```c
static int is_valid_hash(
    const char hash[ROVELLE_SHA256_HEX_SIZE]
);
```

## Контракт helper

Возвращает:

```text
1  хеш корректный
0  хеш некорректный
```

Это не `rovelle_status_t`. Здесь нужен простой ответ «да или нет».

## Алгоритм

1. Если `hash == NULL`, вернуть `0`.
2. Проверить `strlen(hash) == ROVELLE_SHA256_HEX_LENGTH`.
3. Пройти циклом по всем 64 символам.
4. Каждый символ должен быть либо `0-9`, либо `a-f`.
5. Если встретился другой символ, вернуть `0`.
6. После цикла вернуть `1`.

Можно использовать `strlen()`. Для неё добавить:

```c
#include <string.h>
```

Не нужно самостоятельно искать `\0` и не нужно использовать `ctype.h`.

## Почему только маленькие буквы

`rovelle_hash_file()` уже создаёт lowercase hex. Один канонический формат
означает один строковый адрес объекта.

## Проверка этапа

В начале `rovelle_blob_get()`:

```text
любой NULL                 → ROVELLE_INVALID_ARGUMENT
неправильный формат hash  → ROVELLE_INVALID_ARGUMENT
```

После этой проверки функция пока может возвращать `ROVELLE_IO_ERROR`.

---

# Этап 4. Подготовить переменные `rovelle_blob_get()`

В начале функции понадобятся:

```c
char blob_path[ROVELLE_PATH_SIZE];
char temporary_path[ROVELLE_PATH_SIZE];
char actual_hash[ROVELLE_SHA256_HEX_SIZE];

FILE *source = NULL;
FILE *temporary = NULL;

int temporary_fd = -1;
int temporary_exists = 0;
int written;

rovelle_status_t status = ROVELLE_OK;
```

## Зачем каждая переменная

- `blob_path`: путь к объекту внутри store.
- `temporary_path`: изменяемый шаблон для `mkstemp()`.
- `actual_hash`: фактический SHA-256 скопированного файла.
- `source`: открытый объект из store.
- `temporary`: временный destination как `FILE *`.
- `temporary_fd`: дескриптор до передачи владения в `fdopen()`.
- `temporary_exists`: нужно ли удалить временный файл в cleanup.
- `written`: результат `snprintf()`.
- `status`: ошибка, с которой функция уйдёт в общий cleanup.

`temporary_fd` и `temporary` описывают один файл на разных этапах. После
успешного `fdopen()` нужно установить `temporary_fd = -1`, потому что теперь
дескриптором владеет `FILE *` и его закроет `fclose()`.

---

# Этап 5. Получить путь к объекту

Вызвать существующую функцию:

```c
status = format_blob_path(
    store_path,
    hash,
    blob_path,
    sizeof(blob_path)
);
```

Если результат не `ROVELLE_OK`, сразу вернуть этот статус. На этом этапе ещё не
создано ресурсов, которые требуется закрывать.

Ожидаемый путь:

```text
<store>/objects/<hash[0..1]>/<hash[2..63]>
```

Не собирай этот путь второй раз вручную.

---

# Этап 6. Открыть объект

Открыть `blob_path`:

```c
source = fopen(blob_path, "rb");
```

Если `fopen()` вернул `NULL`:

```text
errno == ENOENT  → ROVELLE_NOT_FOUND
остальные ошибки → ROVELLE_IO_ERROR
```

После установки `status` перейти в `cleanup`.

Почему режим `rb`: blob является набором байтов, а не текстовым документом.

---

# Этап 7. Создать временный файл назначения

Собрать изменяемый шаблон:

```text
<destination>.tmp-XXXXXX
```

Например:

```text
/tmp/restored.txt.tmp-XXXXXX
```

Использовать `snprintf()` и проверить его результат:

```text
written < 0                         → ошибка
written >= sizeof(temporary_path)  → путь не поместился
```

В обоих случаях вернуть `ROVELLE_IO_ERROR` через cleanup.

Затем:

```c
temporary_fd = mkstemp(temporary_path);
```

`mkstemp()` заменит `XXXXXX` случайными символами, создаст файл и изменит строку
`temporary_path`. Поэтому туда передаётся массив `char[]`, а не строковый
литерал.

Если `mkstemp()` вернул `-1`, вернуть `ROVELLE_IO_ERROR` через cleanup.

После успеха:

```c
temporary_exists = 1;
```

Преобразовать дескриптор в поток:

```c
temporary = fdopen(temporary_fd, "wb");
```

Если `fdopen()` успешен:

```c
temporary_fd = -1;
```

## Почему временный файл рядом с destination

`rename()` гарантирует атомарную замену только внутри одной файловой системы.
Если временный файл создать внутри store, а destination находится на другом
диске, `rename()` может завершиться с `EXDEV`.

---

# Этап 8. Скопировать объект

Использовать существующий helper:

```c
status = copy_stream(source, temporary);
```

Не писать второй цикл копирования.

После успешного копирования:

1. Закрыть `source` через `fclose()`.
2. Установить `source = NULL`.
3. Выполнить `fflush(temporary)`.
4. Получить дескриптор через `fileno(temporary)`.
5. Выполнить `fsync()`.
6. Закрыть `temporary` через `fclose()`.
7. Установить `temporary = NULL`.

Если любой шаг завершился ошибкой, установить `ROVELLE_IO_ERROR` и перейти в
`cleanup`.

## Зачем `fflush` и `fsync`

- `fflush()` передаёт данные из буфера `FILE *` операционной системе.
- `fsync()` просит операционную систему записать данные файла на носитель.
- `fclose()` закрывает поток.

Хешировать файл нужно после закрытия потока, чтобы `rovelle_hash_file()` увидела
полностью записанные данные.

---

# Этап 9. Проверить содержимое

Вычислить SHA-256 временной копии:

```c
status = rovelle_hash_file(temporary_path, actual_hash);
```

Если функция вернула ошибку, перейти в cleanup.

Сравнить строки:

```c
strcmp(actual_hash, hash)
```

Результат `0` означает, что строки равны.

Если строки не равны:

```c
status = ROVELLE_CORRUPT;
goto cleanup;
```

Не удаляй и не исправляй объект внутри store. Эта задача только обнаруживает
повреждение. Временная копия будет удалена в cleanup.

---

# Этап 10. Опубликовать destination

Если хеш совпал:

```c
rename(temporary_path, destination_path)
```

При успехе существующий `destination` атомарно заменяется проверенной версией.
Читатель видит либо старый полный файл, либо новый полный файл.

После успешного `rename()`:

```c
temporary_exists = 0;
status = ROVELLE_OK;
```

Если `rename()` завершился ошибкой, установить `ROVELLE_IO_ERROR` и перейти в
cleanup.

---

# Этап 11. Написать общий cleanup

Перед `return status`:

1. Если `source != NULL`, закрыть через `fclose()`.
2. Если `temporary != NULL`, закрыть через `fclose()`.
3. Иначе, если `temporary_fd != -1`, закрыть через `close()`.
4. Если `temporary_exists != 0`, удалить `temporary_path` через `unlink()`.

Если cleanup сам завершился ошибкой, менять `status` на `ROVELLE_IO_ERROR`
только когда до этого было `ROVELLE_OK`. Первичная ошибка полезнее ошибки
cleanup.

Можно повторить структуру cleanup из `rovelle_blob_put()`.

---

# Этап 12. Добавить команду CLI

В `print_help()` добавить:

```text
rovelle blob get <store> <hash> <destination>
```

В `main()` распознать:

```text
argc == 6
argv[1] == "blob"
argv[2] == "get"
argv[3] == store
argv[4] == hash
argv[5] == destination
```

Вызвать:

```c
rovelle_blob_get(argv[3], argv[4], argv[5])
```

При ошибке напечатать в `stderr`:

```text
blob get failed: ROVELLE_...
```

При успехе ничего печатать не требуется. Команда возвращает `0`.

---

# Проверка всей задачи

## Чистый сценарий

```sh
rm -rf /tmp/rovelle-store
rm -f /tmp/source.txt /tmp/restored.txt

printf 'hello rovelle\n' > /tmp/source.txt

HASH=$(./build/rovelle blob put \
    /tmp/rovelle-store \
    /tmp/source.txt)

./build/rovelle blob get \
    /tmp/rovelle-store \
    "$HASH" \
    /tmp/restored.txt

cmp /tmp/source.txt /tmp/restored.txt
```

Ожидается: `cmp` ничего не печатает и возвращает код `0`.

## Объект отсутствует

Передать 64 нуля как хеш.

Ожидается:

```text
blob get failed: ROVELLE_NOT_FOUND
```

## Хеш неправильного формата

Передать:

```text
abc
```

и отдельно:

```text
../../etc/passwd
```

Ожидается:

```text
blob get failed: ROVELLE_INVALID_ARGUMENT
```

## Повреждённый объект

1. Положить файл через `blob put`.
2. Найти созданный объект в `objects/<2>/<62>`.
3. Заменить его содержимое другой строкой.
4. Создать destination с текстом `working version`.
5. Запустить `blob get`.

Ожидается:

```text
blob get failed: ROVELLE_CORRUPT
```

Destination по-прежнему содержит:

```text
working version
```

Рядом с ним нет файлов `*.tmp-*`.

---

# Definition of Done

- Проект собирается со строгими предупреждениями.
- Обычный blob восстанавливается без изменений.
- Неверный хеш не используется как часть пути.
- Отсутствующий объект возвращает `ROVELLE_NOT_FOUND`.
- Повреждённый объект возвращает `ROVELLE_CORRUPT`.
- Повреждённый объект не заменяет существующий destination.
- После ошибок не остаются временные файлы.
- В help отображается команда `blob get`.

# Если застрял на стриме

Сделай сначала самый короткий вертикальный сценарий:

```text
validate hash
→ format path
→ fopen blob
→ copy to temporary
→ hash temporary
→ compare
→ rename
```

После того как он заработает, добавь точные статусы ошибок и полный cleanup.
Не начинай оптимизировать чтение и хеширование в один проход: это отдельная
задача после работающей корректной версии.
