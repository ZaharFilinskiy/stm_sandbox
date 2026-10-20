# Клиент для Windows и Linux

Исходники клиента: `../camera_client` относительно корня stm_sandbox.
Это отдельный соседний Git-репозиторий, поэтому clone прошивки сам по себе
не скачивает клиента. Клонируйте/скопируйте оба репозитория в одну папку.

Полная инструкция: [README клиента](../../camera_client/README.md).
Требуется Python 3.10+; графическое окно использует tkinter и Pillow.

Windows PowerShell/cmd:

```powershell
cd ..\camera_client
.\run.bat 192.168.1.123
```

Linux:

```bash
cd ../camera_client
./run.sh 192.168.1.123
```

Launcher создаёт платформенное .venv и при необходимости устанавливает Pillow.
Окружение не переносится между ОС. На Linux tkinter устанавливается
системным пакетным менеджером (для Debian/Ubuntu python3-tk и python3-venv).

PPM сохраняется стандартной библиотекой без графического окна, PNG/JPEG
используют Pillow. Формат соответствует расширению файла:

```text
python client.py 192.168.1.123 --save frame.ppm
```

GitHub Actions клиента содержит матрицу Windows/Linux, Python 3.10/3.13.
Локальная проверка здесь выполнена на Linux; Windows job выполнится после
публикации клиента на GitHub. Firmware/протокол CAM1 для переносимости
клиента не менялись. Снимки, .venv и cache в Git не включаются.
