#!/usr/bin/env python3
"""OV2640 UDP snapshot receiver. Windows/Linux viewer requires Pillow and tkinter; PPM saving uses stdlib."""

import argparse

from pathlib import Path

import secrets

import socket

import struct

import time

REQUEST = struct.Struct('!4sII')

HEADER = struct.Struct('!4sIIIHHI')

CHUNK = 1200


def receive_frame(sock, timeout=8.0, test_pattern=False):
    """Получить один полный RGB565 кадр, повторяя потерянные UDP-запросы.

        Новый frame_id запускает snapshot только на первом запросе offset=0.
        Повторы сохраняют ID/offset; устаревшие ответы отбрасываются.
        Статус busy ожидается до общего deadline; принимаются только полные части.
        Возвращает (raw_bytes, width, height). test_pattern нужен ветке testpattern."""

    frame_id = secrets.randbits(32)

    offset = 0

    frame = bytearray()

    deadline = time.monotonic() + timeout

    while offset < 153600:
        if time.monotonic() >= deadline:
            raise TimeoutError('Нет полного кадра: проверьте сеть, питание и подключение камеры')

        sock.send(REQUEST.pack(b'TST1' if test_pattern else b'CAM1', frame_id, offset))

        try:
            data = sock.recv(2048)
        except socket.timeout:
            continue

        if len(data) < HEADER.size:
            continue

        magic, ident, pos, total, width, height, status = HEADER.unpack_from(data)

        if (magic, ident, pos) != (b'CAM1', frame_id, offset):
            continue  # Discard delayed replies to earlier requests.

        if (total, width, height) != (153600, 320, 240):
            raise ValueError('Неподдерживаемые размеры кадра')

        if status == 1:
            time.sleep(0.02)

            continue

        if status == 2:
            raise RuntimeError('Ошибка OV2640/DCMI: проверьте camera_init_error и camera_capture_errors в отладчике')

        if status != 0:
            raise RuntimeError(f'Ошибка протокола: {status}')

        payload = data[HEADER.size:]

        if len(payload) != min(CHUNK, total - offset):
            continue

        frame.extend(payload)

        offset += len(payload)

    return bytes(frame), width, height


def rgb565_to_rgb(raw, swap=False):
    """Преобразовать поток RGB565 в RGB888 для Pillow или PPM.

        По умолчанию старший байт первым; swap меняет порядок двух байтов.
        Биты расширяются до 8 без изменения геометрии изображения."""

    if len(raw) % 2:
        raise ValueError('Нечётная длина RGB565')

    output = bytearray(len(raw) // 2 * 3)

    for i in range(0, len(raw), 2):
        value = (raw[i + 1] << 8 | raw[i]) if swap else (raw[i] << 8 | raw[i + 1])

        r, g, b = value >> 11, value >> 5 & 63, value & 31

        j = i // 2 * 3

        output[j:j + 3] = bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))

    return bytes(output)


def save_image(path, raw, width, height, swap=False):
    """Write PPM without dependencies, or an actual PNG/JPEG with Pillow."""
    rgb = rgb565_to_rgb(raw, swap)

    suffix = path.suffix.lower()

    if suffix == '.ppm':
        path.write_bytes(f'P6\n{width} {height}\n255\n'.encode('ascii') + rgb)

        return

    formats = {'.png': 'PNG', '.jpg': 'JPEG', '.jpeg': 'JPEG'}

    if suffix not in formats:
        raise ValueError('Используйте расширение .ppm, .png, .jpg или .jpeg.')

    try:
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError('Для PNG/JPEG установите Pillow: python -m pip install -r requirements.txt') from exc

    Image.frombytes('RGB', (width, height), rgb).save(path, format=formats[suffix])


def main():
    """Разобрать аргументы, открыть UDP и сохранить кадр либо запустить GUI."""

    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument('host', nargs='?', default='192.168.1.123')

    parser.add_argument('--port', type=int, default=5000)

    parser.add_argument('--save', type=Path, help='Сохранить один кадр: .ppm (без пакетов), .png или .jpg (Pillow)')

    parser.add_argument('--test-pattern', action='store_true', help='Запросить тестовое изображение STM32 без захвата камеры')

    parser.add_argument('--swap-bytes', action='store_true', help='Поменять порядок байтов RGB565')

    args = parser.parse_args()

    if not 1 <= args.port <= 65535:
        parser.error('--port должен быть в диапазоне 1..65535')

    if args.save and args.save.suffix.lower() not in ('.ppm', '.png', '.jpg', '.jpeg'):
        parser.error('--save: используйте .ppm, .png, .jpg или .jpeg')

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.connect((args.host, args.port))

        sock.settimeout(0.2)

        if args.save:
            raw, w, h = receive_frame(sock, test_pattern=args.test_pattern)

            save_image(args.save, raw, w, h, args.swap_bytes)

            print(f'Сохранено: {args.save}')

            return

        try:
            import tkinter as tk

            from PIL import Image, ImageTk
        except ImportError as exc:
            raise RuntimeError('Для окна нужны Pillow и tkinter. Используйте run.py; на Linux установите python3-tk.') from exc

        try:
            root = tk.Tk()
        except tk.TclError as exc:
            raise RuntimeError('Не удалось открыть окно tkinter. Нужен графический сеанс; без него используйте --save frame.ppm.') from exc

        root.title('STM32H755 / UDP test' if args.test_pattern else 'OV2640 / STM32H755')

        label = tk.Label(root)

        label.pack()

        status = tk.Label(root, text='Подключение...')

        status.pack()

        # Network worker keeps the window responsive during capture/timeouts.
        import queue

        import threading

        results = queue.Queue(maxsize=1)

        stopped = threading.Event()

        def worker():
            """Получать кадры в фоне, чтобы сеть и таймауты не блокировали tkinter."""

            while not stopped.is_set():
                try:
                    raw, w, h = receive_frame(sock, test_pattern=args.test_pattern)

                    item = (rgb565_to_rgb(raw, args.swap_bytes), w, h, None)
                except (OSError, RuntimeError, ValueError, TimeoutError) as exc:
                    item = (None, 0, 0, str(exc))

                    stopped.wait(1)

                try:
                    results.put(item, timeout=0.1)
                except queue.Full:
                    pass

        def refresh():
            """Обновлять изображение только из главного GUI потока."""

            try:
                rgb, w, h, error = results.get_nowait()

                if error:
                    status.config(text=error)
                else:
                    photo = ImageTk.PhotoImage(Image.frombytes('RGB', (w, h), rgb).resize((640, 480)))

                    label.config(image=photo)

                    label.image = photo

                    status.config(text=f'{w}×{h} RGB565')
            except queue.Empty:
                pass

            root.after(30, refresh)

        def close():
            """Остановить фоновую работу и закрыть окно."""

            stopped.set()

            root.destroy()

        root.protocol('WM_DELETE_WINDOW', close)

        threading.Thread(target=worker, daemon=True).start()

        refresh()

        root.mainloop()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        raise SystemExit(130)
    except (OSError, RuntimeError, ValueError, TimeoutError, ImportError) as exc:
        raise SystemExit(str(exc))
