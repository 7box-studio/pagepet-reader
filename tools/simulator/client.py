"""Local pipe protocol for the native firmware process. No server or network."""
import json
from pathlib import Path
import queue
import struct
import subprocess
import threading
import time
import zlib


def read_exact(stream, length):
    data = bytearray()
    while len(data) < length:
        part = stream.read(length - len(data))
        if not part:
            raise EOFError("Симулятор закрыл поток экрана")
        data.extend(part)
    return bytes(data)


class Backend:
    def __init__(self, executable: Path, card: Path, env=None):
        self.events = queue.Queue()
        self.logs = queue.Queue(maxsize=400)
        self.closed = False
        self.process = subprocess.Popen(
            [str(executable.resolve()), str(card.resolve())],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=env,
        )
        self._frame_thread = threading.Thread(target=self._frames, daemon=True)
        self._log_thread = threading.Thread(target=self._logs, daemon=True)
        self._frame_thread.start()
        self._log_thread.start()

    def _frames(self):
        try:
            while header := self.process.stdout.readline():
                state = json.loads(header)
                length = state["bytes"]
                if not isinstance(length, int) or not 0 < length <= 1024 * 1024:
                    raise ValueError("Некорректный размер экрана")
                self.events.put((state, read_exact(self.process.stdout, length)))
            if not self.closed:
                self.events.put(RuntimeError("Процесс симулятора завершился; подробности в журнале"))
        except (ValueError, KeyError, EOFError, OSError) as error:
            if not self.closed:
                self.events.put(error)

    def _logs(self):
        for line in self.process.stderr:
            message = line.decode("utf-8", errors="replace").rstrip()
            if self.logs.full():
                try:
                    self.logs.get_nowait()
                except queue.Empty:
                    pass
            self.logs.put(message)

    def send(self, command):
        if self.closed or self.process.poll() is not None:
            raise RuntimeError("Процесс симулятора не запущен")
        self.process.stdin.write((command + "\n").encode("ascii"))
        self.process.stdin.flush()

    def ready(self, timeout=30):
        deadline = time.monotonic() + timeout
        while True:
            event = self.events.get(timeout=max(0, deadline - time.monotonic()))
            if isinstance(event, Exception):
                raise event
            if not event[0]["busy"]:
                return event

    def close(self):
        if self.closed:
            return
        self.closed = True
        if self.process.poll() is None:
            try:
                self.send_quit()
                self.process.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                self.process.terminate()
                try:
                    self.process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait()
        self._frame_thread.join(timeout=2)
        self._log_thread.join(timeout=2)
        for stream in (self.process.stdin, self.process.stdout, self.process.stderr):
            stream.close()

    def send_quit(self):
        self.process.stdin.write(b"quit\n")
        self.process.stdin.flush()


def gray_frame(pbm):
    magic, geometry, bitmap = pbm.split(b"\n", 2)
    if magic != b"P4":
        raise ValueError("Expected a PBM framebuffer")
    width, height = map(int, geometry.split())
    stride = (width + 7) // 8
    if len(bitmap) != stride * height:
        raise ValueError("Incomplete framebuffer")
    lookup = [bytes(0 if byte & (0x80 >> bit) else 255 for bit in range(8)) for byte in range(256)]
    rows = []
    for y in range(height):
        row = b"".join(lookup[byte] for byte in bitmap[y * stride:(y + 1) * stride])
        rows.append(row[:width])
    return width, height, rows


def as_pgm(pbm):
    # Tk's PPM loader accepts P5/P6, but not packed P4 images.
    width, height, rows = gray_frame(pbm)
    return f"P5\n{width} {height}\n255\n".encode("ascii") + b"".join(rows)


def save_png(pbm, path):
    """Export the full-resolution, portrait framebuffer using only stdlib."""
    width, height, rows = gray_frame(pbm)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    encoded = b"\x89PNG\r\n\x1a\n"
    encoded += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
    encoded += chunk(b"IDAT", zlib.compress(b"".join(b"\0" + row for row in rows))) + chunk(b"IEND", b"")
    Path(path).write_bytes(encoded)
