# RVL-003: достать blob и не выдать повреждённый файл

## Зачем эта задача

`blob put` уже кладёт содержимое по его SHA-256. Теперь Rovelle должна уметь
достать объект по хешу и проверить, что содержимое хранилища не изменилось.

После этой задачи должна работать команда:

```sh
./build/rovelle blob get <store> <hash> <destination>
```

Успешная команда создаёт `destination` с исходным содержимым. Если объекта нет
или его содержимое не соответствует хешу, команда завершается ошибкой и не
публикует повреждённый файл.

## Граница задачи

В задачу входят:

- поиск объекта по SHA-256;
- проверка формата входного хеша;
- копирование во временный файл рядом с `destination`;
- проверка SHA-256 временной копии;
- атомарный `rename()` только после успешной проверки;
- отдельная ошибка для повреждённого объекта;
- команда CLI `blob get`.

В задачу не входят:

- сеть и HTTP;
- manifest релиза;
- параллельное скачивание;
- удаление старых blobs;
- Windows;
- автоматическое восстановление повреждённого объекта.

## Публичный контракт

В `include/rovelle/blob.h` добавить:

```c
rovelle_status_t rovelle_blob_get(
    const char *store_path,
    const char hash[ROVELLE_SHA256_HEX_SIZE],
    const char *destination_path
);
```

В `rovelle_status_t` добавить статус:

```c
ROVELLE_CORRUPT
```

Его смысл: объект найден, но вычисленный SHA-256 не совпал с хешем, по которому
объект запросили.

## Команда CLI

```text
rovelle blob get <store> <hash> <destination>
```

Пример:

```sh
./build/rovelle blob get \
    /tmp/rovelle-store \
    9be43cdff7b69dff1ac9f13d864a4a635fcf149b03b2cb89a1c179db7899267d \
    /tmp/restored.txt
```

## Правила для входного хеша

Первая версия принимает ровно 64 символа:

```text
0-9 a-f
```

Пустая строка, короткий хеш, длинный хеш, `/`, `.` и заглавные буквы дают
`ROVELLE_INVALID_ARGUMENT`. Так пользовательский ввод не может превратиться в
произвольный путь по файловой системе.

## Порядок работы

```text
validate arguments and hash
    ↓
format <store>/objects/<2 chars>/<62 chars>
    ↓
open stored object
    ↓
create <destination>.tmp-XXXXXX with mkstemp
    ↓
copy object to the temporary file
    ↓
fflush + fsync + fclose
    ↓
hash the temporary copy
    ↓
if hash differs: unlink temporary, return ROVELLE_CORRUPT
    ↓
rename temporary to destination
```

Временный файл создаётся рядом с `destination`, а не внутри store. Тогда
`rename()` выполняется в пределах одной файловой системы и остаётся атомарным.

Проверяется временная копия. Если объект в хранилище повреждён, существующий
`destination` остаётся нетронутым.

## Что можно переиспользовать

Из `src/blob.c`:

```c
copy_stream()
format_blob_path()
```

Из публичного API:

```c
rovelle_hash_file()
```

Не нужно заново писать SHA-256, преобразование digest в hex или цикл
копирования.

## Разрешённые функции

```text
strlen, strcmp
fopen, fwrite, fread, ferror, fflush, fclose
snprintf
mkstemp, fdopen, fileno, fsync
rename, unlink, close
rovelle_hash_file
```

Других скрытых ограничений нет.

## Обработка ошибок

- неверный аргумент или формат хеша: `ROVELLE_INVALID_ARGUMENT`;
- объекта нет: `ROVELLE_NOT_FOUND`;
- хеш временной копии не совпал: `ROVELLE_CORRUPT`;
- остальные ошибки файловой системы: `ROVELLE_IO_ERROR`;
- при любой ошибке удалить созданный временный файл;
- существующий `destination` менять только успешным `rename()`.

## Критерии готовности

### 1. Обычное чтение

```sh
printf 'hello rovelle\n' > /tmp/source.txt
HASH=$(./build/rovelle blob put /tmp/rovelle-store /tmp/source.txt)
./build/rovelle blob get /tmp/rovelle-store "$HASH" /tmp/restored.txt
cmp /tmp/source.txt /tmp/restored.txt
```

`cmp` не печатает ошибок.

### 2. Объект не найден

Хеш правильного формата, которого нет в store, возвращает
`ROVELLE_NOT_FOUND`. Файл назначения не создаётся.

### 3. Неверный хеш

Короткий хеш и строка с `/` возвращают `ROVELLE_INVALID_ARGUMENT`.

### 4. Повреждённый объект

После ручного изменения blob команда возвращает `ROVELLE_CORRUPT`.
Повреждённое содержимое не появляется в `destination`.

### 5. Существующий destination

Если `destination` уже содержит рабочую версию, а blob повреждён, рабочая
версия остаётся без изменений.

### 6. Чистый cleanup

После успеха и после каждой ошибки рядом с `destination` нет файлов
`*.tmp-*`.

## Архитектурный вывод

Content-addressed storage доверяет содержимому только после повторной проверки.
Получатель сначала подготавливает новую версию отдельно и лишь затем атомарно
делает её видимой. Этот же принцип позже будет использован при доставке релиза
на работающий экземпляр.
