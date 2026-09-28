# TcgServer

Сервер для карточной игры (TCG), написанный на C++17 с использованием Boost.Asio.
Сервер принимает TCP-подключения, позволяет создавать игровые комнаты, присоединяться к ним
и получать список доступных комнат.

**Протокол - бинарный, с length-prefixed фреймингом.** Общение идёт байтами с явными типами и длинами, 
без `\0`-терминаторов и текстовых разделителей. 

---

## Содержание

- [Возможности](#возможности)
- [Требования](#требования)
- [Установка зависимостей](#установка-зависимостей)
- [Сборка проекта](#сборка-проекта)
- [Запуск](#запуск)
- [Docker](#docker)
- [Сетевой протокол](#сетевой-протокол)
- [Конфигурация](#конфигурация)

---

## Возможности

- Приём TCP-подключений (асинхронный ввод-вывод через Boost.Asio).
- Создание игровых комнат с уникальным ID.
- Присоединение игроков к существующим комнатам (максимум 2 игрока на комнату).
- Получение списка доступных комнат.
- Автоматическое удаление пустых комнат.
- **Бинарный wire-формат** с length-prefix фреймингом.
- Логирование с уровнями (`TRACE`/`DEBUG`/`INFO`/`WARN`/`ERROR`/`FATAL`), миллисекундами, ID сессии, IP клиента и hex-дампами.
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

> **Примечание.** Boost.Asio - header-only библиотека. Под Windows требуется линковка
> с системными библиотеками `ws2_32` и `mswsock`. Под Linux - с `pthread`.

---

## Установка зависимостей

### Windows + vcpkg (рекомендуется)

1. **Установите Visual Studio 2022** с компонентом «Разработка классических приложений на C++».

   Скачать: <https://visualstudio.microsoft.com/>


2. **Установите CMake** (или используйте версию, идущую с VS).

   Скачать: <https://cmake.org/download/>


3. **Установите vcpkg.**

   Клонируйте репозиторий vcpkg в удобное место. В примерах ниже используется
   `C:\vcpkg`, но вы можете выбрать любой другой путь - главное, запомните его,
   он понадобится при сборке проекта.

   ```powershell
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   cd C:\vcpkg
   .\bootstrap-vcpkg.bat
   ```

4. **Установите Boost.Asio через vcpkg.**

   ```powershell
   .\vcpkg install boost-asio:x64-windows
   ```

   Этой команды достаточно. vcpkg автоматически подтянет транзитивные зависимости:
   `boost-system`, `boost-core`, `boost-config`, `boost-array` и другие.


5. **Проверьте установку.**

   ```powershell
   .\vcpkg list | Select-String "boost-"
   ```

   В списке должны присутствовать `boost-asio:x64-windows` и `boost-system:x64-windows`.

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

   Или откройте `build\TcgServer.sln` в Visual Studio / Rider и нажмите **Build**.
   Конфигурацию (Debug / Release) можно переключить в выпадающем списке вверху IDE.

### Linux

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
```

Если `cmake --build` недоступен в старой версии - используйте `make -j$(nproc)`.

### macOS

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
```

Если `cmake --build` недоступен - используйте `make -j$(sysctl -n hw.ncpu)`.

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

- **Server** - асинхронно принимает TCP-подключения, создаёт `Session` на каждого клиента.
- **Session** - обрабатывает входящие пакеты, хранит состояние игрока (имя, комнату),
  ведёт аккумулятор входящих байт и парсит пакеты по length-prefix.
- **Room** - модель игровой комнаты, хранит игроков и IP хоста.
- **Protocol** - бинарный `Writer`/`Reader` (u8/u16/u32/string), сборка и парсинг фреймов.
- **Logger** - уровни, компоненты, ID сессии, hex-дампы, потокобезопасность.
- **Utils** - преобразование ANSI -> UTF-8, генерация ID комнат.
- **Config** - парсинг `config.ini`.

---

## Docker

Сервер собирается и запускается в Docker. Используется **multi-stage build**:

- **Stage 1 (builder)** - `debian:bookworm-slim` с полным toolchain (GCC, CMake, Boost SDK).
- **Stage 2 (runtime)** - `gcr.io/distroless/cc-debian12:nonroot`, содержащий только бинарник и минимальные системные библиотеки (~25 МБ против ~1.5 ГБ).

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

1. Собирается образ `tcgserver:latest` (первая сборка ~2-5 минут).
2. Запускается контейнер `tcgserver` в фоне.
3. Порт `8080` пробрасывается на хост.
4. Создадются два named volume: `tcgserver_tcg-config` и `tcgserver_tcg-logs`.

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

После этого volume инициализируется тем `config.ini`, который запечён в образ (из корня проекта на момент сборки).

### Что внутри образа

| Слой | Содержимое |
|---|---|
| **builder (отбрасывается)** | GCC, CMake, git, Boost SDK, исходники |
| **runtime (финальный)** | `TcgServer`, `config.ini`, `glibc`, `libstdc++` |

Boost SDK **не попадает в runtime** - Asio header-only, весь нужный код уже в бинарнике при сборке.

---

## Сетевой протокол

Протокол - **бинарный, length-prefixed**.

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

### Опкоды клиента

| Код | Название | Payload |
|---|---|---|
| `0x01` | `CreateRoom` | `string playerName` |
| `0x02` | `JoinRoom` | `string roomId` + `string playerName` |
| `0x03` | `ListRooms` | - (пусто) |

### Опкоды сервера

| Код | Название | Payload |
|---|---|---|
| `0x80` | `Error` | `string message` |
| `0x81` | `CreateRoomResponse` | `string roomId` |
| `0x82` | `JoinRoomResponse` | `string roomId` |
| `0x83` | `ListRoomsResponse` | `u16 count` + `count × (string roomId, string hostIp, u8 playersCount)` |

> **Примечание.** Если в `ListRoomsResponse` нет ни одной комнаты, `count = 0`
> и payload содержит только `u16 0x0000`.

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
| Числа | ASCII-цифры (переменная длина) | Фиксированные `u8`/`u16`/`u32` |
| TCP-склейка/разрезание | Ломает парсинг | Обрабатывается корректно |
| Парсинг | Сканирование байт | Прямое чтение по длине |

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

Незнакомые ключи игнорируются. Некорректный `port` - остаётся значение по умолчанию.

### Где лежит файл

| Способ запуска | Путь к `config.ini` |
|---|---|
| Windows (exe) | Рядом с `TcgServer.exe` - `build\Debug\config.ini` |
| Linux / macOS (бинарник) | Рядом с `TcgServer` или в текущей директории |
| Docker | Volume `tcgserver_tcg-config` -> `/app/config/config.ini` |

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

| Приоритет | Источник | Примененние |
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