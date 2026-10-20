#!/usr/bin/env python3
"""Portable launcher: create a private venv and run the camera receiver."""

import os
from pathlib import Path
import subprocess
import sys
import venv


def main():
    """Prepare dependencies for this OS; all paths are relative to this file."""
    if sys.version_info < (3, 10):
        raise RuntimeError('Нужен Python 3.10 или новее.')

    directory = Path(__file__).resolve().parent

    environment = directory / '.venv'

    python = environment / ('Scripts/python.exe' if os.name == 'nt' else 'bin/python')

    if not python.is_file():
        print('Создание окружения Python...', flush=True)

        venv.EnvBuilder(with_pip=True).create(environment)

    if '--help' not in sys.argv[1:] and '-h' not in sys.argv[1:]:
        # PPM snapshots only need the standard library; GUI and PNG/JPEG need Pillow.
        arguments = sys.argv[1:]

        save_index = next((i for i, a in enumerate(arguments) if a == '--save' or a.startswith('--save=')), None)

        save_path = None

        if save_index is not None:
            item = arguments[save_index]

            save_path = item.partition('=')[2] if '=' in item else (arguments[save_index + 1] if save_index + 1 < len(arguments) else None)

        needs_pillow = save_index is None or (save_path is not None and Path(save_path).suffix.lower() != '.ppm')

        if needs_pillow:
            check = subprocess.run([str(python), '-c', 'import PIL'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

            if check.returncode:
                subprocess.run([str(python), '-m', 'pip', 'install', '-r', str(directory / 'requirements.txt')], check=True)

    return subprocess.call([str(python), str(directory / 'client.py'), *sys.argv[1:]])


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f'Ошибка запуска: {exc}\nНа Linux проверьте пакеты python3-venv и python3-tk.', file=sys.stderr)

        raise SystemExit(1)
    except KeyboardInterrupt:
        raise SystemExit(130)
