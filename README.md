# TcgServer

Сервер для карточной игры (TCG) на C++17 с использованием Boost.Asio.
Принимает TCP-подключения, позволяет создавать игровые комнаты, присоединяться
к ним и получать список доступных комнат. При заполнении комнаты (2/2)
сервер инициализирует матч и отправляет клиентам полное состояние игры.

Протокол - бинарный, с length-prefixed фреймингом. Общение идёт байтами
с явными типами и длинами, без `\0`-терминаторов и текстовых разделителей.

---

## Содержание

- [Возможности](#возможности)
- [Требования](#требования)
- [Установка зависимостей](#установка-зависимостей)
- [Сборка проекта](#сборка-проекта)
- [Запуск](#запуск)
- [Docker](#docker)
- [Сетевой протокол](#сетевой-протокол)
   - [Версия wire-формата](#версия-wire-формата)
   - [Идентификация карт](#идентификация-карт)
   - [Старт матча](#старт-матча)
   - [Синхронизация состояний](#синхронизация-состояний)
   - [Инициализация матча](#инициализация-матча)
- [Конфигурация](#конфигурация)

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
- Синхронизация - полный snapshot состояния; сервер шлёт его
  сразу после старта матча и по запросу клиента.
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
- Готов к запуску в Docker (multi-stage build, distroless runtime).

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

### Windows + vcpkg (рекомендуется)

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

### Windows (PowerShell)

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

### Краткое описание модулей

- **Server** - асинхронно принимает TCP-подключения, создаёт `Session`
  на каждого клиента.
- **Session** - обрабатывает входящие пакеты, хранит состояние игрока
  (имя, комнату), ведёт аккумулятор входящих байт и парсит пакеты
  по length-prefix.
- **PlayerState** - снимок состояния одного игрока в комнате:
  `playerId` / `playerName`; `attributes` (массив `AttrValue` - max/current
  для `health` / `mana` / `defence`); `hand` / `deck` / `discard` как
  `std::vector<uint16_t>` (CardRegistry ids). Дефолты атрибутов
  (15 / 10 / 10) задаются в конструкторе `PlayerState::PlayerState()`.
- **Room** - модель игровой комнаты. Хранит игроков, IP хоста и **полное
  состояние матча** (`hostState_` / `guestState_`): hand, deck, discard,
  attributes, game phase, turn, winner. Сохраняется между отправками
  `MatchStart` / `StateSync`.
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

### Изменение config.ini в работающем контейнере

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

**Единый источник правды - `DT_Cards`** (DataTable в UE, UAsset).

**Сервер** не может читать UAsset, поэтому держит
свою таблицу `{id, rowName}`. Синхронизация с `DT_Cards` - ручная.

**Проверка консистентности.** В шапке `MatchStart` и `StateSync` сервер шлёт
`u32 registry_version` - FNV-1a хэш от `(id, rowName)` всей таблицы. Клиент
считает хэш от и сравнивает. При расхождении:

- матч продолжается;
- в лог клиента - `CardRegistry mismatch`;
- для неизвестных id клиент подставляет `<unknown>` вместо имени.

Это защита от типичной ошибки «добавил карту только на сервере».

**Требования к `rowName`** (единый ключ на сервере и клиенте; в `DT_Cards`
это `RowName`, в `server/CardRegistry.cpp` - поле `rowName`):

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

**Добавление новой карты:**

1. В `DT_Cards` добавить строку с новым уникальным `CardId`
   (следующий свободный, не переиспользовать старые).
2. В `CardRegistry.cpp` на сервере добавить `{N, "RowName"}`.
3. Исправить `CardRegistry.cpp` в плагине.
4. `version()` пересчитается автоматически при сборке.

**Лимит id.** `u16` -> максимум **65 535 карт**. Когда упрётесь - `u32`
и bump `kWireVersion` до 3.

### Опкоды клиента

| Код | Название | Payload |
|---|---|---|
| `0x01` | `CreateRoom` | `string playerName` |
| `0x02` | `JoinRoom` | `string roomId` + `string playerName` |
| `0x03` | `ListRooms` | - (пусто) |
| `0x04` | `RequestStateSync` | - (пусто); запрос полного `StateSync` |

### Опкоды сервера

| Код | Название | Payload |
|---|---|---|
| `0x80` | `Error` | `string message` |
| `0x81` | `CreateRoomResponse` | `string roomId` |
| `0x82` | `JoinRoomResponse` | `string roomId` |
| `0x83` | `ListRoomsResponse` | `u16 count` + `count × (string roomId, string hostIp, u8 playersCount)` |
| `0x90` | `MatchStart` | см. [MatchStart (0x90)](#matchstart-0x90) |
| `0x91` | `StateSync` | см. [StateSync (0x91)](#statesync-0x91) |

> **Примечание.** Если в `ListRoomsResponse` нет ни одной комнаты,
> `count = 0` и payload содержит только `u16 0x0000`.

### Старт матча

Сервер шлёт обоим игрокам **один раз** при заполнении комнаты (2/2).
Содержит всё, что нужно клиенту для входа в матч.

> **BP-контракт (UE):** формат соответствует `FST_MatchInitData`.

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
```

`your_attr_count = opp_attr_count = 3` - `health` / `mana` / `defence`.
Порядок и `rowName` фиксированы (`kAttrs` в `Room.cpp`).

### Синхронизация состояний

Полный snapshot состояния матча. Сервер шлёт:

- **сразу после `MatchStart`** в `initializeGame` (`state_seq = 1`);
- по запросу клиента `RequestStateSync (0x04)` - с тем же `state_seq`;

Клиент хранит последний `LastStateSync` для catch-up и рендерит UI.

> **BP-контракт (UE):** формат соответствует `FST_StateSyncData`.

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

`state_seq` монотонно растёт: `1` - старт, `2` - после первого действия и т.д.
Клиент может игнорировать пакеты с `state_seq` меньше уже применённого
(защита от переупорядочивания).

`extensions` - контейнер `key -> value` для будущих правил (эффекты карт,
щиты, дебаффы). Сервер заполнит его, когда появятся правила. Клиент,
не знающий про поле, просто проигнорирует запись.

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
  |<-- StateSync seq=1 -----------|--- StateSync seq=1 ----->|
  |                               |                          |
  | (UI готов)                    |                          | (UI готов)
```

Ключевое:

1. **`MatchStart` - один раз** на игрока, при `room->playerCount() == 2`.
2. **`StateSync seq=1` - сразу после `MatchStart`**, отдельным пакетом.
   Клиент обрабатывает оба независимо.
3. Сервер **не шлёт ACK** - клиент после `CreateRoomResponse` /
   `JoinRoomResponse` просто ждёт `MatchStart`.
4. Если `MatchStart` или `StateSync` пришёл **до** того, как клиентский UI
   подписался на делегаты - все нормально: серверная часть хранит
   `LastMatchInit` / `LastStateSync`, клиент делает catch-up через
   `HasMatchInit()` / `GetLastMatchInit()` и `HasStateSync()` /
   `GetLastStateSync()` (см. плагин UE).

#### RequestStateSync (0x04)

Клиент запрашивает полный снимок заново. Сервер отвечает **внеочередным
`StateSync`** с тем же `state_seq` (не инкрементит).

**Когда использовать:**

- После переподключения (клиент потерял TCP, переподключился - нужно
  восстановить UI, не дожидаясь action-опкода).
- После `CardRegistry mismatch` - клиент может перечитать DT и запросить
  снимок.
- Для дебага - вручную дёрнуть `UTcgNetworkSubsystem::RequestStateSync()`
  из BP.

**Ограничения:**

- Автоматического reconnect нет - клиент должен сам вызвать `Connect()` +
  `JoinRoom()` при разрыве.
- `RequestStateSync` работает **только в активной комнате**. Если клиент
  вышел из комнаты - сервер шлёт `Error("Not in a room")`.
- При reconnect клиент попадёт в **новую** комнату (старая уже удалена
  сервером после `removePlayer`), поэтому `state_seq` начнётся с 1.

### Пример обмена

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

### Почему бинарный формат

| Аспект | Текстовый протокол | Бинарный (текущий) |
|---|---|---|
| Границы пакета | Неявные, угадываются | Явные, `u16 length` в начале |
| Границы строки | `\0`-терминатор | `u16 length` перед байтами |
| Спецсимволы (`\0`, `;`, `,`) | Ломают протокол | Не влияют |
| Числа | ASCII-цифры (переменная длина) | Фиксированные `u8` / `u16` / `u32` |
| TCP-склейка / разрезание | Ломает парсинг | Обрабатывается корректно |
| Парсинг | Сканирование байт | Прямое чтение по длине |

**Экономия от v2:** карта - 2 байта (`u16 id`) вместо ~10–20
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

### Где лежит файл

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