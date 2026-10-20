# Клиент для Windows и Linux в общем репозитории

Клиент включён в этот репозиторий в ветке **camera-client**, каталог
`camera_client/`. Ветка создана от main, поэтому содержит и прошивку.
Подмодуль и отдельное клонирование клиента не требуются.

```bash
git switch camera-client
cd camera_client
```

Полная инструкция: [README клиента](../camera_client/README.md).
Нужен Python 3.10+. Launcher создаёт локальное .venv и при необходимости
устанавливает Pillow. Окружение не переносится между Windows/Linux.

Windows PowerShell/cmd, из корня репозитория:

```powershell
cd camera_client
.\run.bat 192.168.1.123
```

Linux, из корня репозитория:

```bash
cd camera_client
./run.sh 192.168.1.123
```

В Debian/Ubuntu нужны python3-venv и python3-tk для окружения и GUI.
PPM сохраняется без сторонних пакетов и графического сеанса:

```text
python client.py 192.168.1.123 --save frame.ppm
```

Тесты, из корня репозитория:

```bash
python -m unittest discover -s camera_client -v
```

Workflow `.github/workflows/camera-client.yml` запускает клиентские тесты
на Windows/Linux, Python 3.10/3.13, из подкаталога camera_client.
Локальная проверка выполнена на Linux; нативный Windows здесь не запускался.
Исходная соседняя папка ~/projstm/camera_client оставлена как прежняя копия;
дальнейшие изменения в ней не синхронизируются с этой веткой автоматически.
