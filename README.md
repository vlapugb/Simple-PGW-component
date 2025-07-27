# 📖 Инструкция по запуску проекта

## 1. Подготовка окружения

1. Скопируйте **файлы конфигурации** в каталог `settings/` **до** сборки проекта.
2. **Названия файлов должны быть строго следующими**:

   * `client_settings.json`
   * `server_settings.json`
3. Убедитесь, что содержимое конфигураций соответствует требуемому формату (см. пример в `docs/`).

---

## 2. Сборка проекта

Из корневого каталога выполните скрипт:

```bash
source configure_and_run.sh
```

При успешной сборке в консоли появится отчёт о пройденных unit‑тестах.

---

## 3. Запуск приложения

### 3.1 Переход в каталог сборки

```bash
cd build/src
```

### 3.2 Запуск сер
set(RUNNABLE ${CMAKE_PROJECT_NAME})
set(OBJ_LIB "${CMAKE_PROJECT_NAME}_lib")
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
add_library(${OBJ_LIB} OBJECT ${SOURCES})

target_include_directories(${OBJ_LIB} SYSTEM PUBLIC ${boost_asio_SOURCE_DIR}/include)
target_include_directories(${OBJ_LIB} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_compile_options(${OBJ_LIB} PUBLIC "-Werror" "-Wall" "-Wextra" "-Wpedantic" "-Wno-error=maybe-uninitialized")вера

```bash
./server
```

### 3.3 Запуск клиента

Выберите **один** из вариантов:

| Команда               | Описание                                                  |
| --------------------- | --------------------------------------------------------- |
| `./client -n <ЧИСЛО>` | Создаёт `<ЧИСЛО>` абонентских сессий                      |
| `./client <IMSI>`     | Создаёт одну сессию с указанным **IMSI**                  |
| `./client`            | Создаёт одну сессию с **рандомным IMSI** оператора Билайн |

---

## 4. HTTP‑API сервера

Взаимодействие с запущенным **crow‑server** через `curl`:

### 4.1 Остановка сервера

```bash
curl "http://<IP>:<PORT>/stop"
```

### 4.2 Проверка сессии абонента

```bash
curl "http://<IP>:<PORT>/check_subscriber?imsi=<ВАШ_IMSI>"
```

Где `<IP>` и `<PORT>` — адрес и порт сервера, а `<ВАШ_IMSI>` — IMSI абонента.

---

## 5. Просмотр логов

* Все логи сохраняются в каталоге `logs/`
* Имена файлов логов совпадают с именами, заданными в соответствующих конфигурационных файлах в `settings/`

## P.S.
* Проект билдится очень долго, минут 15 из-за того, что делается fetch_content boost, 

