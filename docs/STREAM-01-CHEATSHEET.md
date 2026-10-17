# Стрим 1: шпаргалка ведущего

## Цель

К концу эфира команда ниже печатает SHA-256 файла:

```sh
./build/rovelle hash README.md
```

Одинаковое содержимое должно давать одинаковый идентификатор независимо от
имени файла.

## Первые две минуты

> Я 13 лет не писал на C. Сегодня начинаю Rovelle, платформу доставки файлов и
> приложений без перезапуска подов. До всей платформы еще далеко. На этом
> стриме дадим содержимому файла устойчивый идентификатор.

Показать цепочку:

```text
file -> SHA-256 -> blob -> manifest -> revision -> delivery
```

## Стартовые команды

```sh
cd /path/to/rovelle
./tools/stream-cmake -S . -B build
./tools/stream-cmake --build build
./build/rovelle --help
./build/rovelle version
```

На стриме использовать `./tools/stream-cmake` вместо прямого вызова `cmake`.
Обертка скрывает имя пользователя и локальный путь проекта в выводе CMake и
компилятора. Обычные команды CMake остаются доступны для работы вне эфира.

## План кода

1. Создать `include/rovelle/hash.h`.
2. Объявить `rovelle_hash_file(path, out_hex)`.
3. Создать `src/hash.c`.
4. Открыть файл через `fopen(path, "rb")`.
5. Читать файл блоками.
6. Передавать блоки в OpenSSL EVP.
7. Преобразовать 32 байта в 64 hex-символа и добавить `\0`.
8. Подключить `OpenSSL::Crypto` в CMake.
9. Добавить `rovelle hash <file>` в CLI.

Нужные имена OpenSSL:

```text
EVP_MD_CTX_new
EVP_DigestInit_ex
EVP_sha256
EVP_DigestUpdate
EVP_DigestFinal_ex
EVP_MD_CTX_free
```

## Что проговаривать

Перед изменением:

1. Что я сейчас хочу получить?
2. Что здесь может сломаться?
3. Как я пойму, что получилось?

После ошибки:

1. Сначала назвать гипотезу.
2. Проверить ее одной командой.
3. Объяснить результат проверки.

## Проверка результата

```sh
./build/rovelle hash README.md
shasum -a 256 README.md

cp README.md /tmp/rovelle-copy.md
./build/rovelle hash /tmp/rovelle-copy.md

: > /tmp/rovelle-empty
./build/rovelle hash /tmp/rovelle-empty

./build/rovelle hash /tmp/rovelle-missing
```

## Если застрял

Если OpenSSL или CMake не поддаются 15 минут, закончить потоковое чтение файла,
показать задуманный API и записать конкретную проблему. Не начинать CAS и
сетевую доставку.

## Завершение

Показать `git diff` и ответить на три вопроса:

1. Что сегодня заработало?
2. Какой инвариант мы получили?
3. Что будет на следующем стриме?

Следующая задача: сохранить blob по его hash через временный файл и атомарное
переименование.
