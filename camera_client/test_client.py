import socket
import threading
import unittest
from unittest.mock import patch
import client

class ReceiverTests(unittest.TestCase):
    def test_colors_and_swap(self):
        self.assertEqual(client.rgb565_to_rgb(bytes.fromhex('f80007e0001f')), bytes.fromhex('ff000000ff000000ff'))
        self.assertEqual(client.rgb565_to_rgb(bytes.fromhex('00f8'), True), bytes.fromhex('ff0000'))

    def test_loss_busy_stale_and_repeated_frames(self):
        frame = bytes(range(256)) * 600
        stop = threading.Event()
        errors = []
        server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        server.bind(('127.0.0.1', 0))
        server.settimeout(0.1)
        def serve():
            dropped = busy = stale = False
            while not stop.is_set():
                try:
                    request, addr = server.recvfrom(2048)
                except socket.timeout:
                    continue
                except OSError:
                    return
                _, ident, off = client.REQUEST.unpack(request)
                if not dropped:
                    dropped = True
                    continue
                status = 0
                if not busy:
                    busy = True
                    status = 1
                if not stale:
                    stale = True
                    server.sendto(client.HEADER.pack(b'CAM1', ident ^ 1, off, len(frame), 320, 240, 0), addr)
                header = client.HEADER.pack(b'CAM1', ident, off, len(frame), 320, 240, status)
                server.sendto(header + (frame[off:off+client.CHUNK] if not status else b''), addr)
        thread = threading.Thread(target=serve)
        thread.start()
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
                receiver.connect(server.getsockname())
                receiver.settimeout(0.05)
                for _ in range(2):
                    self.assertEqual(client.receive_frame(receiver), (frame, 320, 240))
        finally:
            stop.set()
            thread.join(1)
            server.close()


# File format and command-line checks run on both Windows and Linux.
class PortableClientTests(unittest.TestCase):
    def test_ppm_needs_no_pillow(self):
        import builtins
        from pathlib import Path
        import tempfile
        original_import = builtins.__import__

        def without_pillow(name, *args, **kwargs):
            if name == 'PIL' or name.startswith('PIL.'):
                raise ImportError('Pillow deliberately unavailable')
            return original_import(name, *args, **kwargs)

        with tempfile.TemporaryDirectory(prefix='camera test ') as directory:
            output = Path(directory) / 'цветной кадр.ppm'
            with patch('builtins.__import__', side_effect=without_pillow):
                client.save_image(output, bytes.fromhex('f80007e0001f'), 3, 1)
            self.assertEqual(output.read_bytes(), b'P6\n3 1\n255\n' + bytes.fromhex('ff000000ff000000ff'))

    def test_png_and_jpeg_have_real_file_signatures(self):
        from pathlib import Path
        import tempfile
        try:
            from PIL import Image
        except ImportError:
            self.skipTest('Pillow not installed')
        with tempfile.TemporaryDirectory(prefix='camera test ') as directory:
            for suffix, signature in [('.png', b'\x89PNG\r\n\x1a\n'), ('.jpg', b'\xff\xd8'), ('.jpeg', b'\xff\xd8')]:
                output = Path(directory) / ('кадр' + suffix)
                client.save_image(output, b'\xf8\x00' * 4, 2, 2)
                self.assertTrue(output.read_bytes().startswith(signature))
                with Image.open(output) as image:
                    self.assertEqual(image.size, (2, 2))

    def test_invalid_format_and_port_fail_before_network(self):
        from pathlib import Path
        import subprocess
        import sys
        for arguments in [('--port', '0'), ('--save', 'frame.xyz')]:
            result = subprocess.run([sys.executable, str(Path(client.__file__)), *arguments], capture_output=True)
            self.assertEqual(result.returncode, 2)

if __name__ == '__main__':
    unittest.main()
