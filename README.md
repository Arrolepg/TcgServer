# TcgServer

Сервер для карточной игры (TCG), написанный на C++17 с использованием Boost.Asio.
Сервер принимает TCP-подключения, позволяет создавать игровые комнаты, присоединяться к ним
и получать список доступных комнат.

---

## Содержание

- [Возможности](#возможности)
- [Требования](#требования)
- [Установка зависимостей](#установка-зависимостей)
- [Сборка проекта](#сборка-проекта)
- [Запуск](#запуск)
- [Сетевой протокол](#сетевой-протокол)

---

## Возможности

- Приём TCP-подключений (асинхронный ввод-вывод через Boost.Asio).
- Создание игровых комнат с уникальным ID.
- Присоединение игроков к существующим комнатам (максимум 2 игрока на комнату).
- Получение списка доступных комнат.
- Автоматическое удаление пустых комнат.
- Логирование всех действий сервера в консоль с временными метками.
- Кроссплатформенный код (Windows / Linux / macOS).

---

## Требования

| Компонент | Минимальная версия | Назначение |
|---|---|---|
| **C++ компилятор** | C++17 | MSVC 2019+, GCC 9+, Clang 10+ |
| **CMake** | 3.15 | Система сборки |
| **Boost.Asio** | 1.70+ | Асинхронный сетевой ввод-вывод |
| **vcpkg** (рекомендуется) | последняя | Менеджер пакетов для Boost |
| **Visual Studio** (Windows) | 2019 / 2022 | Компилятор MSVC + MSBuild |
| **Git** | любая | Клонирование репозитория |

> **Примечание.** Boost.Asio - header-only библиотека, но для работы под Windows
> требуется линковка с системными библиотеками `ws2_32` и `mswsock`.

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
[yyyy-MM-dd HH:mm:ss] Server listening on port 8080
```

Сервер готов принимать подключения на порту **8080**.

### Смена порта

Порт задаётся в `src/main.cpp`:

```cpp
Server server(io, 8080);
```

Измените значение и пересоберите проект:

**Windows:**
```powershell
cmake --build build --config Debug
```

**Linux / macOS:**
```bash
cmake --build build
```

---

### Краткое описание модулей

- **Server** - асинхронно принимает TCP-подключения, создаёт `Session` на каждого клиента.
- **Session** - обрабатывает входящие пакеты, хранит состояние игрока (имя, текущую комнату).
- **Room** - модель игровой комнаты, хранит игроков и IP хоста.
- **Protocol** - опкоды, парсинг нуль-терминированных строк, сборка ответов.
- **Logger** - единая точка логирования.
- **Utils** - преобразование ANSI-строк в UTF-8, генерация ID комнат.

---

## Сетевой протокол

### Формат пакета

**Запрос от клиента:**

```
[1 байт: opcode] [строка1\0] [строка2\0] ...
```

**Ответ сервера:**

```
[1 байт: opcode] [payload, оканчивающийся \0]
```

Строки внутри payload - нуль-терминированные (`\0`). Если в пакете несколько строк
подряд, они просто идут друг за другом, каждая заканчивается `\0`.

Сервер добавляет `\0` в конец **всегда** - это делает функция `buildResponse`.
Даже если payload пустой, пакет будет `[opcode][\0]`.

### Опкоды клиента

| Код | Название | Payload | Ответ сервера |
|---|---|---|---|
| `0x01` | `CreateRoom` | `name\0` | `0x81` + `roomId\0` |
| `0x02` | `JoinRoom` | `roomId\0name\0` | `0x82` + `roomId\0` |
| `0x03` | `ListRooms` | - | `0x83` + список комнат |

### Опкоды сервера

| Код | Название | Payload | Описание |
|---|---|---|---|
| `0x80` | `Error` | `message\0` | Ошибка (комната не найдена, полна и т.д.) |
| `0x81` | `CreateRoomResponse` | `roomId\0` | Успешно созданная комната |
| `0x82` | `JoinRoomResponse` | `roomId\0` | Успешное присоединение |
| `0x83` | `ListRoomsResponse` | `room1,ip,count;room2,ip,count\0` | Список комнат |

> **Примечание.** Если в `ListRoomsResponse` нет ни одной комнаты, payload пустой,
> и клиент получает пакет `[0x83][\0]` без данных.

### Пример обмена

**Создание комнаты:**

```
-> 0x01 "Alice\0"
<- 0x81 "ROOM-1234\0"
```

**Присоединение к комнате:**

```
-> 0x02 "ROOM-1234\0" "Bob\0"
<- 0x82 "ROOM-1234\0"
```

**Список комнат:**

```
-> 0x03
<- 0x83 "ROOM-1234,192.168.1.5,1;ROOM-5678,10.0.0.2,2\0"
```

Формат описания комнаты в списке: `roomId,hostIp,playerCount`, разделитель между
комнатами - `;`. Финальный `\0` добавляет сервер (функция `buildResponse`).