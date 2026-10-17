# Rovelle Stream 04: manifest превращает blobs в релиз

## Название стрима

> Меняю один файл, а доставляю только изменение | Пишу manifest на C | Rovelle #4

## Результат эфира

К концу стрима должна работать команда:

```sh
./build/rovelle manifest publish <store> <manifest-file>
```

Она проверяет все ссылки на blobs и печатает SHA-256 всего manifest. Этот хеш
является идентификатором ревизии.

## Хук в начале

> У меня уже есть файлы по хешам. Но набор случайных хешей ещё не является
> релизом. Сегодня я соберу manifest: изменю один файл из двух и покажу, почему
> Rovelle не должна заново доставлять второй.

## Простое объяснение новому зрителю

> Я пишу Rovelle, платформу доставки изменений приложения. Сейчас строю её
> нижний слой на C. Файлы уже хранятся по SHA-256. Сегодня объединяю несколько
> файлов в одну неизменяемую ревизию.

Повторить это примерно через 25 минут и после вопроса, которому не хватает
контекста.

## 00:00–00:08. Показать проблему

Нарисовать в Excalidraw:

```text
objects/a1/...  ?
objects/b2/...  ?  → какой из этих файлов образует релиз?
objects/c3/...  ?
```

Потом добавить:

```text
manifest
├── server/index.js  → a1...
└── public/style.css → b2...
```

Проговорить:

> Blob не знает своего имени в приложении. Он знает только содержимое. Manifest
> возвращает логический путь и фиксирует точный набор файлов.

## 00:08–00:18. Зафиксировать протокол

Показать формат:

```text
ROVELLE-MANIFEST-V1
<hash>\tpublic/style.css
<hash>\tserver/index.js
```

Объяснить три решения:

1. Заголовок версионирует формат.
2. Сортировка делает представление каноническим.
3. Хеш самого manifest становится идентификатором ревизии.

## 00:18–00:30. Переиспользовать проверку хеша

Вынести проверку в:

```c
int rovelle_hash_is_valid(const char *hash);
```

Добавить:

```c
rovelle_status_t rovelle_blob_verify(
    const char *store_path,
    const char hash[ROVELLE_SHA256_HEX_SIZE]
);
```

Контрольная точка:

```sh
rbuild
```

Во время сборки посмотреть чат.

## 00:30–00:45. Валидировать логический путь

Написать:

```c
static int is_valid_logical_path(const char *path);
```

Сначала проговорить примеры на экране:

```text
public/app.css     да
server/index.js    да
../secret          нет
/etc/passwd        нет
assets//logo.svg   нет
```

## 00:45–01:15. Прочитать manifest

Реализовать по порядку:

1. `fopen()`.
2. `getline()` для заголовка.
3. Цикл `getline()` для записей.
4. Разделение строки по `\t`.
5. Проверка хеша и пути.
6. Проверка строгой сортировки.
7. `rovelle_blob_verify()` для каждого хеша.
8. `rovelle_blob_put()` для manifest.
9. Cleanup для файла и выделенной строки.

После каждого законченного куска говорить:

```text
Сейчас мне нужно ...
Я ожидаю ...
Проверю это ...
Получилось ...
```

## 01:15–01:25. Добавить CLI

Команда:

```text
rovelle manifest publish <store> <manifest-file>
```

Добавить её в разбор аргументов и `--help`, затем выполнить:

```sh
rbuild
```

## 01:25–01:40. Финальная демонстрация

### Версия 1

```sh
rm -rf /tmp/rovelle-demo
mkdir -p /tmp/rovelle-demo/input

printf 'console.log("v1");\n' > /tmp/rovelle-demo/input/index.js
printf 'body { color: black; }\n' > /tmp/rovelle-demo/input/style.css

JS_V1=$(./build/rovelle blob put \
    /tmp/rovelle-demo/store \
    /tmp/rovelle-demo/input/index.js)

CSS=$(./build/rovelle blob put \
    /tmp/rovelle-demo/store \
    /tmp/rovelle-demo/input/style.css)

printf 'ROVELLE-MANIFEST-V1\n%s\tpublic/style.css\n%s\tserver/index.js\n' \
    "$CSS" "$JS_V1" \
    > /tmp/rovelle-demo/release-v1.rvl

REVISION_V1=$(./build/rovelle manifest publish \
    /tmp/rovelle-demo/store \
    /tmp/rovelle-demo/release-v1.rvl)

echo "$REVISION_V1"
find /tmp/rovelle-demo/store/objects -type f | wc -l
```

Ожидается три объекта: два файла приложения и manifest.

### Версия 2: изменился только JavaScript

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

printf 'v1: %s\nv2: %s\n' "$REVISION_V1" "$REVISION_V2"
find /tmp/rovelle-demo/store/objects -type f | wc -l
```

Теперь объектов пять. Добавились новый `index.js` и новый manifest. Старый CSS
не копировался второй раз.

### Ошибка: manifest ссылается на отсутствующий blob

```sh
MISSING=0000000000000000000000000000000000000000000000000000000000000000

printf 'ROVELLE-MANIFEST-V1\n%s\tserver/index.js\n' \
    "$MISSING" \
    > /tmp/rovelle-demo/broken.rvl

./build/rovelle manifest publish \
    /tmp/rovelle-demo/store \
    /tmp/rovelle-demo/broken.rvl
```

Ожидаемый результат:

```text
manifest publish failed: ROVELLE_NOT_FOUND
```

## Финальный вывод

> В первой ревизии было два файла и manifest. Я изменил только JavaScript. Во
> второй ревизии Rovelle переиспользовала старый CSS и добавила только новый
> JavaScript и новый manifest. Следующий шаг: стабильная ссылка `current`,
> которую можно атомарно переключить с одной ревизии на другую.

## Фрагмент для шортса

Записать финальную демонстрацию отдельным чистым дублем после стрима:

> Я изменил один файл из двух. При доставке архива пришлось бы передать его
> целиком. В Rovelle старый CSS уже лежит по своему хешу, поэтому появились
> только новый JavaScript и новый manifest. Хеш manifest стал идентификатором
> всей ревизии.

На экране последовательно показать `release-v1.rvl`, изменение `index.js`, два
разных хеша ревизии и изменение числа объектов с 3 до 5.
