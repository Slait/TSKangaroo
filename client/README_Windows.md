# RCKangaroo Distributed Client - Windows Build Guide

Инструкция по сборке клиента RCKangaroo для Windows.

## Быстрый старт

### Автоматическая установка (рекомендуется)

1. **Запустите CMD или PowerShell от имени администратора**

2. **Выполните автоматическую установку:**
```cmd
setup_windows.bat
```

3. **Соберите проект:**
```cmd
build_windows.bat
```

4. **Запустите клиент:**
```cmd
x64\Release\RCKangarooClient.exe -server http://your-server:8080
```

## Требования

- **Операционная система**: Windows 10/11 x64
- **Visual Studio**: 2019 или 2022 (Community/Professional/Enterprise)
- **CUDA Toolkit**: 11.8+ (рекомендуется 12.x)
- **Git**: для скачивания зависимостей

## Файлы проекта

- `RCKangarooClient.sln` - Solution файл для Visual Studio
- `RCKangarooClient.vcxproj` - Файл проекта Visual Studio
- `setup_windows.bat` - Автоматическая установка зависимостей
- `build_windows.bat` - Автоматическая сборка

## Ручная установка

### 1. Установка Visual Studio

Скачайте Visual Studio с официального сайта:
https://visualstudio.microsoft.com/

При установке выберите:
- **Workload**: "Desktop development with C++"
- **Individual components**:
  - MSVC v143 compiler toolset
  - Windows 10/11 SDK
  - CMake tools

### 2. Установка CUDA Toolkit

Скачайте CUDA Toolkit с:
https://developer.nvidia.com/cuda-downloads

Выберите:
- Operating System: Windows
- Architecture: x86_64
- Version: 10/11
- Installer Type: exe (local)

### 3. Установка vcpkg

```cmd
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
vcpkg integrate install
```

### 4. Установка библиотек

```cmd
vcpkg install curl:x64-windows
vcpkg install jsoncpp:x64-windows
```

### 5. Настройка переменных окружения

```cmd
setx VCPKG_ROOT "C:\vcpkg" /M
setx CUDA_PATH "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6" /M
```

Перезапустите CMD/PowerShell после установки переменных.

## Сборка

### Через Visual Studio

1. Откройте `RCKangarooClient.sln` в Visual Studio
2. Выберите конфигурацию **Release** и платформу **x64**
3. Build → Build Solution (Ctrl+Shift+B)

### Через командную строку

```cmd
build_windows.bat
```

Или используя MSBuild напрямую:
```cmd
MSBuild.exe RCKangarooClient.sln /p:Configuration=Release /p:Platform=x64
```

## Результат сборки

Исполняемый файл будет создан в:
```
x64\Release\RCKangarooClient.exe
```

## Тестирование

```cmd
cd x64\Release
RCKangarooClient.exe -server http://localhost:8080
```

## Устранение неполадок

### CUDA не найден

```cmd
# Проверить установку CUDA
dir "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA"

# Установить переменную окружения
setx CUDA_PATH "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6" /M
```

### vcpkg библиотеки не найдены

```cmd
# Проверить установку
dir "%VCPKG_ROOT%\installed\x64-windows\lib"

# Переустановить
vcpkg remove curl:x64-windows jsoncpp:x64-windows
vcpkg install curl:x64-windows jsoncpp:x64-windows
```

### Ошибки компиляции

1. **Убедитесь, что используете x64 платформу**, а не x86
2. **Проверьте версию CUDA** - должна быть совместима с Visual Studio
3. **Очистите сборку** перед повторной попыткой:
   ```cmd
   rmdir /s /q x64
   ```

### Проблемы с антивирусом

Некоторые антивирусы могут блокировать сборку. Добавьте в исключения:
- Папку проекта
- `C:\vcpkg\`
- Visual Studio
- CUDA Toolkit

## Производительность

Для максимальной производительности:

1. **Отключите энергосбережение**:
   ```cmd
   powercfg /setactive SCHEME_MIN
   ```

2. **Проверьте температуру GPU**:
   ```cmd
   nvidia-smi -q -d TEMPERATURE
   ```

3. **Используйте Release конфигурацию** вместо Debug

## Поддержка

- Полная документация: `../DISTRIBUTED_README.md`
- GitHub: https://github.com/RetiredC
- Bitcoin Talk: https://bitcointalk.org/index.php?topic=5517607