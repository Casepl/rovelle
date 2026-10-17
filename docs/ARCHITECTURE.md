# Rovelle: принятая архитектура платформы

Статус: accepted design baseline

Дата фиксации: 2026-10-12

Связанный реестр: [DECISIONS.md](DECISIONS.md)

Этот документ фиксирует общее понимание архитектуры после design review. Он
описывает целевую модель. `ROADMAP.md` отвечает за последовательность стримов и
может отставать от этого документа. При конфликте архитектурных формулировок
приоритет имеет этот файл.

## 1. Обещание продукта

Rovelle безопасно доставляет и активирует новую ревизию приложения без
перезапуска узла.

Платформа связывает производство, хранение, доставку и активацию программных и
инженерных артефактов:

```text
source
  -> build graph
  -> tests
  -> immutable artifacts
  -> delivery
  -> prepare
  -> activate
  -> observe
  -> rollback
```

Артефактом может быть browser bundle, SSR runtime, исполняемый файл, ML-модель,
конфигурация, FPGA bitstream, отчёт или evidence bundle.

## 2. Архитектурные слои

```text
BUILD PLANE
  SourceSnapshot -> Target -> ActionGraph -> Executor

ARTIFACT PLANE
  CAS -> Manifest -> Release -> Ref

DELIVERY PLANE
  Control Plane -> Desired State -> Agent -> Prepared Revision

ACTIVATION PLANE
  Supervisor -> Health Gate -> Switch -> Drain -> Rollback

RUNTIME PLANE
  Node.js / Browser / Service / FPGA / ML / Physical system
```

Платформа не смешивает эти обязанности. Например, Slurm выбирает вычислительный
узел, CAS хранит результат, agent доставляет его, а supervisor активирует.

## 3. Основные сущности

### SourceSnapshot

Неизменяемый снимок входного дерева. Источником может быть Git commit, Perforce
changelist, SVN revision или локальный workspace.

### Target

Стабильная пользовательская цель:

```text
//web:bundle
//api:server
//agent:binary
//hdl/counter:simulate
```

### Action

Неизменяемая единица вычисления с объявленными командой, входами, toolchain,
окружением, ресурсами и выходами.

### Artifact

Неизменяемое содержимое, адресуемое по hash.

### Release

Неизменяемый manifest совместимого набора артефактов, тестов и provenance.

### Ref

Изменяемый именованный указатель на release:

```text
refs/production -> revision-42
refs/canary     -> revision-43
```

### Runtime Generation

Запущенный экземпляр одной release revision с собственным module graph,
состоянием и lifecycle.

## 4. Компоненты и языки

| Компонент | Основной язык | Ответственность |
| --- | --- | --- |
| `rovelle-store` | C | локальный CAS, hash, атомарные blobs и manifests |
| `rovelle-control-plane` | Go | API, executions, desired state, rollout и scheduler adapters |
| `rovelle-worker` | Rust | исполнение actions, staging, sandbox и публикация результатов |
| `rovelle-agent` | Rust | доставка, локальный кеш и reconciliation |
| `rovelle-build` | C++ | анализ графа, targets, actions и объяснение пересборки |
| `rovelle-supervisor` | C++ | процессы runtime, IPC, health, switch и drain |
| `rovelle-sdk` | TypeScript | Node.js, SSR, browser ESM и framework adapters |
| `rovelle-lab` | Python | fault injection, симуляция и анализ данных |
| `rovelle-unity-lab` | C# | Unity simulator для Physical AI |

Язык выбирается по обязанности компонента. Разные языки также используются как
реальные клиенты одной build pipeline, а не ради переписывания одной функции.

## 5. Artifact storage

Текущий C CAS является:

- reference implementation формата;
- локальным хранилищем разработки;
- кешем worker и agent;
- backend для single-node установки.

Глобальное хранилище подключается через общий интерфейс:

```text
ArtifactBackend
  LocalFilesystem
  S3 / MinIO
  YTsaurus
  другие object stores
```

Rovelle не строит собственный YTsaurus в критическом пути первой версии.
YTsaurus может подключаться отдельно как artifact backend и как executor для
подходящих data workloads.

## 6. Build platform для разных языков

Language adapter преобразует нативный проект в общий `ActionGraph`:

```text
NodeAdapter ----+
GoAdapter ------+
RustAdapter ----+--> ActionGraph -> Executor
CMakeAdapter ---+
VhdlAdapter ----+
PythonAdapter --+
UnityAdapter ---+
```

Rovelle не заменяет npm, Cargo, Go Modules, pip, Conan, CMake, GHDL, Quartus
или Unity. Она фиксирует их входы, версии, toolchains, действия и результаты.

### Форматы

- внутренний `ActionGraph` имеет версионированный машинный формат, например
  Protobuf;
- первая пользовательская конфигурация использует простой `rovelle.toml`;
- Starlark или собственный rule language добавляется после двух подтверждённых
  случаев повторного использования правил.

Пример:

```toml
[target.web]
type = "node"
root = "web"
outputs = ["dist/**"]
```

### Analysis и execution

Сборка состоит из двух фаз:

1. analysis строит и проверяет граф;
2. execution исполняет готовые actions.

Динамическое обнаружение зависимостей является отдельной discovery action. Её
результат публикуется как версионированный dependency manifest и становится
входом последующей action.

Generated files не записываются обратно в source tree. Они публикуются в CAS и
передаются следующей action как артефакты.

### ActionDigest

Ключ кеша включает:

```text
command
arguments
declared environment
input Merkle tree
toolchain
target platform
build configuration
adapter version
```

Executor и worker не входят в ключ, если они не влияют на результат.

### Уровни воспроизводимости

```text
BEST_EFFORT
DECLARED_INPUTS
HERMETIC
VERIFIED
```

Существующий Make/npm/Cargo flow можно сначала подключить как `BEST_EFFORT`, а
затем постепенно объявить точные inputs, outputs и environment.

### Тестовые события

Все test adapters преобразуют результаты в общий поток:

```text
suite_started
test_started
test_passed
test_failed
attachment
coverage
suite_finished
```

Это относится к Jest, Go test, Cargo, pytest, Unity и HDL simulation.

## 7. Исполнение и compute schedulers

Rovelle управляет графом и кешем. LSF, Slurm, Kubernetes или другой scheduler
решает, где и когда запустить job.

```text
Executor
  LocalExecutor
  SlurmExecutor
  LsfExecutor
  KubernetesExecutor
  YTsaurusExecutor
```

Первым кластерным backend становится Slurm. LSF является обязательной
enterprise-интеграцией, но не первой реализацией.

### Асинхронный протокол

```text
submit(Action) -> ExecutionId
status(ExecutionId)
cancel(ExecutionId)
logs(ExecutionId)
result(ExecutionId)
```

Состояния:

```text
CREATED
STAGING_INPUTS
QUEUED
ALLOCATED
RUNNING
UPLOADING_OUTPUTS
SUCCEEDED
FAILED
CANCELLED
TIMED_OUT
LOST
```

Worker сам получает lease у control plane и отправляет heartbeat. Control plane
не подключается к worker по SSH.

### Семантика надёжности

- исполнение имеет гарантию `at-least-once`;
- повтор безопасен благодаря immutable inputs и content-addressed outputs;
- все outputs сначала загружаются в CAS;
- `ActionResult` публикуется только после проверки полного набора outputs;
- отменённая execution не публикует новый `ActionResult`;
- осиротевшие blobs удаляются позднее garbage collector;
- независимые ветви графа могут продолжать работу;
- зависимые от упавшей action ветви отменяются.

### Короткие и длинные actions

Длинная synthesis или emulation action получает отдельную scheduler job.
Множество коротких actions исполняется внутри выделенной worker session, чтобы
не перегружать scheduler массовыми `sbatch` или `bsub`.

### Ресурсы и toolchains

Portable requirements включают CPU, RAM, disk, timeout, platform labels,
locality, exclusive access и license features.

Toolchain может быть предоставлен как:

```text
native installation
environment module
container image
preinstalled worker label
hardware appliance
```

EDA license является планируемым ресурсом. Rovelle передаёт требование и
измеряет license wait time, но сначала не пишет собственный license manager.

### Sandbox и доверие

```text
NONE
FILESYSTEM
PROCESS
CONTAINER
HARDWARE
```

Кеш разделяется на trust domains: local, team, organization и
verified-release. Результат недоверенного worker не продвигается в production
без нужной policy.

Action можно запустить дважды на независимых workers и сравнить output hashes.

## 8. Release, delivery и activation

Release manifest неизменяем и подписан. Mutable только refs.

Control plane хранит desired state, а agent сообщает observed state:

```text
desired:  r43
prepared: r43
active:   r42
```

Доставка имеет две фазы:

1. `prepare`: скачать, собрать snapshot и проверить всё без влияния на runtime;
2. `activate`: атомарно переключить уже подготовленную generation.

Базовые rollout policies:

```text
all-at-once
rolling
canary
percentage
manual stages
```

Health Gate использует readiness, application health, error rate, latency,
custom verification и manual approval.

Rollback переключает ref на предыдущую подготовленную revision и не запускает
сборку. Предыдущая generation сохраняется на grace period.

Garbage collection учитывает refs, active leases, незавершённые builds и
retention policy.

## 9. Node.js runtime generations

Замена JavaScript-файла на диске не меняет уже созданные V8 functions, module
records, closures, timers, listeners и connections. Поэтому единицей активации
является целая runtime generation.

Первая реализация использует отдельный child process:

```text
Pod
  rovelle-supervisor
    generation A
    generation B
```

Порядок переключения:

```text
prepare B
start B
warm up B
health check B
atomically route new requests to B
drain A
stop A
```

Pod и supervisor не перезапускаются. `vm.Context` не считается достаточной
границей изоляции. Worker Threads и более лёгкие режимы исследуются позднее.

## 10. Изолированные модули

Сначала активируется whole-application generation. После работающего end-to-end
flow отдельные рискованные модули могут получить собственные workers.

Module Descriptor содержит:

```text
revision
server entrypoint
browser entrypoint
runtime/toolchain
contract versions
assets
health checks
resource limits
```

Один логический контракт допускает размещение:

```text
in-process
child-process
same-node service
remote service
browser-only
```

Server render возвращает структурированный результат, а не только HTML:

```json
{
  "html": "...",
  "head": [],
  "assets": [],
  "state": {},
  "diagnostics": {},
  "revision": "r42"
}
```

Shell явно выбирает failure policy: fail-page, render-fallback, omit-module или
use-previous-revision.

Произвольное состояние между generations автоматически не переносится. Модуль
хранит его снаружи или предоставляет версионированные snapshot/restore hooks.

До активации release validator проверяет совместимость контрактов.

## 11. Browser ESM и SSR

Логические specifiers стабильны, физические URL содержат content hash:

```text
@app/main -> /objects/<hash>/main.js
@app/ui   -> /objects/<hash>/ui.js
```

SSR-ответ фиксирует одну release revision и отдаёт import map той же ревизии.
Открытая страница продолжает использовать свою revision. Новая применяется при
следующей навигации или управляемом reload.

Immutable objects получают долгий cache lifetime. HTML и release manifest
ревалидируются. CDN занимается доставкой байтов, но не заменяет release
consistency, activation и rollback.

## 12. Отличие от микрофронтенд-сервисов

Первый supervisor не собирает страницу из удалённых HTML-фрагментов. Он
переключает полную SSR generation. Поэтому это blue-green runtime внутри узла,
а не сеть SSR-микросервисов.

Если каждый модуль станет отдельным процессом, возвращающим HTML, архитектура
сблизится с сервисными микрофронтендами. Отличие Rovelle должно сохраняться в
общей цепочке build, CAS, differential delivery, revision pinning, activation,
rollback и evidence, а также в нескольких вариантах размещения одного модуля.

## 13. Security, provenance и audit

Content hash подтверждает целостность. Release manifest дополнительно
подписывается. Agent самостоятельно проверяет hash, подпись, project identity и
policy перед prepare и activate.

Разрешения разделены:

```text
build
publish
promote
activate
rollback
approve
administer
```

CAS может физически дедуплицировать blobs между tenants, но manifests, refs,
ACL и квоты изолированы.

Production artifact связан с source revision, graph, action digests,
toolchains, workers, tests, approvals и release manifest.

Admission policy проверяет тесты, подписи, допустимые toolchains, approvals и
при необходимости reproducibility verification.

Build, publish, promotion, activation и rollback создают immutable audit
events. Их можно экспортировать во внешнее аналитическое хранилище. Собственная
event database остаётся отдельной продуктовой вертикалью.

Секреты не попадают в manifests, CAS и логи. Action с секретом по умолчанию не
кешируется, пока adapter не объявит безопасную модель.

## 14. Milestones

### M1: single-node end-to-end

```text
source
-> build
-> CAS
-> release
-> prepare
-> activate
-> health check
-> rollback
```

Первая вертикаль: Node.js SSR-приложение с browser ESM.

### M2: delivery cluster

Три application nodes, один control plane, progressive rollout и падение одного
agent.

### M3: distributed build

Slurm execution и общий cache hit между `LocalExecutor` и `SlurmExecutor`.

### M4: multilingual repository

```text
web/      TypeScript
api/      Go
agent/    Rust
storage/  C
hdl/      VHDL
```

Изменение одного подграфа не пересобирает независимые языковые targets.

Последовательность adapters: Node.js/TypeScript, Go, Rust, C/C++, Python,
VHDL/GHDL, C#/Unity.

## 15. Non-goals первой версии

Rovelle не пишет собственные:

- Git или Perforce;
- package manager;
- container runtime;
- Kubernetes;
- LSF, Slurm или YTsaurus;
- frontend framework;
- programming language;
- распределённую event database.

Существующие CI вызывают Rovelle через CLI или HTTP API.

## 16. Критерии успеха

```text
pod restart count          = 0
corrupt activation count   = 0
rollback time              < 10 seconds as initial target
changed bytes transferred  proportional to actual change
in-flight requests lost    = 0
release revision visible   on every node
```

Дополнительные метрики:

```text
publish latency
cache hit ratio
dependency wait
input staging time
scheduler queue time
license wait time
execution time
output upload time
prepare and activation latency
convergence time
recovery after disconnect
```

Численные SLO, кроме первоначальной цели rollback, уточняются после первого
измеряемого прототипа.

## 17. Версионирование внешних контрактов

Manifests, descriptors, events, CLI/HTTP schemas и protocols версионируются с
первого дня. Внутренний код и storage layout до стабилизации могут меняться.
