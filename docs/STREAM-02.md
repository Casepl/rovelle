# RVL-002: атомарно положить файл в blob-хранилище

## Зачем эта задача

На прошлом стриме файл получил идентификатор: SHA-256 в виде 64 hex-символов.
Теперь нужно сохранить содержимое так, чтобы:

- путь к файлу зависел только от его содержимого;
- два одинаковых файла занимали одно место;
- читатель никогда не увидел наполовину записанный blob;
- итоговый blob действительно соответствовал хешу в имени.

После этой задачи должна работать команда:

```sh
./build/rovelle blob put .rovelle-store README.md
```

Она печатает SHA-256, а файл появляется по адресу:

```text
.rovelle-store/objects/<первые 2 символа хеша>/<остальные 62 символа>
```

Например:

```text
.rovelle-store/objects/33/3b46bd12d0e...
```

## Что именно нужно написать

Это задача только на `blob put`.

В неё **не входят**:

- `blob get`;
- сеть;
- база данных;
- потоки;
- удаление старых blobs;
- Windows;
- защита от криптографической коллизии SHA-256.

`blob get` и проверка повреждённого хранилища будут отдельной задачей.

## Файлы

Создать:

```text
include/rovelle/blob.h
src/blob.c
```

Изменить:

```text
include/rovelle/hash.h
cli/main.c
CMakeLists.txt
```

Не менять алгоритм SHA-256 в `src/hash.c`, кроме замены числа `65` на именованную
константу.

## Публичный контракт

### 1. Именованные размеры SHA-256

В `include/rovelle/hash.h` добавить:

```c
#define ROVELLE_SHA256_HEX_LENGTH 64
#define ROVELLE_SHA256_HEX_SIZE (ROVELLE_SHA256_HEX_LENGTH + 1)
```

`64` — число видимых hex-символов. Дополнительный байт нужен для `\0`, которым
заканчивается строка C.

Сигнатуру существующей функции записать так:

```c
rovelle_status_t rovelle_hash_file(
    const char *path,
    char out_hex[ROVELLE_SHA256_HEX_SIZE]
);
```

### 2. Новый API blob

В `include/rovelle/blob.h` объявить:

```c
#ifndef ROVELLE_BLOB_H
#define ROVELLE_BLOB_H

#include "rovelle/hash.h"

rovelle_status_t rovelle_blob_put(
    const char *store_path,
    const char *source_path,
    char out_hash[ROVELLE_SHA256_HEX_SIZE]
);

#endif
```

Контракт функции:

- `store_path` — корень локального хранилища;
- `source_path` — файл, который нужно сохранить;
- `out_hash` — место, куда функция запишет 64 символа хеша и `\0`;
- функция возвращает `ROVELLE_OK` или существующий `rovelle_status_t`;
- при успехе итоговый blob полностью записан и доступен по пути из его хеша.

## Почему мы передаём массив в функцию

В C нельзя безопасно вернуть массив, созданный внутри функции:

```c
char hash[65];
return hash; /* ошибка: локальный массив исчезнет после выхода из функции */
```

Поэтому память создаёт вызывающий код:

```c
char hash[ROVELLE_SHA256_HEX_SIZE];
rovelle_blob_put(store, file, hash);
```

А функция получает адрес этой памяти и заполняет её. В параметрах функции запись
`char out_hash[65]` фактически превращается в `char *out_hash`. Число `65`
документирует договор, но компилятор не проверяет, что вызывающий код действительно
передал 65 байт.

Владение памятью остаётся у вызывающего кода. `rovelle_blob_put()` не должна
освобождать `out_hash` и не должна сохранять этот указатель после возврата.

## Какие массивы будут в этой задаче

### `out_hash`

```c
char out_hash[ROVELLE_SHA256_HEX_SIZE]
```

Выход функции. Его создаёт вызывающий код, функция заполняет.

### Буфер копирования

```c
unsigned char buffer[64 * 1024];
```

Временная память для чтения файла блоками. Мы не загружаем весь файл в память.
Этот массив живёт внутри функции копирования и нигде не сохраняется.

### Буферы путей

Для первой POSIX-версии разрешено использовать:

```c
#define ROVELLE_PATH_SIZE 4096

char objects_path[ROVELLE_PATH_SIZE];
char tmp_path[ROVELLE_PATH_SIZE];
char shard_path[ROVELLE_PATH_SIZE];
char final_path[ROVELLE_PATH_SIZE];
```

Каждый вызов `snprintf()` обязательно проверять: результат не должен быть
отрицательным или больше либо равен размеру выходного массива.

## Разрешённые функции

В этой задаче можно пользоваться готовыми функциями libc и POSIX:

```text
fopen, fread, fwrite, ferror, fflush, fclose
snprintf
mkdir, stat
mkstemp, fdopen, fileno, fsync
rename, unlink, close
```

Также нужно использовать уже написанную функцию:

```text
rovelle_hash_file
```

Не нужно вручную реализовывать SHA-256, преобразование digest в hex, копирование
строк или системные вызовы записи.

Единственный низкоуровневый алгоритм, который нужно написать самостоятельно в
этой задаче, — цикл `fread()` → `fwrite()` в `copy_stream()`. Не использовать
`copyfile()`, `sendfile()` или запуск `cp` через shell.

В прошлой задаче условие о ручном переводе digest в hex следовало написать
заранее. В этом ticket полный список ограничений находится в этом разделе:
других скрытых запретов на библиотечные функции нет.

## Заголовки для `src/blob.c`

Начать файл можно с такого набора:

```c
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rovelle/blob.h"
```

Для `snprintf()` и строковых литералов отдельный `string.h` не нужен. Если в
реализации появится `strlen()`, добавить `#include <string.h>` в этот момент.

## Каркас, с которого начать код

Сначала вставить структуру файла, собрать проект и только потом заполнять
функции сверху вниз:

```c
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rovelle/blob.h"

#define ROVELLE_PATH_SIZE 4096

static rovelle_status_t ensure_directory(const char *path)
{
    /* TODO 1: mkdir, EEXIST и проверка через stat */
    return ROVELLE_IO_ERROR;
}

static rovelle_status_t copy_stream(FILE *source, FILE *destination)
{
    /* TODO 2: buffer[64 * 1024], fread, fwrite и ferror */
    return ROVELLE_IO_ERROR;
}

static rovelle_status_t format_blob_path(
    const char *store_path,
    const char hash[ROVELLE_SHA256_HEX_SIZE],
    char *out_path,
    size_t out_path_size
)
{
    /* TODO 3: snprintf и проверка его результата */
    return ROVELLE_IO_ERROR;
}

rovelle_status_t rovelle_blob_put(
    const char *store_path,
    const char *source_path,
    char out_hash[ROVELLE_SHA256_HEX_SIZE]
)
{
    /* TODO 4: orchestration и единый cleanup */
    return ROVELLE_IO_ERROR;
}
```

Возвраты `ROVELLE_IO_ERROR` здесь временные, чтобы каркас компилировался. Их
нужно заменить реальной логикой. Параметры, которые ещё не используются, строгий
компилятор пометит как ошибку. Поэтому каркас удобно добавлять по одной функции,
а не пытаться оставить все четыре пустыми на несколько сборок.

## Обязательная декомпозиция

В `src/blob.c` сделать одну публичную функцию и небольшие `static` helpers.
Названия можно оставить такими:

```c
static rovelle_status_t ensure_directory(const char *path);

static rovelle_status_t copy_stream(FILE *source, FILE *destination);

static rovelle_status_t format_blob_path(
    const char *store_path,
    const char hash[ROVELLE_SHA256_HEX_SIZE],
    char *out_path,
    size_t out_path_size
);

rovelle_status_t rovelle_blob_put(
    const char *store_path,
    const char *source_path,
    char out_hash[ROVELLE_SHA256_HEX_SIZE]
);
```

### `ensure_directory`

Должна:

1. вызвать `mkdir(path, 0755)`;
2. вернуть `ROVELLE_OK`, если каталог создан;
3. при `EEXIST` через `stat()` проверить, что это действительно каталог;
4. в остальных случаях вернуть `ROVELLE_IO_ERROR`.

Она создаёт только один уровень. Вызывать её отдельно для `store`, `objects`,
`tmp` и каталога с первыми двумя символами хеша.

### `copy_stream`

Должна:

1. читать `source` в `unsigned char buffer[64 * 1024]`;
2. для каждого прочитанного блока вызвать `fwrite()`;
3. проверить, что записано ровно столько байт, сколько прочитано;
4. после цикла проверить `ferror(source)`;
5. вернуть `ROVELLE_OK` или `ROVELLE_IO_ERROR`.

Она не открывает и не закрывает файлы. Владение `FILE *` остаётся у
`rovelle_blob_put()`.

### `format_blob_path`

Должна собрать строку:

```text
<store>/objects/<hash[0]><hash[1]>/<hash начиная с символа 2>
```

Использовать `snprintf()`. Проверить, что путь поместился в `out_path`.

Параметр `out_path_size` нужен потому, что размер буфера пути не является частью
типа `char *`. Без него функция не знает, сколько байт ей разрешено записать.

## Алгоритм `rovelle_blob_put`

Выполнять шаги именно в этом порядке.

### 1. Проверить аргументы

Если `store_path`, `source_path` или `out_hash` равны `NULL`, вернуть
`ROVELLE_INVALID_ARGUMENT`.

### 2. Создать служебные каталоги

Создать по одному уровню:

```text
<store>
<store>/objects
<store>/tmp
```

### 3. Создать уникальный временный файл

Собрать шаблон:

```text
<store>/tmp/blob-XXXXXX
```

Передать изменяемый массив с этим шаблоном в `mkstemp()`. Функция заменит
`XXXXXX` уникальными символами и вернёт файловый дескриптор.

Преобразовать дескриптор в `FILE *` через:

```c
FILE *temporary = fdopen(temporary_fd, "wb");
```

Если `fdopen()` завершился ошибкой, закрыть дескриптор через `close()` и удалить
временный файл через `unlink()`.

### 4. Скопировать исходный файл во временный

Открыть `source_path` как `"rb"`, затем вызвать `copy_stream()`.

После копирования:

1. вызвать `fflush(temporary)`;
2. вызвать `fsync(fileno(temporary))`;
3. закрыть оба файла;
4. при любой ошибке удалить временный файл.

### 5. Посчитать хеш временного файла

Вызвать:

```c
rovelle_hash_file(tmp_path, out_hash);
```

Мы хешируем временную копию, а не исходный файл. Исходный файл мог измениться
между вычислением хеша и копированием. Такой порядок гарантирует, что имя blob
соответствует байтам, которые будут опубликованы.

### 6. Создать shard-каталог и итоговый путь

Создать каталог:

```text
<store>/objects/<первые 2 символа хеша>
```

После этого собрать `final_path` через `format_blob_path()`.

### 7. Обработать повторный `put`

Через `stat(final_path, ...)` проверить, существует ли blob.

Если существует обычный файл:

1. удалить временный файл;
2. вернуть `ROVELLE_OK`;
3. не создавать вторую копию.

Если по этому пути находится не обычный файл, вернуть `ROVELLE_CONFLICT`.

### 8. Атомарно опубликовать blob

Вызвать:

```c
rename(tmp_path, final_path)
```

Временный файл лежит внутри того же `store`, поэтому переименование происходит
в пределах одной файловой системы. Читатель увидит либо отсутствие `final_path`,
либо целый файл. Промежуточное содержимое по итоговому пути не появляется.

При ошибке удалить `tmp_path` и вернуть `ROVELLE_IO_ERROR`.

## Правило cleanup

В функции много ресурсов:

- исходный `FILE *`;
- временный `FILE *` или файловый дескриптор;
- временный файл на диске.

Использовать один блок `cleanup:`. Перед каждым выходом установить `status`, а в
cleanup закрыть только открытые ресурсы и удалить временный файл, если он ещё не
был переименован.

Не вызывать одновременно `fclose(temporary)` и `close(temporary_fd)` для одного
открытого ресурса. После успешного `fdopen()` дескриптор принадлежит `FILE *`, и
его закрывает `fclose()`.

## CLI

В `cli/main.c` добавить разбор только этой формы:

```text
rovelle blob put <store> <file>
```

Условие содержит ровно 5 аргументов:

```text
argv[0] = rovelle
argv[1] = blob
argv[2] = put
argv[3] = store
argv[4] = file
```

При успехе напечатать только хеш и перевод строки. При ошибке напечатать в
`stderr`:

```text
blob put failed: <status>
```

Коды завершения:

- `0` — успех;
- `1` — операция не выполнена;
- `2` — неизвестная команда или неверное число аргументов.

Обновить `--help`, добавив пример синтаксиса команды.

## CMake

Добавить `src/blob.c` в `rovelle_core`.

Для POSIX API добавить приватное определение компилятора:

```cmake
target_compile_definitions(rovelle_core PRIVATE
    ROVELLE_VERSION="${PROJECT_VERSION}"
    _POSIX_C_SOURCE=200809L
)
```

Новых библиотек для этой задачи не нужно.

## Критерии готовности

### 1. Первый `put`

```sh
rm -rf /tmp/rovelle-store
./build/rovelle blob put /tmp/rovelle-store README.md
find /tmp/rovelle-store -type f
```

Ожидание: команда печатает 64-символьный хеш, а в `objects` существует один
итоговый файл. В `tmp` ничего не осталось.

### 2. Повторный `put`

```sh
./build/rovelle blob put /tmp/rovelle-store README.md
find /tmp/rovelle-store/objects -type f | wc -l
```

Ожидание: печатается тот же хеш, количество объектов остаётся равным `1`.

### 3. Другое имя, то же содержимое

```sh
cp README.md /tmp/same-content.txt
./build/rovelle blob put /tmp/rovelle-store /tmp/same-content.txt
find /tmp/rovelle-store/objects -type f | wc -l
```

Ожидание: тот же хеш и один объект.

### 4. Другое содержимое

```sh
printf 'another file\n' > /tmp/another.txt
./build/rovelle blob put /tmp/rovelle-store /tmp/another.txt
find /tmp/rovelle-store/objects -type f | wc -l
```

Ожидание: другой хеш и два объекта.

### 5. Отсутствующий исходный файл

```sh
./build/rovelle blob put /tmp/rovelle-store /tmp/missing
```

Ожидание: ненулевой код завершения и отсутствие мусора в `<store>/tmp`.

## Что не считается готовым

- запись сразу в `final_path`;
- вычисление хеша исходника, затем повторное чтение исходника для копирования;
- чтение всего файла одним большим `malloc()`;
- игнорирование результата `snprintf`, `fwrite`, `fflush`, `fsync`, `fclose` или
  `rename`;
- оставшийся временный файл после обработанной ошибки;
- создание второго итогового файла для того же содержимого.

## Маленькие коммиты

Если хочется фиксировать продвижение по шагам:

1. `add blob put API and CLI parsing`
2. `create blob store directories and temp file`
3. `copy source into temporary blob`
4. `publish blob by sha256 path`
5. `handle duplicate blobs and cleanup`

## Что сказать в начале стрима

> На прошлом стриме файл получил SHA-256. Сегодня я превращу этот хеш в адрес.
> Команда скопирует файл во временную область, посчитает хеш именно сохранённых
> байтов и одним rename опубликует готовый blob. Повторная загрузка того же
> содержимого не создаст новую копию.

## Граница следующей задачи

Следующая задача добавит `blob get`: найдёт объект по хешу, повторно вычислит его
SHA-256, обнаружит повреждение и только после проверки скопирует файл получателю.
