# Rovelle: реестр принятых решений

Статус: все перечисленные решения приняты.

Дата фиксации: 2026-10-12.

Нормативное описание: [ARCHITECTURE.md](ARCHITECTURE.md).

Этот файл нужен для учёта design review. Номера относятся к вопросам в
обсуждении. Архитектура определяется содержанием `ARCHITECTURE.md`, а не
необходимостью восстанавливать историю чата.

## Ранние решения

До текущего подробного реестра были приняты следующие направления:

- результат шести месяцев: устанавливаемый open-source продукт;
- дополнительные результаты: инженерный бренд, системное обучение и переход к
  Physical AI;
- обещание: безопасная доставка и активация без перезапуска узла;
- два параллельных трека: продукт и ограниченные исследовательские spikes;
- вертикали являются самостоятельными продуктами и формируют требования к
  Rovelle;
- одновременно активны платформа и не более двух продуктовых вертикалей;
- общий компонент извлекается после двух реальных применений;
- production languages: C, Go, Rust, TypeScript, C++, Python и C#;
- VHDL analyzer является отдельной build/evidence вертикалью;
- AI не входит в trusted core анализатора и только предлагает проверяемые
  правила, объяснения и evidence gaps;
- базовый trusted core анализатора детерминирован: parser, semantics, rules,
  tests и provenance;
- build kernel общий для software, FPGA, ML и других инженерных артефактов;
- Git, SVN и Perforce подключаются adapters и не заменяются платформой.

## Q21-Q25: AI и анализатор

- Q21: AI находится вне trusted core.
- Q22: первый AI-сценарий превращает текст требования в draft детерминированного
  правила и тестов с обязательным human review.
- Q23: сохраняется полный provenance AI-предложения и человеческого решения.
- Q24: внешние AI providers допускаются policy, локальные adapters поддерживаются,
  передаваемый исходный код минимизируется.
- Q25: AI добавляется после parser, AST, dependency graph, deterministic rules,
  fixtures и metrics.

## Q26-Q31: общий build kernel

- Q26: один build kernel и специализированные domain adapters.
- Q27: SCM не заменяется, используются Git/Perforce/SVN/workspace adapters.
- Q28: первый FPGA flow: GHDL, dependency graph/cache, затем Quartus и Vivado.
- Q29: FPGA cache key учитывает tool/version, device, speed grade, constraints,
  rule pack и reference checkpoint.
- Q30: cross-domain demo показывает общий кеш и `why-rebuilt` для software и
  VHDL.
- Q31: позиционирование: engineering production platform для software, FPGA и
  AI artifacts.

## Q32-Q42: distributed execution

- Q32: Rovelle управляет graph/actions, scheduler управляет ресурсами.
- Q33: Executor имеет асинхронный протокол.
- Q34: inputs и outputs передаются через CAS; shared filesystem является
  дополнительным adapter.
- Q35: первый кластерный backend Slurm, затем LSF.
- Q36: длинные actions являются отдельными jobs, короткие работают в worker
  sessions.
- Q37: Go control plane, Rust worker, C storage core.
- Q38: worker сам получает работу, исходящее SSH-подключение не требуется.
- Q39: lease и heartbeat обеспечивают восстановление после потери worker.
- Q40: toolchains поддерживают native, modules, containers, worker labels и
  hardware appliances.
- Q41: первый cluster prototype запускает GHDL через Slurm.
- Q42: один Action contract работает локально и через schedulers; backend не
  меняет cache key без фактического влияния на результат.

## Q43-Q51: runtime и storage

- Q43: единица серверной активации целая runtime generation.
- Q44: первая изоляция Node.js использует child process.
- Q45: логические module names стабильны, физические URL содержат content hash.
- Q46: SSR и browser assets фиксируют одну release revision.
- Q47: открытая страница остаётся на своей revision до навигации или reload.
- Q48: исходная формулировка заменена: разные языки используются для проверки
  одной pipeline, а не для искусственного переписывания компонентов.
- Q49: C CAS является локальным `ArtifactBackend`, а не обязательным глобальным
  distributed storage.
- Q50: YTsaurus подключается отдельными artifact и executor adapters.
- Q51: собственная event database не входит в критический путь первой Rovelle.

## Q52-Q60: modules и multilingual pipeline

- Q52: сначала whole-application generation, затем изоляция модулей.
- Q53: module contract шире HTML и включает lifecycle, health, assets, state и
  diagnostics.
- Q54: один contract допускает process, remote service и browser ESM.
- Q55: language adapter преобразует проект в универсальный ActionGraph.
- Q56: нативные package managers остаются источниками dependency resolution.
- Q57: приняты уровни BEST_EFFORT, DECLARED_INPUTS, HERMETIC и VERIFIED.
- Q58: итоговый demo repository содержит TypeScript, Go, Rust, C и VHDL.
- Q59: release manifest может содержать разнотипные artifacts с отдельными
  rollout policies.
- Q60: запрос фиксирует release revision для server modules и browser assets.

## Q61-Q73: описание и семантика графа

- Q61: внутренний ActionGraph имеет версионированный машинный формат.
- Q62: первая конфигурация использует `rovelle.toml`; rule language появляется
  после двух применений.
- Q63: targets адресуются как `//path:target`.
- Q64: ActionDigest включает command, environment, inputs, toolchain, platform,
  configuration и adapter version.
- Q65: существующие flows подключаются постепенно, начиная с наблюдения.
- Q66: adapters появляются в порядке Node, Go, Rust, C/C++, Python, VHDL и C#.
- Q67: Target анализируется в одну или несколько immutable Actions.
- Q68: analysis и execution разделены.
- Q69: dynamic dependency discovery является отдельной action.
- Q70: generated files публикуются в CAS, а не source tree.
- Q71: secrets не попадают в CAS/logs и по умолчанию отключают кеширование.
- Q72: test runners преобразуются в общий event stream.
- Q73: независимые branches могут продолжать работу; доступны fail-fast и
  keep-going.

## Q74-Q88: надёжность execution и release

- Q74: execution имеет at-least-once semantics.
- Q75: ActionResult публикуется атомарно после загрузки всех outputs.
- Q76: cache разделён по trust domains.
- Q77: sandbox levels: NONE, FILESYSTEM, PROCESS, CONTAINER и HARDWARE.
- Q78: отмена запрещает публикацию результата; blobs очищаются GC.
- Q79: reproducibility проверяется двойным независимым исполнением.
- Q80: сохраняются structured events и stdout/stderr artifacts.
- Q81: release является immutable signed manifest.
- Q82: mutable refs изменяются compare-and-swap.
- Q83: desired, prepared и active state разделены.
- Q84: delivery разделена на prepare и activate.
- Q85: поддерживаются all-at-once, rolling, canary, percentage и manual stages.
- Q86: rollout управляется Health Gate из нескольких сигналов.
- Q87: rollback использует уже подготовленную revision.
- Q88: GC учитывает refs, leases, builds и retention.

## Q89-Q103: runtime, browser и security

- Q89: Module Descriptor описывает entries, runtime, contracts, assets, health
  и resources.
- Q90: запрос pin-ится к release revision.
- Q91: render возвращает структурированный результат.
- Q92: placement выбирается policy, а не module descriptor.
- Q93: shell явно задаёт failure behavior.
- Q94: state хранится снаружи или переносится versioned hooks.
- Q95: contract compatibility проверяется до activation.
- Q96: browser page не переключается под работающим пользователем.
- Q97: release manifest подписывается.
- Q98: agent самостоятельно проверяет identity, signature, hash и policy.
- Q99: build, publish, promote, activate, rollback, approve и administer имеют
  разные permissions.
- Q100: tenant manifests, refs, ACL и quotas изолированы.
- Q101: production artifact имеет end-to-end provenance.
- Q102: admission policy проверяет tests, signatures, toolchains и approvals.
- Q103: lifecycle создаёт immutable audit events для внешней аналитики.

## Q104-Q111: граница первого продукта

- Q104: первая вертикаль Node.js SSR с browser ESM.
- Q105: первый milestone single-node end-to-end.
- Q106: второй milestone три nodes и progressive rollout.
- Q107: третий milestone Slurm и общий cache с LocalExecutor.
- Q108: Git, package manager, container runtime, Kubernetes, schedulers,
  YTsaurus, frameworks и languages не переписываются.
- Q109: success metrics включают zero pod restart, zero corrupt activation,
  zero lost in-flight requests и быстрый rollback.
- Q110: существующие CI используют CLI и HTTP API.
- Q111: внешние schemas и protocols версионируются с первого дня.

## Правило дальнейшего учёта

Фраза «принимаю рекомендуемые» закрывает все ожидающие вопросы с рекомендованным
ответом, кроме явно названных исключений. Новые решения добавляются в этот
реестр и затем отражаются в `ARCHITECTURE.md`.
