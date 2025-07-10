# RCKangaroo Distributed System

Клиент-серверная версия RCKangaroo для распределенного решения ECDLP (Elliptic Curve Discrete Logarithm Problem).

## Архитектура

Система состоит из двух компонентов:

1. **Сервер** (Python) - координирует работу, хранит Distinguished Points (DP), распределяет задачи
2. **Клиент** (C++/CUDA) - выполняет GPU вычисления, отправляет DP на сервер

### Преимущества распределенной системы

- **Централизованная база DP**: все клиенты используют общую базу точек
- **Последовательный поиск**: исключает дублирование работы
- **Масштабируемость**: легко добавлять новые GPU-машины
- **Отказоустойчивость**: сервер отслеживает статус задач
- **Координация**: автоматическое уведомление всех клиентов о найденном решении

## Быстрый старт

### 1. Запуск сервера

```bash
cd server
pip install -r requirements.txt
python kangaroo_server.py --host 0.0.0.0 --port 8080
```

Сервер будет доступен по адресу: `http://your-server:8080`

### 2. Настройка поиска

```bash
# Настроить поиск для puzzle #85
python kangaroo_server.py --configure \
  0x1000000000000000000000 \
  0x1FFFFFFFFFFFFFFFFFFFFF \
  0329c4574a4fd8c810b7e42a4b398882b381bcd85e40c6883712912d167c83e73a \
  16 \
  0x1000000000000000
```

### 3. Запуск клиентов

```bash
cd client
make deps  # Проверить зависимости
make       # Собрать клиент

# Запустить клиент
./RCKangarooClient -server http://server-ip:8080 -clientid client-001
```

## Детальная инструкция

### Установка сервера

#### Требования
- Python 3.7+
- Flask
- SQLite3

#### Установка
```bash
cd server
pip install -r requirements.txt
```

#### Запуск сервера
```bash
python kangaroo_server.py [опции]

Опции:
  --host HOST     IP адрес сервера (по умолчанию: 0.0.0.0)
  --port PORT     Порт сервера (по умолчанию: 8080)
  --db PATH       Путь к базе данных (по умолчанию: kangaroo_server.db)
```

### Установка клиента

#### Для Linux

##### Требования
- CUDA Toolkit 11.x+
- g++
- libjsoncpp-dev
- libcurl4-openssl-dev

##### Установка зависимостей (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install build-essential
sudo apt-get install libjsoncpp-dev libcurl4-openssl-dev
```

##### Сборка клиента
```bash
cd client
make deps   # Проверить зависимости
make        # Собрать
```

#### Для Windows

##### Требования
- Windows 10/11 x64
- Visual Studio 2019 или 2022 (Community/Professional/Enterprise)
- CUDA Toolkit 11.8+ (рекомендуется 12.x)
- Git для Windows

##### Автоматическая установка (рекомендуется)

1. **Запустите PowerShell или CMD от имени администратора**

2. **Скачайте и установите зависимости:**
```cmd
cd client
setup_windows.bat
```

Этот скрипт автоматически:
- Проверит наличие Visual Studio и CUDA
- Установит vcpkg менеджер пакетов
- Скачает и скомпилирует libcurl и jsoncpp
- Настроит переменные окружения

3. **Соберите проект:**
```cmd
build_windows.bat
```

Или откройте `RCKangarooClient.sln` в Visual Studio и соберите вручную.

##### Ручная установка

1. **Установите Visual Studio 2019/2022**
   - Скачайте с https://visualstudio.microsoft.com/
   - Выберите "Desktop development with C++" workload
   - Убедитесь, что установлены MSVC compiler и Windows SDK

2. **Установите CUDA Toolkit**
   ```
   Скачайте с: https://developer.nvidia.com/cuda-downloads
   Выберите: Windows → x86_64 → 10/11 → exe (local)
   ```

3. **Установите vcpkg (менеджер пакетов)**
   ```cmd
   cd C:\
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   bootstrap-vcpkg.bat
   vcpkg integrate install
   ```

4. **Установите зависимости через vcpkg**
   ```cmd
   vcpkg install curl:x64-windows
   vcpkg install jsoncpp:x64-windows
   ```

5. **Настройте переменные окружения**
   ```cmd
   setx VCPKG_ROOT "C:\vcpkg" /M
   setx CUDA_PATH "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6" /M
   ```

6. **Соберите проект**
   - Откройте `client\RCKangarooClient.sln` в Visual Studio
   - Выберите конфигурацию "Release" и платформу "x64"
   - Build → Build Solution (Ctrl+Shift+B)

   Или из командной строки:
   ```cmd
   cd client
   build_windows.bat
   ```

##### Файлы проекта Windows
- `RCKangarooClient.sln` - Solution файл
- `RCKangarooClient.vcxproj` - Файл проекта
- `setup_windows.bat` - Автоматическая установка зависимостей
- `build_windows.bat` - Автоматическая сборка проекта

##### Результат сборки
```
client\x64\Release\RCKangarooClient.exe  - Исполняемый файл
```

##### Тестирование Windows сборки
```cmd
cd client\x64\Release
RCKangarooClient.exe -server http://localhost:8080
```

### Настройка поиска

Перед запуском клиентов необходимо настроить сервер:

```bash
# Формат: start_range end_range pubkey dp_bits range_size
./RCKangarooClient -server http://server:8080 -configure \
  START_HEX END_HEX PUBKEY_HEX DP_BITS RANGE_SIZE_HEX
```

#### Пример для Bitcoin Puzzle #85
```bash
./RCKangarooClient -server http://server:8080 -configure \
  0x1000000000000000000000 \
  0x1FFFFFFFFFFFFFFFFFFFFF \
  0329c4574a4fd8c810b7e42a4b398882b381bcd85e40c6883712912d167c83e73a \
  16 \
  0x100000000000000
```

Параметры:
- `start_range`: Начало диапазона поиска
- `end_range`: Конец диапазона поиска  
- `pubkey`: Публичный ключ для поиска
- `dp_bits`: Количество битов для Distinguished Points (14-60)
- `range_size`: Размер подзадачи для одного клиента

### Запуск клиентов

#### Базовый запуск
```bash
./RCKangarooClient -server http://server:8080
```

#### С настройками
```bash
./RCKangarooClient -server http://server:8080 \
  -clientid my-gpu-farm-01 \
  -gpu 01234567  # Использовать GPU 0,1,2,3,4,5,6,7
```

#### Опции клиента
- `-server URL`: URL сервера
- `-clientid ID`: Уникальный ID клиента (по умолчанию: hostname_timestamp)
- `-gpu LIST`: Список GPU для использования (например, "0123")

## Мониторинг

### Web интерфейс сервера

Откройте `http://server:8080` в браузере для просмотра:
- Статус поиска (найдено решение или нет)
- Количество обработанных Distinguished Points
- Статистика рабочих диапазонов
- Настройки поиска

### API сервера

#### GET /api/status
```json
{
  "solved": false,
  "solution": null,
  "dp_count": 15423,
  "work_ranges": {
    "pending": 847,
    "assigned": 12,
    "completed": 0
  },
  "search_range": {
    "start": "0x1000000000000000000000",
    "end": "0x1FFFFFFFFFFFFFFFFFFFFF",
    "pubkey": "0329c4574a4fd8c810b7e42a4b398882b381bcd85e40c6883712912d167c83e73a",
    "dp_bits": 16
  }
}
```

## Примеры использования

### 1. Простой поиск (тест)

#### Linux
```bash
# Сервер
python kangaroo_server.py

# Настройка (небольшой диапазон для теста)
./RCKangarooClient -server http://localhost:8080 -configure \
  0x200000000000000000 0x400000000000000000 \
  0290e6900a58d33393bc1097b5aed31f2e4e7cbd3e5466af958665bc0121248483 \
  14 0x10000000000000

# Клиент
./RCKangarooClient -server http://localhost:8080
```

#### Windows
```cmd
# Сервер (в отдельном CMD окне)
cd server
python kangaroo_server.py

# Настройка (в другом CMD окне)
cd client\x64\Release
RCKangarooClient.exe -server http://localhost:8080 -configure ^
  0x200000000000000000 0x400000000000000000 ^
  0290e6900a58d33393bc1097b5aed31f2e4e7cbd3e5466af958665bc0121248483 ^
  14 0x10000000000000

# Клиент
RCKangarooClient.exe -server http://localhost:8080
```

### 2. Крупномасштабный поиск

```bash
# Настройка для puzzle #130
./RCKangarooClient -server http://server:8080 -configure \
  0x200000000000000000000000000000000 \
  0x3FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \
  0302dbc2596e2294226137968fc4be5a6a9e01768b3a5ee48642e2bd4b2e02d9efe \
  20 \
  0x1000000000000000000000000

# Запуск множества клиентов
for i in {1..10}; do
  ssh gpu-node-$i "cd /path/to/client && ./RCKangarooClient -server http://server:8080 -clientid node-$i"
done
```

### 3. Мониторинг через curl

```bash
# Статус
curl http://server:8080/api/status

# Проверка решения
while true; do
  status=$(curl -s http://server:8080/api/status | jq -r '.solved')
  if [ "$status" = "true" ]; then
    echo "РЕШЕНИЕ НАЙДЕНО!"
    curl -s http://server:8080/api/status | jq -r '.solution'
    break
  fi
  sleep 60
done
```

## Рекомендации по настройке

### Выбор DP битов
- **14-16 бит**: Для небольших диапазонов (<70 бит)
- **16-18 бит**: Для средних диапазонов (70-90 бит)  
- **18-22 бит**: Для больших диапазонов (>90 бит)

Меньшие значения = больше точек в базе, но меньше накладных расходов на GPU.

### Размер подзадач
- **Малые GPU (<8GB)**: 0x1000000000000 - 0x10000000000000
- **Средние GPU (8-16GB)**: 0x10000000000000 - 0x100000000000000  
- **Мощные GPU (>16GB)**: 0x100000000000000 - 0x1000000000000000

### Сетевые настройки
- Клиенты отправляют DP каждые 30 секунд
- Размер пакета DP обычно 100-1000 точек
- Трафик: ~1-10 KB/s на клиент

## Устранение неполадок

### Сервер не запускается

#### Linux
```bash
# Проверить порт
netstat -tulpn | grep 8080

# Права на базу данных
ls -la kangaroo_server.db
chmod 666 kangaroo_server.db
```

#### Windows
```cmd
# Проверить порт
netstat -an | findstr 8080

# Проверить файрвол Windows
netsh advfirewall firewall show rule name="Python"
```

### Клиент не подключается

#### Linux
```bash
# Проверить доступность сервера
curl http://server:8080/api/status

# Проверить файрвол
telnet server 8080
```

#### Windows
```cmd
# Проверить доступность сервера
curl http://server:8080/api/status

# Или через PowerShell
Invoke-WebRequest -Uri http://server:8080/api/status

# Проверить подключение
telnet server 8080
```

### Ошибки компиляции Windows

#### CUDA не найден
```cmd
# Установить переменную CUDA_PATH
setx CUDA_PATH "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6" /M

# Или добавить в PATH
setx PATH "%PATH%;%CUDA_PATH%\bin" /M
```

#### vcpkg библиотеки не найдены
```cmd
# Переустановить интеграцию vcpkg
cd C:\vcpkg
vcpkg integrate install

# Установить переменную VCPKG_ROOT
setx VCPKG_ROOT "C:\vcpkg" /M

# Переустановить библиотеки
vcpkg remove curl:x64-windows jsoncpp:x64-windows
vcpkg install curl:x64-windows jsoncpp:x64-windows
```

#### Ошибки линковщика
```
Ошибка: LNK2001: unresolved external symbol
```

**Решение:**
1. Убедитесь, что все .lib файлы найдены:
   ```cmd
   dir "%VCPKG_ROOT%\installed\x64-windows\lib\*.lib"
   ```

2. Проверьте настройки проекта:
   - Configuration Properties → VC++ Directories → Library Directories
   - Configuration Properties → Linker → Input → Additional Dependencies

#### Ошибки CUDA компилятора
```
nvcc fatal: Cannot find compiler 'cl.exe'
```

**Решение:**
1. Запустите Visual Studio Developer Command Prompt
2. Или добавьте MSVC в PATH:
   ```cmd
   call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
   ```

### Низкая производительность

#### Linux/Windows
```bash
# Проверить использование GPU
nvidia-smi

# Мониторинг клиента  
./RCKangarooClient -server http://server:8080 -gpu 0 # только один GPU для теста
```

#### Windows специфично
```cmd
# Проверить энергосбережение
powercfg /list
powercfg /setactive SCHEME_MIN  # Максимальная производительность

# Проверить тепловой троттлинг
nvidia-smi -q -d TEMPERATURE
```

### Потеря точек
- Увеличить значение DP битов
- Проверить стабильность сети
- Мониторить логи сервера

### Проблемы с файрволом Windows

#### Разрешить клиенту сетевые подключения
```cmd
# Добавить исключение в файрвол
netsh advfirewall firewall add rule name="RCKangarooClient" dir=out action=allow program="C:\path\to\RCKangarooClient.exe"
```

#### Разрешить серверу входящие подключения
```cmd
# Для Python сервера
netsh advfirewall firewall add rule name="Python Server" dir=in action=allow program="C:\Python39\python.exe" protocol=TCP localport=8080
```

## Безопасность

### Рекомендации
- Используйте HTTPS для продакшена
- Ограничьте доступ к серверу файрволом
- Регулярно делайте бэкапы базы данных
- Мониторьте ресурсы сервера

### Бэкап базы данных
```bash
# Создать бэкап
cp kangaroo_server.db backup_$(date +%Y%m%d_%H%M%S).db

# Автоматический бэкап
echo "0 */6 * * * cp /path/to/kangaroo_server.db /backup/$(date +%Y%m%d_%H%M%S).db" | crontab -
```

## Производительность

### Ожидаемые показатели
- **RTX 4090**: ~8-10 GKey/s
- **RTX 3090**: ~4-6 GKey/s  
- **RTX 3080**: ~3-5 GKey/s

### Оптимизация
- Используйте современные GPU (Ampere, Ada Lovelace)
- Подбирайте оптимальные значения DP
- Балансируйте нагрузку между клиентами
- Мониторьте сетевые задержки

## Лицензия

GPLv3 - см. файл LICENSE.TXT

## Поддержка

- GitHub: https://github.com/RetiredC
- Обсуждение: https://bitcointalk.org/index.php?topic=5517607