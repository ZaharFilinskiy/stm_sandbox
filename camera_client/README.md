# OV2640 / STM32 UDP client — Windows и Linux

Python-клиент для прошивки `stm_sandbox`: получение RGB565 320×240 по UDP,
просмотр в окне и сохранение изображения. Требуется **Python 3.10+**.
Клиент находится в общем репозитории прошивки, в ветке **camera-client**,
в каталоге `camera_client/`. Отдельный репозиторий клиента не нужен.

```text
stm_sandbox/
  Core/             # прошивка STM32
  docs/             # документация платы и алгоритма
  camera_client/    # этот клиент
  .github/workflows/camera-client.yml
```

После клонирования общего репозитория:

```bash
git switch camera-client
cd camera_client
```

## Windows: быстрый запуск

Установите Python 3.10+ с поддержкой Tcl/Tk и опцией Add Python to PATH.
Откройте PowerShell или cmd в папке клиента:

```powershell
.\run.bat 192.168.1.123
```

Либо используйте Python Launcher напрямую:

```powershell
py -3 run.py 192.168.1.123
```

`run.py` создаёт локальное окружение `.venv` и при необходимости устанавливает
Pillow. При первой установке пакетов нужен интернет. Повторный запуск использует
установленные пакеты. Активация PowerShell-скрипта не требуется, поэтому менять
ExecutionPolicy не нужно. Пути с пробелами поддерживаются.

## Linux: быстрый запуск

Для Debian/Ubuntu необходимые системные компоненты:

```bash
sudo apt install python3 python3-venv python3-tk
```

Затем из папки клиента:

```bash
./run.sh 192.168.1.123
```

Также работает `python3 run.py 192.168.1.123`. В других дистрибутивах установите
эквивалентные пакеты Python, venv и tkinter своим пакетным менеджером.
Для окна нужен графический сеанс. В headless/SSH режиме сохраняйте PPM.

## Сохранение кадра

PPM не требует Pillow и tkinter:

```powershell
# Windows
py -3 client.py 192.168.1.123 --save frame.ppm
```

```bash
# Linux
python3 client.py 192.168.1.123 --save frame.ppm
```

PNG/JPEG требуют Pillow; launcher установит его автоматически:

```powershell
.\run.bat 192.168.1.123 --save frame.png
```

```bash
./run.sh 192.168.1.123 --save frame.jpg
```

Формат файла соответствует расширению. Файл .jpg теперь содержит JPEG,
а не PPM под другим именем. Существующий файл назначения перезаписывается.

## Ручная установка без launcher

Windows:

```powershell
py -3 -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements.txt
.venv\Scripts\python.exe client.py 192.168.1.123
```

Linux:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python client.py 192.168.1.123
```

`.venv` платформозависима: не копируйте окружение между Windows/Linux,
создавайте его заново из исходников. В Git окружение и полученные снимки
не включаются. На Windows tkinter ставится установщиком Python, не pip.

## Сеть и параметры

IP платы по умолчанию 192.168.1.123, UDP-порт 5000. Настройте Ethernet ПК
в той же подсети, например 192.168.1.10/24. При запросе firewall Windows
разрешите Python доступ к частной сети. У платы могут быть сохранённые
сетевые настройки, отличающиеся от адреса по умолчанию.

```text
client.py [host] [--port 5000] [--save path] [--swap-bytes] [--test-pattern]
```

`--swap-bytes` меняет порядок байтов RGB565. `--test-pattern` понимает только
прошивка ветки testpattern; main принимает реальные кадры CAM1.
Работающий viewer не требует доступа к последовательному порту или ST-LINK.
Ctrl+C завершает клиент; окно закрывается обычной кнопкой закрытия.

Сообщение OV2640/DCMI означает ответ платы с ошибкой захвата. Таймаут без
ответа требует проверки IP, кабеля, firewall и прошивки. При ошибке tkinter
проверьте его установку и наличие графического сеанса.

## Тестирование и Git

```bash
python -m unittest -v
```

Тесты проверяют RGB565, потери UDP, busy, устаревшие пакеты, повторные кадры
и форматы сохранения. Они используют локальный UDP, физическая плата не нужна.
GitHub Actions настроен для Windows/Linux и Python 3.10/3.13; фактическое
выполнение workflow начнётся после публикации репозитория на GitHub.

Подробное устройство прошивки: [CAMERA_ARCHITECTURE.md](../docs/CAMERA_ARCHITECTURE.md).
Клиент и прошивка версионируются одним репозиторием. Работа клиента ведётся
в ветке camera-client. Локальное окружение и снимки исключены из Git.
