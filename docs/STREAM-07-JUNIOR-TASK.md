# RVL-007: проверить blobs перед публикацией manifest

## Контекст

Rovelle уже умеет публиковать manifest такого вида:

```text
ROVELLE-MANIFEST-V1
<sha256><TAB>server/index.js
```

На шестом стриме появилась проверка каждой строки: hash должен состоять из
64 символов `0-9` или `a-f`, а путь не должен быть пустым.

Но сейчас manifest успешно публикуется даже в двух неправильных случаях:

1. объекта с указанным hash вообще нет в store;
2. объект есть, но его содержимое изменилось и больше не соответствует hash.

## Что нужно сделать

Добавить функцию:

```c
rovelle_status_t rovelle_blob_verify(
    const char *store_path,
    const char hash[ROVELLE_SHA256_HEX_SIZE]
);
```

Она должна найти blob в store, вычислить hash его фактического содержимого и
сравнить его с ожидаемым hash.

Затем вызвать эту функцию для каждой записи manifest до публикации самого
manifest.

После изменения команда:

```sh
./build/rovelle manifest publish <store> <manifest-file>
```

должна вести себя так:

| Содержимое store | Результат |
|---|---|
| Blob существует и соответствует hash | Manifest публикуется |
| Blob отсутствует | `ROVELLE_NOT_FOUND` |
| Blob существует, но повреждён | `ROVELLE_CORRUPT` |

Главный результат стрима:

```text
невалидная ссылка на blob -> manifest не сохраняется -> revision не появляется
```

## Граница задачи

Сегодня нужно:

1. добавить публичную `rovelle_blob_verify()`;
2. проверить в ней формат аргументов;
3. собрать путь объекта через существующую `format_blob_path()`;
4. вычислить фактический hash через `rovelle_hash_file()`;
5. вернуть `ROVELLE_CORRUPT`, если фактический hash не совпал с ожидаемым;
6. вызвать проверку для каждой записи в `rovelle_manifest_publish()`;
7. вручную показать исправный, отсутствующий и повреждённый blob;
8. проверить сборку и существующие тесты.

Сегодня не нужно:

- проверять безопасность логических путей;
- проверять сортировку или дубликаты записей manifest;
- менять `blob get`;
- добавлять `refs/current`;
- защищаться от параллельного изменения store;
- переписывать парсер manifest.

## Файлы

Изменить:

```text
include/rovelle/blob.h
src/blob.c
src/manifest.c
```

Посмотреть как примеры:

```text
include/rovelle/hash.h
src/hash.c
tests/cas_smoke.sh
```

## Шаг 1. Показать текущую ошибку

Запустить из корня проекта:

```sh
mkdir -p tmp/rovelle-stream07
DEMO_DIR=$(mktemp -d "$PWD/tmp/rovelle-stream07/demo.XXXXXX")

printf 'console.log("hello");\n' > "$DEMO_DIR/index.js"

BLOB_HASH=$(./build/rovelle blob put \
  "$DEMO_DIR/store" \
  "$DEMO_DIR/index.js")

printf 'ROVELLE-MANIFEST-V1\n%s\tserver/index.js\n' \
  "$BLOB_HASH" > "$DEMO_DIR/good.rvl"

./build/rovelle manifest publish \
  "$DEMO_DIR/store" \
  "$DEMO_DIR/good.rvl"
```

Первая публикация должна пройти: blob действительно находится в store.

Теперь создать manifest со строкой из 64 нулей. Это правильный по формату hash,
но объекта с таким адресом нет:

```sh
MISSING_HASH=$(printf '%064d' 0)

printf 'ROVELLE-MANIFEST-V1\n%s\tserver/missing.js\n' \
  "$MISSING_HASH" > "$DEMO_DIR/missing.rvl"

./build/rovelle manifest publish \
  "$DEMO_DIR/store" \
  "$DEMO_DIR/missing.rvl"
```

Текущее неправильное поведение: команда печатает revision и завершается
успешно.

Ожидаемое поведение после задачи:

```text
manifest publish failed: ROVELLE_NOT_FOUND
```

Контрольная точка: объяснить, почему `rovelle_hash_is_valid()` принимает
64 нуля, хотя соответствующего blob нет.

## Шаг 2. Добавить публичный контракт

В `include/rovelle/blob.h` объявить `rovelle_blob_verify()` рядом с `blob put`
и `blob get`.

Контракт функции:

- `NULL` или неправильный hash -> `ROVELLE_INVALID_ARGUMENT`;
- объекта нет -> `ROVELLE_NOT_FOUND`;
- объект не читается -> ошибка от `rovelle_hash_file()`;
- содержимое не совпадает с ожидаемым hash -> `ROVELLE_CORRUPT`;
- содержимое совпадает -> `ROVELLE_OK`.

## Шаг 3. Реализовать `rovelle_blob_verify()`

Реализацию добавить в `src/blob.c`. Использовать существующие функции:

```text
rovelle_hash_is_valid() -> проверить формат ожидаемого hash
format_blob_path()      -> получить путь objects/xx/...
rovelle_hash_file()     -> вычислить hash содержимого объекта
strcmp()                -> сравнить ожидаемый и фактический hash
```

Функция только читает и проверяет объект. Она не должна копировать файл,
создавать временный файл или вызывать `rename()`.

Порядок рассуждения:

```text
аргументы корректны?
    -> где лежит объект?
        -> удалось вычислить его hash?
            -> hash совпал?
```

Контрольная точка:

```sh
./tools/rbuild
./tools/rtest
```

## Шаг 4. Подключить проверку к manifest

Открыть цикл записей в `rovelle_manifest_publish()`.

Сейчас в нём уже получены:

```c
const char *hash = manifest_line;
const char *path = separator + 1;
```

После проверки формата `hash` и непустого `path`, но до увеличения
`entry_count`, вызвать `rovelle_blob_verify()`.

Если функция вернула не `ROVELLE_OK`:

1. сохранить этот статус;
2. перейти в существующий `cleanup`;
3. не вызывать финальный `rovelle_blob_put()` для manifest.

Ключевой инвариант:

```text
все строки проверены -> только потом manifest сохраняется как revision
```

Контрольная точка:

```sh
./tools/rbuild
```

## Шаг 5. Доказать результат

Повторить публикацию `missing.rvl` из шага 1. Теперь ожидается:

```text
manifest publish failed: ROVELLE_NOT_FOUND
```

Затем ещё раз опубликовать `good.rvl`. Он должен по-прежнему вернуть revision.

Для проверки повреждения изменить уже сохранённый blob, не меняя его адрес:

```sh
OBJECT_DIR=${BLOB_HASH%${BLOB_HASH#??}}
OBJECT_NAME=${BLOB_HASH#??}
OBJECT_PATH="$DEMO_DIR/store/objects/$OBJECT_DIR/$OBJECT_NAME"

printf 'damaged content\n' > "$OBJECT_PATH"

./build/rovelle manifest publish \
  "$DEMO_DIR/store" \
  "$DEMO_DIR/good.rvl"
```

Ожидается:

```text
manifest publish failed: ROVELLE_CORRUPT
```

Финальная проверка:

```sh
./tools/rbuild
./tools/rtest
```

## Критерии готовности

Задача готова, если одновременно выполнены пять условий:

1. Исправный blob позволяет опубликовать manifest.
2. Отсутствующий blob возвращает `ROVELLE_NOT_FOUND`.
3. Повреждённый blob возвращает `ROVELLE_CORRUPT`.
4. При любой ошибке manifest не передаётся в `rovelle_blob_put()`.
5. Сборка и все существующие тесты проходят.

## Возможные ошибки

### Проверено только существование файла

Тогда повреждённый blob пройдёт проверку. Нужно вычислить hash содержимого и
сравнить его с адресом.

### Проверяется `server/index.js`

Это логический путь будущего файла в релизе. Проверять нужно объект внутри
`store/objects`, найденный по hash.

### Manifest сохраняется до окончания цикла

Если публикация происходит внутри цикла, ошибка в следующей строке обнаружится
слишком поздно. `rovelle_blob_put()` для manifest должен оставаться после
успешной проверки всего документа.

## Решение, которое принимает владелец

Оставить `rovelle_blob_verify()` отдельной публичной функцией модуля blob или
встроить проверку в manifest?

Рекомендуемый вариант — отдельная функция. Только модуль blob должен знать,
как hash превращается в путь `store/objects/xx/...`; manifest получает готовый
статус проверки и не дублирует устройство CAS.
