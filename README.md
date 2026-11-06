# TcgServer

Сервер для карточной игры (TCG) на C++17 с использованием Boost.Asio.
Принимает TCP-подключения, позволяет создавать игровые комнаты, присоединяться
к ним и получать список доступных комнат. При заполнении комнаты (2/2)
сервер инициализирует матч, отправляет клиентам полное состояние игры и запускает 
таймер хода. Ход можно завершить вручную или по истечении таймера - сервер самостоятельно
перключит игрока.

Протокол - бинарный, с length-prefixed фреймингом. Общение идёт байтами
с явными типами и длинами, без `\0`-терминаторов и текстовых разделителей.

---

## Содержание

- [Возможности](#возможности)
- [Требования](#требования)
- [Установка зависимостей](#установка-зависимостей)
   - [Windows](#windows)
   - [Linux](#linux)
   - [MacOs](#macos)
- [Сборка проекта](#сборка-проекта)
    - [Windows](#windows-1)
    - [Linux](#linux-1)
    - [MacOs](#macos-1)
- [Запуск](#запуск)
    - [Смена порта](#смена-порта)
- [Docker](#docker)
    - [Требования](#требования-1)
    - [Запуск](#запуск-1)
    - [Просмотр логов](#просмотр-логов)
    - [Проверка статуса](#проверка-статуса)
    - [Остановка и запуск](#остановка-и-запуск)
    - [Пересборка после правок кода](#пересборка-после-правок-кода)
    - [Изменение конфигурации в работающем контейнере](#изменение-конфигурации-в-работающем-контейнере)
    - [Что внутри образа](#что-внутри-образа)
- [Краткое описание модулей](#краткое-описание-модулей)
- [Сетевой протокол](#сетевой-протокол)
   - [Версия wire-формата](#версия-wire-формата)
   - [Формат пакета](#формат-пакета)
   - [Кодировка полей](#кодирование-полей)
   - [Идентификация карт](#идентификация-карт)
   - [Опкоды клиента](#опкоды-клиента)
   - [Опкоды сервера](#опкоды-сервера)
   - [Старт матча](#старт-матча)
   - [Синхронизация состояний](#синхронизация-состояний)
   - [Управление ходом](#управление-ходом)
   - [Инициализация матча](#инициализация-матча)
   - [Пример обмена](#примеры-обмена)
   - [Примеры формата пакетов сервера при MatchStart, StateSync, EndTurn](#примеры-формата-пакетов-сервера-при-matchstart-statesync-endturn)
   - [Почему бинарный формат](#почему-бинарный-формат)
- [Конфигурация](#конфигурация)
   - [Расположение файла](#расположение-файла)
   - [Уровни логирования](#уровни-логирования)
   - [Приоритет источников](#приоритет-источников)
   - [Проверка зашруженной конфигурации](#проверка-загруженной-конфигурации)

---

## Возможности

- Приём TCP-подключений (асинхронный ввод-вывод через Boost.Asio).
- Создание игровых комнат с уникальным ID.
- Присоединение игроков к существующим комнатам (максимум 2 игрока).
- Получение списка доступных комнат.
- Автоматическое удаление пустых комнат.
- Инициализация матча при заполнении комнаты (2/2): раздача карт, атрибуты,
  бросок кубика для определения первого хода, снимок руки и колоды.
- Старт матча - один раз на игрока после заполнения комнаты.
- Серверный таймер хода: `30 с` на игрока, тик `1 с`. Каждый тик сервер
рассылает `StateSync` с актуальным `current_turn_time`. По достижении `0`
ход автоматически переходит к другому игроку, таймер сбрасывается на `30`.
- Клиентский ход: игрок может завершить свой ход
  досрочно. Сервер валидирует, что вызвал именно текущий игрок - иначе
  игнорирует.
- Добор карты при смене хода: при любом переходе хода новый текущий игрок берёт
  верхнюю карту (`deck.back()`) из своей колоды в руку. Добор пропускается
  только когда игрок уже получил суммарно `>= kDeckSize` карт
  (`hand + discard`, соответствует `YourRecieved` / `OpponentRecieved`
  в `StateSync`) - в лог пишется
  `Draw skipped: <playerId> at max received (<N> >= <kDeckSize>)`.
  Что означает, что после того как игрок получил все `kDeckSize`
  карт колоды, добор больше не происходит.
- Синхронизация - полный snapshot состояния.
- Хранение состояния игроков в `Room` между отправками: `hand` / `deck` /
  `discard` / `attributes` не пересоздаются на каждый пакет.
- Идентификация карт по стабильным `u16 id` - 2 байта на карту вместо
  `RowName` (`u16 length + UTF-8`).
- Версия `CardRegistry` (FNV-1a хэш) в шапке `MatchStart` / `StateSync`
  для детекта рассинхронизации карт между сервером и клиентом.
- Отслеживание фазы игры: `Init` / `Playing` / `Finished` плюс `WinnerPlayerId`.
- Бинарный wire-формат с length-prefix фреймингом.
- Логирование с уровнями (`TRACE`/`DEBUG`/`INFO`/`WARN`/`ERROR`/`FATAL`),
  миллисекундами, ID сессии, IP клиента и hex-дампами.
- Конфигурация через `config.ini` (уровень логов, порт).
- Кроссплатформенный код (Windows / Linux / macOS).
- Готов к запуску в Docker (многостадийная сборка (multi-stage build), 
финальный образ без дистрибутива (distroless runtime)).

> Размер колоды - `kDeckSize = 30`.
> Стартовая рука - `kInitialHandSize = 5`.
> Полный размер «пула карт» игрока за матч:
> `hand + discard + deck = 30`.

---

## Требования

| Компонент | Минимальная версия | Назначение |
|---|---|---|
| **C++ компилятор** | C++17 | MSVC 2019+, GCC 9+, Clang 10+ |
| **CMake** | 3.15 | Система сборки |
| **Boost.Asio** | 1.70+ | Асинхронный сетевой ввод-вывод |
| **vcpkg** (рекомендуется) | последняя | Менеджер пакетов для Boost |
| **Visual Studio** (Windows) | 2019 / 2022 | Компилятор MSVC + MSBuild |
| **Docker** (опционально) | 20.10+ | Запуск в контейнере |
| **Git** | любая | Клонирование репозитория |

> **Примечание.** Boost.Asio - header-only библиотека. Под Windows требуется
> линковка с системными библиотеками `ws2_32` и `mswsock`. Под Linux -
> с `pthread`.

---

## Установка зависимостей

### Windows

1. **Установите Visual Studio 2022** с компонентом «Разработка классических
   приложений на C++». Скачать: <https://visualstudio.microsoft.com/>

2. **Установите CMake** (или используйте версию из VS).
   Скачать: <https://cmake.org/download/>

3. **Установите vcpkg.**

   ```powershell
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   cd C:\vcpkg
   .\bootstrap-vcpkg.bat
   ```

4. **Установите Boost.Asio через vcpkg.**

   ```powershell
   .\vcpkg install boost-asio:x64-windows
   ```

   Этой команды достаточно. vcpkg автоматически подтянет транзитивные
   зависимости: `boost-system`, `boost-core`, `boost-config`, `boost-array`
   и другие.

5. **Проверьте установку.**

   ```powershell
   .\vcpkg list | Select-String "boost-"
   ```

   В списке должны присутствовать `boost-asio:x64-windows` и
   `boost-system:x64-windows`.

### Linux

```bash
sudo apt update
sudo apt install build-essential cmake git libboost-all-dev
```

### macOS

```bash
brew install cmake boost
```

---

## Сборка проекта

### Windows

1. **Клонируйте репозиторий:**

   ```powershell
   git clone https://github.com/Arrolepg/TcgServer.git
   cd TcgServer
   ```

2. **Сгенерируйте решение (Debug):**

   ```powershell
   mkdir build
   cd build

   cmake .. `
     -G "Visual Studio 17 2022" -A x64 `
     -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
     -DVCPKG_TARGET_TRIPLET=x64-windows
   ```

3. **Соберите проект:**

   ```powershell
   cmake --build . --config Debug
   ```

   Или откройте `build\TcgServer.sln` в Visual Studio / Rider и нажмите
   **Build**. Конфигурацию (Debug / Release) можно переключить в выпадающем
   списке вверху IDE.

### Linux

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
```

Если `cmake --build` недоступен в старой версии - используйте
`make -j$(nproc)`.

### macOS

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
```

Если `cmake --build` недоступен - используйте
`make -j$(sysctl -n hw.ncpu)`.

---

## Запуск

После успешной сборки бинарник лежит в:

- **Windows:** `build\Debug\TcgServer.exe` (или `build\Release\TcgServer.exe`)
- **Linux / macOS:** `build/TcgServer`

Запустите его:

```powershell
.\TcgServer.exe
```

В консоли появится:

```
2026-09-24 13:24:08.123 [INFO ] [T:1A7C] [MAIN] Config loaded from: ...\config.ini
2026-09-24 13:24:08.123 [INFO ] [T:1A7C] [MAIN] Log level: DEBUG (from config)
2026-09-24 13:24:08.123 [INFO ] [T:1A7C] [MAIN] Port: 8080
2026-09-24 13:24:08.124 [INFO ] [T:1A7C] [SERVER] Listening on 0.0.0.0:8080
```

Сервер готов принимать подключения на порту **8080**.

### Смена порта

Порт задаётся в `config.ini` (см. раздел [Конфигурация](#конфигурация)).

---

## Docker

Сервер собирается и запускается в Docker. Используется **multi-stage build**:

- **Stage 1 (builder)** - `debian:bookworm-slim` с полным toolchain
  (GCC, CMake, Boost SDK).
- **Stage 2 (runtime)** - `gcr.io/distroless/cc-debian12:nonroot`, содержащий
  только бинарник и минимальные системные библиотеки (~25 МБ против ~1.5 ГБ).

### Требования

| Компонент | Минимальная версия | Назначение |
|---|---|---|
| **Docker Desktop** (Windows / macOS) | 20.10+ | Запуск контейнеров |
| **Docker Engine + Compose** (Linux) | 20.10+ | Запуск контейнеров |
| **Свободный порт** | 8080 | Проброс на хост |

### Запуск

Из корня проекта:

```powershell
docker compose up -d --build
```

Что произойдёт:

1. Собирается образ `tcgserver:latest` (первая сборка ~2–5 минут).
2. Запускается контейнер `tcgserver` в фоне.
3. Порт `8080` пробрасывается на хост.
4. Создаются два named volume: `tcgserver_tcg-config` и `tcgserver_tcg-logs`.

### Просмотр логов

```powershell
docker compose logs -f tcgserver
```

`Ctrl+C` - выйти из просмотра (контейнер продолжит работать).

### Проверка статуса

```powershell
docker compose ps
```

Ожидаемый вывод:

```
NAME        IMAGE              STATUS         PORTS
tcgserver   tcgserver:latest   Up 5 seconds   0.0.0.0:8080->8080/tcp
```

### Остановка и запуск

| Команда | Что делает |
|---|---|
| `docker compose stop` | Остановить (контейнер остаётся) |
| `docker compose start` | Запустить остановленный |
| `docker compose down` | Остановить и удалить контейнер (volumes сохраняются) |
| `docker compose down -v` | Остановить и удалить всё, включая volumes |

### Пересборка после правок кода

```powershell
docker compose up -d --build
```

### Изменение конфигурации в работающем контейнере

`config.ini` живёт в named volume. Два способа его поменять.

**Способ 1 - через `docker cp`:**

```powershell
docker cp .\config.ini tcgserver:/app/config/config.ini
docker compose restart
```

**Способ 2 - сбросить volume и пересоздать из образа:**

```powershell
docker compose down
docker volume rm tcgserver_tcg-config
docker compose up -d
```

После этого volume инициализируется тем `config.ini`, который запечён
в образ (из корня проекта на момент сборки).

### Что внутри образа

| Слой | Содержимое |
|---|---|
| **builder (отбрасывается)** | GCC, CMake, git, Boost SDK, исходники |
| **runtime (финальный)** | `TcgServer`, `config.ini`, `glibc`, `libstdc++` |

Boost SDK **не попадает в runtime** - Asio header-only, весь нужный код уже
в бинарнике при сборке.

---

## Краткое описание модулей

- **Server** - асинхронно принимает TCP-подключения, создаёт `Session`
  на каждого клиента.
- **Session** - обрабатывает входящие пакеты, хранит состояние игрока
  (имя, комнату), ведёт аккумулятор входящих байт и парсит пакеты
  по length-prefix.
- **PlayerState** - снимок состояния одного игрока в комнате:
  `playerId` / `playerName`; `attributes` (массив `AttrValue` - max/current
  для `health` / `mana` / `defence`); `hand` / `deck` / `discard` как
  `std::vector<uint16_t>`. Дефолты атрибутов
  (15 / 10 / 10) задаются в конструкторе `PlayerState::PlayerState()`.
- **Room** - модель игровой комнаты. Хранит игроков, IP хоста и **полное
  состояние матча** (`hostState_` / `guestState_`): hand, deck, discard,
  attributes, game phase, turn, winner. Отвечает за:
    - инициализацию матча : раздача, бросок кубика, установка и запуск таймера;
    - серверный таймер хода реализован на boost::asio::steady_timer;
    - переключение хода
    - обработку клиентского завершения хода: валидация, что
      вызвал именно текущий игрок, иначе игнорирует;
    - полный snapshot обоим игрокам.

При отключении любого игрока комната закрывается целиком (
оставшийся игрок получает `Error("Opponent disconnected")` и его
сессия принудительно закрывается).
- **CardRegistry** - таблица `{u16 id, string rowName}` на сервере.
  Ручной дубликат `DT_Cards` (standalone C++ не читает UAsset). `version()` -
  FNV-1a от таблицы, для детекта рассинхронизации с клиентом. См.
  [Идентификация карт](#идентификация-карт).
- **Protocol** - бинарный `Writer` / `Reader` (u8 / u16 / u32 / string),
  сборка и парсинг фреймов.
- **Logger** - уровни, компоненты, ID сессии, hex-дампы, потокобезопасность.
- **Utils** - преобразование ANSI → UTF-8, генерация ID комнат.
- **Config** - парсинг `config.ini`.

---

## Сетевой протокол

Протокол - бинарный, length-prefixed.

### Версия wire-формата

Текущая версия - **v2** (`Protocol::kWireVersion = 2`). Она встроена в шапку
опкодов `MatchStart` и `StateSync` как `u32 wire_version`.

- **v1** - карты идентифицировались `RowName` (`u16 len + UTF-8`).
- **v2** - карты идентифицируются `u16 id`; добавлены `registry_version`,
  `GamePhase`, `discard`, extension-fields.

Клиент проверяет `wire_version` при парсинге. При несовпадении пакет
отвергается, в лог уходит `wire version mismatch`. **v2 не совместим с v1** -
сервер и клиент нужно пересобирать вместе.

### Формат пакета

Каждый пакет в TCP-потоке имеет вид:

```
 0               2               3               N
 +---------------+---------------+---------------+
 |    length     |    opcode     |    payload    |
 |    (u16 LE)   |     (u8)      |   (N-1 байт)  |
 +---------------+---------------+---------------+
```

где N = length.

| Поле | Размер | Описание |
|---|---|---|
| `length` | 2 байта | Размер опкода и payload вместе (минимум 1). Little-endian. |
| `opcode` | 1 байт | Тип сообщения. |
| `payload` | `length - 1` байт | Сырые данные. |

### Кодирование полей

Внутри payload числа и строки кодируются фиксированным образом:

| Тип | Кодировка |
|---|---|
| `u8` | 1 байт |
| `u16` | 2 байта, little-endian |
| `u32` | 4 байта, little-endian |
| `string` | `u16 length` + `length` байт UTF-8 |
| `cardId` | 2 байта, `u16` little-endian (см. [Идентификация карт](#идентификация-карт)) |

**Пример:** строка `"Alice"` в wire-формате:

```
05 00 41 6C 69 63 65
|---| |------------|
   |        |- 5 байт UTF-8 ("Alice")
   |- длина = 5 (u16 LE)
```

**Пример полного пакета** `CreateRoom("Alice")`:

```
08 00 01 05 00 41 6C 69 63 65
|----| | |---| |------------|
   |   |   |         |
   |   |   |         |- "Alice" UTF-8 (5 байт)
   |   |   |- длина строки = 5
   |   |- opcode = 0x01 (CreateRoom)
   |- length всего пакета = 8
```

### Идентификация карт

В wire-формате карта - это **`u16 id`** (2 байта). `rowName`
(например, `"Fireball"`) живёт только на концах соединения.

**Единым источником правды считается - `DT_Cards`** (DataTable в UE, UAsset).

**Сервер** не может читать UAsset, поэтому держит
свою таблицу `{id, rowName}`. Синхронизация с `DT_Cards` - ручная.

**Проверка консистентности.** В шапке `MatchStart` и `StateSync` сервер отправляет
`u32 registry_version` - FNV-1a хэш от `(id, rowName)` всей таблицы. Клиент
считает хэш от сервера и сравнивает. При расхождении:

- матч продолжается;
- в лог клиента - `CardRegistry mismatch`;
- для неизвестных id клиент подставляет `<unknown>` вместо имени.

Это защита от типичной ошибки «добавил карту только на сервере».

**Требования к `rowName`**:

| Требование |
|---|
| **Точное совпадение** (case-sensitive) |
| Без пробелов по краям |
| Без `,` и `"` |
| Уникален |
| ≤ 255 символов |

Если версии разошлись из-за опечатки - сервер продолжит работать,
но клиент покажет `<unknown>` для этой карты. Ловится по
`CardRegistry mismatch` в логе клиента.

Пример лога клиента:
```
LogTcgNetwork: 
    Warning: 
        MatchStart: 
            CardRegistry mismatch (wire=0x447E5F6F local=0x41DF532A). 
Card names may be wrong; sync DT_Cards.csv with client registry.
```

**Добавление новой карты:**

1. В `DT_Cards` в UE добавить новую строку карты.
2. В `CardRegistry.cpp` на сервере добавить `{N, "RowName"}`.
3. В `TcgCardRegistry.cpp` на сервере добавить `{N, "RowName"}`.
4. `version()` пересчитается автоматически при сборке.

**Лимит id карт.** `u16` -> максимум **65 535 карт**.

### Опкоды клиента

| Код | Название | Payload |
|---|---|---|
| `0x01` | `CreateRoom` | `string playerName` |
| `0x02` | `JoinRoom` | `string roomId` + `string playerName` |
| `0x03` | `ListRooms` | - (пусто) |
| `0x04` | `EndTurn` | - (пусто)|

> **Примечание.** `EndTurn` не имеет прямого ответа - результат приходит 
> как `StateSync` обоим игрокам.

### Опкоды сервера

| Код | Название | Payload |
|---|---|---|
| `0x80` | `Error` | `string message` |
| `0x81` | `CreateRoomResponse` | `string roomId` |
| `0x82` | `JoinRoomResponse` | `string roomId` |
| `0x83` | `ListRoomsResponse` | `u16 count` + `count × (string roomId, string hostIp, u8 playersCount)` |
| `0x90` | `MatchStart` | см. [Старт матча](#старт-матча) |
| `0x91` | `StateSync` | см. [Синхронизация состояний](#синхронизация-состояний) |

> **Примечание.** Если в `ListRoomsResponse` нет ни одной комнаты,
> `count = 0` и payload содержит только `u16 0x0000`.

### Старт матча

Сервер отправляет обоим игрокам **один раз** при заполнении комнаты (2/2).
Содержит всё, что нужно клиенту для входа в матч.

Формат пакета при иниицализации матча:

```
u32 wire_version
u32 registry_version
str room_id
str your_player_id
str your_player_name
u16 your_attr_count
u16 your_hand_count
u16 your_deck_count
str opponent_player_id
str opponent_player_name
u16 opp_attr_count
u16 opp_hand_count
str first_player_id
u8  dice_roll
u8  current_turn_time
u8  turn_time_limit
```

`your_attr_count = opp_attr_count = 3` - `health` / `mana` / `defence`.
Порядок и `rowName` фиксированы.

### Синхронизация состояний

Полный snapshot состояния матча. Сервер отправляет:

- после каждого игрового действия;
- `state_seq` начинается с 1 (первое действие) и растёт монотонно.

**На старте `StateSync` не отправляется.** Стартовое состояние передаётся
в `MatchStart (0x90)`, что явялется событием, а не потоком состояния.

Клиент хранит последний `LastStateSync` для catch-up и рендерит UI.

Формат пакетов при синхронизации состояний:

```
u32 wire_version
u32 registry_version
u32 state_seq
str room_id
u8  game_phase
u8  turn_number
str current_turn_player_id
u8  dice_roll
str winner_player_id
u8  current_turn_time
u8  turn_time_limit

str your_player_id
str your_player_name
u16 your_attr_count
u16 your_hand_count
u16 your_deck_count
u16 your_discard_count

str opponent_player_id
str opponent_player_name
u16 opp_attr_count
u16 opp_hand_count
u16 opp_deck_count
u16 opp_discard_count

u16 ext_count
```

`state_seq` монотонно растёт: `1` - после первого действия и т.д.
Клиент игнорирует пакеты с `state_seq` меньше уже применённого.

`extensions` - контейнер `key -> value` для будущих правил (эффекты карт,
щиты, дебаффы). Сервер заполнит его, когда появятся правила. Клиент,
не знающий про поле, просто проигнорирует запись.

### Управление ходом

**Таймер хода.** 

На `Room` заведён
`boost::asio::steady_timer`. Когда матч инициализирован
(`initializeGame` -> `startTurnTimer`), сервер запускает
`scheduleNextTick`: `expires_after(1s)`, `async_wait` → `onTurnTick`.

В `onTurnTick`:

1. Если таймер отменён (`operation_aborted`) или игра не в `Playing` -
   `stopTurnTimer` и выход.
2. `--currentTurnTime_`.
3. Если `currentTurnTime_ == 0` -> `endTurn()` (автопереход).
   Иначе -> `broadcastStateSync()` (клиенты видят тик таймера).
4. `scheduleNextTick()` - следующий тик через 1 с.

**Ручное завершение хода.**

`Session::handleMessage` диспатчит `EndTurn` в
`currentRoom_->handleEndTurn(session)`.

`Room::handleEndTurn`:

1. Проверяет `hasMatchState_ && gamePhase_ == Playing`.
2. Определяет `callerId` по `hostSession_` / `guestSession_`. 
Если это не участник комнаты - выход.
3. Если `callerId != currentTurnPlayerId_` - пишет
   `EndTurn ignored: caller=... current=...` в лог и выходит.
4. Иначе вызывает `endTurn()`.

**`endTurn()`:**

1. `currentTurnPlayerId_` переключается на другого игрока.
2. `++turnNumber_`.
3. `currentTurnTime_ = turnTimeLimit_` (сброс на 30).
4. Новый текущий игрок пытается добрать одну карту с верха (`back()`)
   своей колоды в руку.
    - Лимит полученных карт: если
      `hand.size() + discard.size() >= kDeckSize` (по умолчанию `30`),
      добор пропускается, в лог пишется
      `Draw skipped: <playerId> at max received (<N> >= <kDeckSize>)`.
    - Иначе - добор: `deck.pop_back()` -> `hand.push_back(drawn)`; в
      лог пишется
      `Draw: <playerId> card=<id> hand=<n> deck=<m> received=<r>`,
      где `r = hand + discard` после добора.
5. `broadcastStateSync()` - оба клиента видят новый ход, нового текущего
   игрока, свежие `hand`/`deck` (включая добор или его пропуск) и новый
   таймер.

**Остановка таймера.** 

`stopTurnTimer` вызывается:

- из `onTurnTick`, если игра не в `Playing`;
- из `removePlayer`, когда `players_.empty()`;
- при закрытии комнаты (через `removePlayer`).

Отмена асинхронного `async_wait` через `turnTimer_.cancel()` -
колбэк срабатывает с `operation_aborted` и корректно выходит.

**Стартовая инициализация.** 

В `initializeGame`:

- `currentTurnTime_ = turnTimeLimit_` до `sendMatchStart` -
  чтобы в `MatchStart` уже был корректный `current_turn_time`;
- без `broadcastStateSync()` при инициализации - стартовое
  состояние идёт через `MatchStart`;
- после `sendMatchStart` обоим игрокам - `startTurnTimer()`.

### Инициализация матча

Полный flow от подключения до старта матча:

```
client A                        server                    client B
  |                               |                          |
  |-- TCP connect --------------->|                          |
  |                               |<------ TCP connect ------|
  |                               |                          |
  |-- CreateRoom("A") ----------->|                          |
  |<-- CreateRoomResp(roomId) ----|                          |
  |                               |                          |
  |                               |<-- JoinRoom(roomId,"B")- |
  |                               |--- JoinRoomResp(roomId)->|
  |                               |                          |
  |                               | (players.size() == 2)    |
  |                               | initializeGame()         |
  |                               |                          |
  |<-- MatchStart (0x90)----------|--- MatchStart (0x90) --->|
  |                               |                          |
  | (UI готов)                    |                          | (UI готов)
```

Ключевое:

1. **`MatchStart` - один раз** на игрока, при `room->playerCount() == 2`.
   Содержит всё, что нужно для стартового UI: рука, колода, атрибуты,
   `dice_roll`, `first_player_id`.
2. **`StateSync` на старте не отправляется.** Первый `StateSync` придёт после
   первого игрового действия, с `state_seq = 1`.
3. Сервер **не отправляет ACK** - клиент после `CreateRoomResponse` /
   `JoinRoomResponse` просто ждёт `MatchStart`.
4. Если `MatchStart` пришёл **до** того, как клиентский UI подписался
   на делегаты - catch-up через `HasMatchInit()` / `GetLastMatchInit()`.
   Аналогично для `StateSync` (`HasStateSync()` / `GetLastStateSync()`)
   - но `StateSync` пока приходит только с actions, так что catch-up
   понадобится, когда actions появятся.
5. **При отключении одного игрока** комната закрывается целиком:
   второй получает `Error("Opponent disconnected")`, его сокет
   закрывается, таймер отменяется (`stopTurnTimer`). 
На клиенте срабатывает `OnDisconnected`, UI очищается.

### Примеры обмена

**Создание комнаты:**

```
-> [u16 8][u8 0x01][u16 5]["Alice"]
  hex: 08 00 01 05 00 41 6C 69 63 65

<- [u16 12][u8 0x81][u16 9]["ROOM-1234"]
  hex: 0C 00 81 09 00 52 4F 4F 4D 2D 31 32 33 34
```

**Присоединение к комнате:**

```
-> [u16 23][u8 0x02][u16 9]["ROOM-1234"][u16 3]["Bob"]
<- [u16 12][u8 0x82][u16 9]["ROOM-1234"]
```

**Список комнат (пустой):**

```
-> [u16 3][u8 0x03]
<- [u16 3][u8 0x83][u16 0]
  hex: 03 00 83 00 00
```

**Список комнат (одна комната):**

```
<- [u16 25][u8 0x83][u16 1][u16 9]["ROOM-1234"][u16 9]["127.0.0.1"][u8 1]
```

**Завершение хода (текущим игроком):**

```
-> [u16 3][u8 0x04]
  hex: 03 00 04

<- [u16 ...][u8 0x91][...]
```
`StateSync` обоим игрокам, `current_turn_player` сменился,
`current_turn_time = 30`

**Завершение хода (не текущим игроком):**

```
-> [u16 3][u8 0x04]
```
`StateSync` не рассылается; в логе сервера: `EndTurn ignored: caller = P2 current = P1`

**Тик таймера (раз в секунду):**

```
<- [u16 ...][u8 0x91][...]
```
`StateSync` обоим игрокам, `current_turn_time` уменьшился на 1,
`state_seq++`

### Примеры формата пакетов сервера при MatchStart, StateSync, EndTurn

Контекст для примеров:

- `room_id = "ROOM-1234"`, `registry_version = 0x41DF532A`, `wire_version = 2`
- игрок `P1 = "Alice"`, 
рука `5` карт (`0x0001..0x0005`), 
колода `3` (`0x0006..0x0008`), 
атрибуты `hp=15/15`, `mana=10/10`, `def=10/10`
- противник `P2 = "Bob"`, 
рука `5` (`0x0011..0x0015`), 
колода `3` (`0x0021..0x0023`), 
те же атрибуты
- `turn_time_limit = 30 (0x1E)`

#### 1. `MatchStart (0x90)` - при инициализации матча

Сервер отправляет этот пакет **один раз каждому игроку** сразу после `players_.size() == 2`. 
`Alice` видит себя как `your_*`, а `Bob-а` как `opp_*`. 
Внутри - `current_turn_time = 30` и `turn_time_limit = 30` в самом конце payload.

Полный пакет для `Alice` (171 байт):

```
AB 00                                                    ; length = 0xAB = 171 (opcode + payload)
90                                                       ; opcode = 0x90 (MatchStart)

; --- payload (170 байт) ---
02 00 00 00                                              ; wire_version = 2
2A 53 DF 41                                              ; registry_version = 0x41DF532A (LE)
09 00 52 4F 4F 4D 2D 31 32 33 34                         ; room_id = "ROOM-1234"
02 00 50 31                                              ; your_player_id = "P1"
05 00 41 6C 69 63 65                                     ; your_player_name = "Alice"
03 00                                                    ; your_attr_count = 3
06 00 68 65 61 6C 74 68 0F 00 00 00 0F 00 00 00          ;   health  15/15
04 00 6D 61 6E 61 0A 00 00 00 0A 00 00 00                ;   mana    10/10
07 00 64 65 66 65 6E 63 65 0A 00 00 00 0A 00 00 00       ;   defence 10/10
05 00                                                    ; your_hand_count = 5
01 00 02 00 03 00 04 00 05 00                            ;   hand ids 0x01..0x05
03 00                                                    ; your_deck_count = 3
06 00 07 00 08 00                                        ;   deck ids 0x06..0x08
05 00                                                    ; your_hand+discard = 5
02 00 50 32                                              ; opponent_player_id = "P2"
03 00 42 6F 62                                           ; opponent_player_name = "Bob"
03 00                                                    ; opp_attr_count = 3
06 00 68 65 61 6C 74 68 0F 00 00 00 0F 00 00 00          ;   health  15/15
04 00 6D 61 6E 61 0A 00 00 00 0A 00 00 00                ;   mana    10/10
07 00 64 65 66 65 6E 63 65 0A 00 00 00 0A 00 00 00       ;   defence 10/10
05 00                                                    ; opp_hand_count = 5
05 00                                                    ; opp_hand+discard = 5
02 00 50 31                                              ; first_player_id = "P1"
05                                                       ; dice_roll = 5
1E                                                       ; current_turn_time = 30
1E                                                       ; turn_time_limit  = 30
```

Для `Bob` - те же байты, только `your_*` = `"P2"/"Bob"/...`, 
`opp_*` = `"P1"/"Alice"/...`. `current_turn_time`/`turn_time_limit` тоже `1E 1E`.

**Важно:**

- Последние **2 байта payload** - это `current_turn_time` и `turn_time_limit`. 
На старте они всегда равны (`1E 1E` = 30 и 30).
- `first_player_id` + `dice_roll` определяют, чей ход первый. 
Если выиграл бросок `P1`, то в `StateSync` `current_turn_player_id` будет `"P1"` до первого `EndTurn` / автоперехода.
Длина пакета зависит от размера руки/колоды (по 2 байта на карту).
- Id карт противника не передаются.

#### 2. Тик таймера - `StateSync (0x91)` каждую секунду

Первый тик приходит ровно через `1 с` после `startTurnTimer()` в `initializeGame`. 
Payload - полный snapshot (в нашем примере 198 байт, пакет 199).

##### Тик #1: `state_seq = 1`, `current_turn_time = 29`

```
BB 00                                                    ; length = 0xBB = 187 (opcode + payload)
91                                                       ; opcode = 0x91 (StateSync)

; --- payload (186 байт) ---
02 00 00 00                                              ; wire_version = 2
2A 53 DF 41                                              ; registry_version
01 00 00 00                                              ; state_seq = 1
09 00 52 4F 4F 4D 2D 31 32 33 34                         ; room_id = "ROOM-1234"
01                                                       ; game_phase = Playing
01                                                       ; turn_number = 1
02 00 50 31                                              ; current_turn_player_id = "P1"
05                                                       ; dice_roll = 5
00 00                                                    ; winner_player_id = "" (пусто)
1D                                                       ; current_turn_time = 29
1E                                                       ; turn_time_limit  = 30

; --- me (84 байта) ---
02 00 50 31                                              ; your_player_id  = "P1"
05 00 41 6C 69 63 65                                     ; your_player_name = "Alice"
03 00                                                    ; your_attr_count
06 00 68 65 61 6C 74 68 0F 00 00 00 0F 00 00 00
04 00 6D 61 6E 61 0A 00 00 00 0A 00 00 00
07 00 64 65 66 65 6E 63 65 0A 00 00 00 0A 00 00 00
05 00                                                    ; your_hand_count = 5
01 00 02 00 03 00 04 00 05 00                            ;   hand ids
03 00                                                    ; your_deck_count = 3
06 00 07 00 08 00                                        ;   deck ids
00 00                                                    ; your_discard_count = 0
05 00                                                    ; your_hand+discard = 5

; --- opp (66 байт) ---
02 00 50 32                                              ; opp_player_id = "P2"
03 00 42 6F 62                                           ; opp_player_name = "Bob"
03 00                                                    ; opp_attr_count
06 00 68 65 61 6C 74 68 0F 00 00 00 0F 00 00 00
04 00 6D 61 6E 61 0A 00 00 00 0A 00 00 00
07 00 64 65 66 65 6E 63 65 0A 00 00 00 0A 00 00 00
05 00                                                    ; opp_hand_count = 5
03 00                                                    ; opp_deck_count = 3
00 00                                                    ; opp_discard_count = 0
05 00                                                    ; opp_hand+discard = 5

00 00                                                    ; ext_count = 0                                                    ; ext_count = 0
```

##### Тик #2: `state_seq = 2`, `current_turn_time = 28`

Полный пакет **побайтово идентичен** тику #1, кроме двух байт:

```
01 00 00 00   ->  02 00 00 00     ; state_seq   1 -> 2
1D            ->  1C              ; cur_time   29 -> 28
```

Всё остальное (рука, колода, атрибуты, `current_turn_player_id`, `turn_number`) - то же самое. 

##### Тик #3 … #N

Аналогично, пока `current_turn_time` не дойдёт до `1`. Тик #30 (если никто не нажимал `EndTurn`):

```
state_seq = 30
current_turn_time = 00
```

И сразу после него сервер вызовет `endTurn()` -> следующий `StateSync` 
уже с `current_turn_player_id = "P2"`, `turn_number = 2`, `current_turn_time = 30 (1E)`. 
То есть клиент увидит:

```
Тик 29: StateSync (seq=29) -> cur_time=1,  cur_turn="P1", turn=1
Тик 30: StateSync (seq=30) -> cur_time=30, cur_turn="P2", turn=2
Тик 31: StateSync (seq=31) -> cur_time=29, cur_turn="P2", turn=2
```

#### 3. `EndTurn (0x04)` от клиента и ответный `StateSync`

Клиент `P1` (текущий игрок) шлёт пустой `EndTurn`:

```
03 00                                                    ; length = 3
04                                                       ; opcode = 0x04 (EndTurn)
                                                         ; payload пустой
```

Прямого ответа нет - сервер рассылает `StateSync` обоим. Перед рассылкой
`endTurn()` переключает ход и даёт новому текущему игроку добрать карту с
верха колоды.

Так как payload собирается per-player, сервер отправляет два разных
пакета (`your_*` и `opp_*` меняются местами). Ниже - что изменилось
относительно тика #2 (`state_seq = 2`, `cur_time = 28`, `cur_turn = "P1"`,
`turn = 1`, руки 5/5, колоды 3/3). Всё, что не указано, - без изменений.

**Для `P1` (Alice - свой ход закончился, Bob добрал):**

```
state_seq              -> 03 00 00 00
current_turn_player_id -> 02 00 50 32 ("P1" -> "P2")
turn_number            -> 02
current_turn_time      -> 1E (28 -> 30)
turn_time_limit        -> 1E (без изменений)

opp_hand_count         -> 06 00 (5 -> 6)
opp_hand+discard       -> 06 00 (5 -> 6)
opp_deck_count         -> 02 00 (3 -> 2)
```

`P1` не добирал - блок `your_*` без изменений. В блоке `opp_*` у `P2`
рука 5 -> 6, колода 3 -> 2. 

В логе сервера: `Draw: P2 card=35 hand=6 deck=2 received=6`.

**Для `P2` (Bob - начал ход, добрал карту):**

```
state_seq              -> 03 00 00 00
current_turn_player_id -> 02 00 50 32 ("P1" -> "P2")
turn_number            -> 02
current_turn_time      -> 1E (28 -> 30)
turn_time_limit        -> 1E (без изменений)

your_hand_count        -> 06 00 (5 -> 6)
your_hand+discard      -> 06 00 (5 -> 6)
your_deck_count        -> 02 00 (3 -> 2)
```

`P2` добрал - в блоке `your_*` рука 5 -> 6, колода
3 -> 2. В блоке `opp_*` у `P1` без изменений.

`current_turn_player_id` теперь `"P2"`, поэтому у `Bob` в UI «мой ход», у
`Alice` - «ход врага»; у `Bob` `YourHandRowNames.Length` = 6 и
`CardCountPlayer` -> `6/30`, у `Alice` `OpponentHandCount` на `EnemyHud`
= 6.

Тот же пакет при автопереходе по таймауту - байты идентичны; разница
только в триггере (`onTurnTick` вместо `handleEndTurn`).

Если лимит сработал (`received == kDeckSize == 30`): у нового текущего
игрока `hand + discard == 30`, добор пропускается. `StateSync` всё равно
уходит обоим - меняются только `state_seq`, `current_turn_player_id`,
`turn_number`, `current_turn_time`; руки, колоды и `*_hand+discard` у
обоих игроков остаются как были. В логе сервера:
`Draw skipped: P2 at max received (30 >= 30)`. Клиент покажет
`CardCountPlayer` = `30/30`.

#### Если `EndTurn` шлёт не текущий игрок

Сервер ничего не отвечает (ни `StateSync`, ни `Error`). `endTurn()`
не вызывается, добора нет, `state_seq` не растёт.  

В логе:

```
[ROOM:ROOM-1234] EndTurn ignored: caller = P1 current = P2
```

### Почему бинарный формат

| Аспект | Текстовый протокол | Бинарный (текущий) |
|---|---|---|
| Границы пакета | Неявные, угадываются | Явные, `u16 length` в начале |
| Границы строки | `\0`-терминатор | `u16 length` перед байтами |
| Спецсимволы (`\0`, `;`, `,`) | Ломают протокол | Не влияют |
| Числа | ASCII-цифры (переменная длина) | Фиксированные `u8` / `u16` / `u32` |
| TCP-склейка / разрезание | Ломает парсинг | Обрабатывается корректно |
| Парсинг | Сканирование байт | Прямое чтение по длине |

**Экономия от id карт, а не названия:** карта - 2 байта (`u16 id`) вместо ~10–20
байт (`u16 len + UTF-8 rowName`). Для стартовой руки (5 карт) + колоды
(15 карт) экономия в `MatchStart` - около 120–240 байт на игрока.
В `StateSync` (рука + колода + discard каждого) - ещё больше.

---

## Конфигурация

Все настройки сервера - в файле `config.ini`.

### Формат файла

```ini
log_level = debug
port = 8080
```

| Ключ | Допустимые значения | По умолчанию | Назначение |
|---|---|---|---|
| `log_level` | `trace`, `debug`, `info`, `warn`, `error`, `fatal` | `debug` | Минимальный уровень логирования |
| `port` | 1–65535 | `8080` | TCP-порт сервера |

Незнакомые ключи игнорируются. Некорректный `port` - остаётся значение
по умолчанию.

### Расположение файла

| Способ запуска | Путь к `config.ini` |
|---|---|
| Windows (exe) | Рядом с `TcgServer.exe` - `build\Debug\config.ini` |
| Linux / macOS (бинарник) | Рядом с `TcgServer` или в текущей директории |
| Docker | Volume `tcgserver_tcg-config` → `/app/config/config.ini` |

Подробнее про правку в Docker - см. раздел [Docker](#docker).

### Уровни логирования

| Уровень | Что показывает | Когда использовать |
|---|---|---|
| `trace` | Сырые hex-дампы каждого read/write и payload | Отладка протокола |
| `debug` | Опкоды recv/send + размеры пакетов | Отладка логики (по умолчанию) |
| `info` | Подключения, создание/удаление комнат, вход игроков | Разработка и прод |
| `warn` | Некорректные данные от клиента | - |
| `error` | Ошибки сокета, write/read failed | - |
| `fatal` | Необработанное исключение | - |

Пример логов на уровне `trace`:

```
2026-09-24 13:46:27.833 [TRACE] [T:8800] [SESSION:0001] [127.0.0.1:62384] << recv 3 bytes: 01 00 03
2026-09-24 13:46:27.833 [DEBUG] [T:8800] [SESSION:0001] [127.0.0.1:62384] << recv opcode=0x03 (ListRooms) payload=0B
2026-09-24 13:46:27.834 [INFO ] [T:8800] [SESSION:0001] [127.0.0.1:62384] ListRooms: returning 0 available room(s), total=0
2026-09-24 13:46:27.834 [INFO ] [T:8800] [SESSION:0001] [127.0.0.1:62384] >> send opcode=0x83 (ListRoomsResponse) payload=2B total=5B
2026-09-24 13:46:27.834 [TRACE] [T:8800] [SESSION:0001] [127.0.0.1:62384] >> wire hex: 03 00 83 00 00
```

Формат строки:

```
YYYY-MM-DD HH:MM:SS.mmm [LEVEL] [T:XXXX] [COMPONENT] [PEER] message
```

| Поле | Описание |
|---|---|
| `YYYY-MM-DD HH:MM:SS.mmm` | Дата и время с миллисекундами |
| `[LEVEL]` | Уровень: `TRACE` / `DEBUG` / `INFO ` / `WARN ` / `ERROR` / `FATAL` |
| `[T:XXXX]` | ID потока (hex) |
| `[COMPONENT]` | `MAIN`, `SERVER` или `SESSION:XXXX` |
| `[PEER]` | IP:порт клиента (только для `SESSION`) |
| `message` | Текст события |

### Приоритет источников

| Приоритет | Источник | Применение |
|---|---|---|
| 1 (высший) | Переменная окружения `TCG_LOG_LEVEL` | CI, Docker, быстрый override |
| 2 | `config.ini` | Основной источник для разработки и прода |
| 3 (низший) | Дефолты в коде (`debug`, порт `8080`) | Если файла нет и ENV не задана |

Пример override через ENV без правки файла:

```powershell
$env:TCG_LOG_LEVEL = "trace"
.\TcgServer.exe
```

После этого в первой строке лога будет:

```
[MAIN] Log level: TRACE (from ENV)
```

Если ENV не задана - берётся значение из `config.ini`, и в логе будет:

```
[MAIN] Log level: DEBUG (from config)
```

### Проверка загруженной конфигурации

При старте сервер пишет три строки - по ним видно, что и откуда пришло:

```
2026-09-24 13:24:08.123 [INFO ] [T:1A7C] [MAIN] Config loaded from: ...\config.ini
2026-09-24 13:24:08.123 [INFO ] [T:1A7C] [MAIN] Log level: DEBUG (from config)
2026-09-24 13:24:08.123 [INFO ] [T:1A7C] [MAIN] Port: 8080
```

Если файл не найден:

```
2026-09-24 13:24:08.123 [WARN ] [T:1A7C] [MAIN] config.ini not found at '...\config.ini', using defaults
```