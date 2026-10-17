# Anthropic Scion: source control для потока агентов

- Статус: `analyzed`
- Добавлено: `2026-10-13`
- Автор первичного материала: Anthropic source control team
- Первичный источник: <https://www.anthropic.com/engineering/redesigning-source-control>
- Дополнительный разбор: Александр Поломодов, «Книжный куб»
- Связанные части Rovelle: Repo, CAS, SourceSnapshot, node cache, derived data

## Зачем мы это рассматриваем

Rovelle планирует отдельную вертикаль монорепозитория и уже использует
content-addressed storage. Scion показывает один вариант source control для
огромного always-online потока агентов и CI jobs.

## Claims источника

| ID | Утверждение | Тип | Основание | Ограничения |
|---|---|---|---|---|
| C1 | Число слияний выросло примерно с 2 500 до более 30 000 в день. | `reported-measurement` | Данные Anthropic | Нет независимой методики и связи с доставленной ценностью. |
| C2 | Always-online клиенты позволяют перенести history, blame, merge и rebase на сервер. | `design-principle` | Архитектура Scion | Не подходит для offline-first и небольших репозиториев автоматически. |
| C3 | Changeset хранит изменения относительно родителя, а дополнительные индексы строятся как derived data. | `design-principle` | Описание модели Scion | Требует отдельной модели consistency и восстановления индексов. |
| C4 | Иерархия daemon, regional cache и authoritative server уменьшила стоимость checkout. | `reported-measurement` | Измерения Anthropic | Нагрузка и стоимость базовой инфраструктуры не раскрыты полностью. |
| C5 | Рабочая копия может быть sparse и материализоваться по требованию. | `design-principle` | Протокол Scion | Нужны semantics для изменений, watchers и инструментов, ожидающих полное дерево. |

## Что утверждает обозреватель

В аннотациях Scion сравнивается с распределённой базой данных: changeset похож
на WAL, дополнительные структуры — на CQRS projections, а CAS и cache tiers —
на data plane. Также поставлено под сомнение слово `instantly`: durability,
visibility ref и готовность derived indexes являются разными состояниями.

## Наши вопросы и возражения

- Какие consistency guarantees нужны между changeset, ref и indexes?
- Как восстанавливается node daemon после потери cache?
- Где проходит граница metadata plane и blob data plane?
- Насколько результаты зависят от Spanner и существующей инфраструктуры?
- Нужен ли Rovelle native VCS до появления нагрузки, которую не выдерживает Git?

## Гипотезы для Rovelle

| ID | Гипотеза | Ожидаемый эффект | Как опровергнуть |
|---|---|---|---|
| H1 | Общий daemon-cache на узле уменьшит повторную доставку inputs для jobs. | Меньше сетевого трафика и быстрее warm actions. | На реальном workload hit rate и время не улучшаются достаточно, чтобы оправдать daemon. |
| H2 | Primary log и перестраиваемые projections подходят Repo и event vertical. | Новые индексы добавляются без изменения write path. | Восстановление и consistency делают модель сложнее прямого transactional storage. |
| H3 | Sparse materialization уменьшит стоимость работы с большой монорепой. | Executor получает только declared inputs. | Toolchains требуют почти полное дерево или discovery постоянно нарушает границу. |

## Эксперимент

- Статус: не запланирован;
- предварительное условие: завершить локальный CAS v1 и Action inputs;
- минимальный кандидат: два локальных jobs используют общий node cache;
- метрики: downloaded bytes, cache hit ratio, preparation latency.

## Решение

- Итог: `не принято`;
- отдельные идеи совпадают с текущей архитектурой CAS, но claims Scion не
  являются доказательством требований Rovelle Repo;
- native source control остаётся будущей продуктовой вертикалью.

## Журнал обсуждения

### 2026-10-15

Отделили автора статьи Anthropic от комментариев Александра Поломодова.
Зафиксировали, что Scion полезен как референс, а не как готовая спецификация
Rovelle Repo.
