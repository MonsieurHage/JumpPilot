"""Loopback integration tests against a fake ARSDK Jumping device.

Exercises the real C sockets/thread/handshake/retry/input/video code, not a Python copy.
Run via make test. No drone or Internet connection is required.
"""
import ctypes as C
import json
import pathlib
import socket
import struct
import sys
import threading
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
lib = C.CDLL(str(ROOT / "build" / ("libjumppilot.dylib" if sys.platform == "darwin" else "libjumppilot.so")))
CALLBACK = C.CFUNCTYPE(None, C.c_void_p, C.c_int, C.c_int, C.POINTER(C.c_uint8), C.c_size_t)
lib.jp_client_create.argtypes = [CALLBACK, C.c_void_p]
lib.jp_client_create.restype = C.c_void_p
lib.jp_client_connect.argtypes = [C.c_void_p, C.c_char_p, C.c_uint16]
for name in ("destroy", "disconnect", "halt"):
    getattr(lib, "jp_client_" + name).argtypes = [C.c_void_p]
lib.jp_client_arm.argtypes = [C.c_void_p, C.c_int]
lib.jp_client_drive.argtypes = [C.c_void_p, C.c_int, C.c_int]
lib.jp_client_jump.argtypes = [C.c_void_p, C.c_int]

def frame(kind, buffer, seq, data):
    return struct.pack("<BBBI", kind, buffer, seq, 7 + len(data)) + data

def frames(packet):
    while packet:
        assert len(packet) >= 7
        kind, buffer, seq, size = struct.unpack_from("<BBBI", packet)
        assert 7 <= size <= len(packet)
        yield kind, buffer, seq, packet[7:size]
        packet = packet[size:]

def until(check, timeout=3):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if check():
            return
        time.sleep(.01)
    raise AssertionError("Condition not reached before timeout")

class Drone:
    def __init__(self, refuse=False):
        self.tcp = socket.socket()
        self.tcp.bind(("127.0.0.1", 0))
        self.tcp.listen(1)
        self.tcp.settimeout(.05)
        self.port = self.tcp.getsockname()[1]
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp.bind(("127.0.0.1", 0))
        self.udp.settimeout(.02)
        self.target = None
        self.received = []
        self.errors = []
        self.handshake = None
        self.quit = False
        self.silent = False
        self.refuse = refuse
        self.drop_jump_ack = False
        self.thread = threading.Thread(target=self.run)
        self.thread.start()

    def run(self):
        try:
            while not self.quit:
                try:
                    connection, _ = self.tcp.accept()
                    break
                except socket.timeout:
                    continue
            else:
                return
            with connection:
                connection.settimeout(2)
                request = b""
                while b"\0" not in request:
                    request += connection.recv(4096)
                self.handshake = json.loads(request.split(b"\0")[0])
                assert self.handshake["controller_type"] == "iPhone"
                self.target = ("127.0.0.1", self.handshake["d2c_port"])
                response = json.dumps({"status": int(self.refuse),
                    "c2d_port": self.udp.getsockname()[1] if not self.refuse else 0,
                    "arstream_fragment_size": 8, "arstream_fragment_maximum_number": 4,
                    "arstream_max_ack_interval": -1}).encode() + b"\0"
                # TCP delivers only a portion of the JSON at a time.
                connection.sendall(response[:17])
                time.sleep(.03)
                connection.sendall(response[17:])
            if self.refuse:
                return
            first_setting = None
            self.send_battery = lambda: self.send(frame(4, 126, 10, bytes([0, 5, 1, 0, 73])))
            next_ping = time.monotonic()
            while not self.quit:
                if not self.silent and time.monotonic() >= next_ping:
                    self.send(frame(2, 0, 4, b"ping1234"))
                    next_ping = time.monotonic() + .2
                try:
                    packet, _ = self.udp.recvfrom(65536)
                except socket.timeout:
                    continue
                for kind, buffer, seq, data in frames(packet):
                    self.received.append((time.monotonic(), kind, buffer, seq, data))
                    if self.silent:
                        continue
                    if buffer == 11 and kind == 4:
                        if self.drop_jump_ack and data[:4] == bytes([3, 2, 3, 0]):
                            continue
                        if data == bytes([0, 2, 0, 0]):
                            if first_setting is None:
                                first_setting = seq
                                continue  # Drop ACK once; exact packet sequence must be retried.
                            assert first_setting == seq
                        self.send(frame(1, 139, 1, bytes([seq])))
                        if data == bytes([0, 4, 0, 0]):
                            self.send_battery()
                            self.send_battery()  # Retransmitted event should be ACKed twice, delivered once.
                        if data == bytes([3, 18, 0, 0, 1]):
                            jpeg = b"\xff\xd8\x01\x02\x03\x04\x05\x06\x07\x08\xff\xd9"
                            first = struct.pack("<HBBB", 65535, 1, 0, 2) + jpeg[:8]
                            last = struct.pack("<HBBB", 65535, 1, 1, 2) + jpeg[8:]
                            # Reverse fragment order and combine multiple ARNetwork frames in one UDP.
                            self.send(frame(3, 125, 2, last) + frame(3, 125, 1, first))
        except Exception as exc:
            if not self.quit:
                self.errors.append(exc)

    def send(self, packet):
        self.udp.sendto(packet, self.target)

    def close(self):
        self.quit = True
        self.thread.join(3)
        assert not self.thread.is_alive()
        self.tcp.close()
        self.udp.close()
        assert not self.errors, self.errors

def main():
    received = []
    @CALLBACK
    def callback(_, event, value, data, length):
        received.append((event, value, C.string_at(data, length) if length else b""))

    drone = Drone()
    client = lib.jp_client_create(callback, None)
    assert client
    try:
        assert lib.jp_client_jump(client, 1) == -1
        assert lib.jp_client_connect(client, b"invalid", 44444) == -1
        assert lib.jp_client_connect(client, b"127.0.0.1", drone.port) == 0
        until(lambda: any(e == 1 and v == 2 for e, v, _ in received))
        until(lambda: any(e == 2 and v == 73 for e, v, _ in received))
        until(lambda: any(e == 3 for e, _, _ in received))
        assert sum(e == 2 for e, _, _ in received) == 1
        video = next(d for e, _, d in received if e == 3)
        assert video == b"\xff\xd8\x01\x02\x03\x04\x05\x06\x07\x08\xff\xd9"
        until(lambda: sum(b == 254 and data == b"\x0a" for _, _, b, _, data in drone.received) == 2)
        settings = [(s, d) for _, _, b, s, d in drone.received if b == 11 and d == bytes([0, 2, 0, 0])]
        assert len(settings) == 2 and settings[0] == settings[1]
        assert any(b == 1 and data == b"ping1234" for _, _, b, _, data in drone.received)
        print("OK: real TCP discovery, split JSON, negotiated UDP port, ACK retry, duplicate event, ping")
        print("OK: video fragments out of order and multiple frames in one UDP datagram")

        lib.jp_client_drive(client, 50, -20)  # Disarmed: wheels must remain zero.
        time.sleep(.1)
        assert all(d[4:] == b"\0\0\0" for _, _, b, _, d in drone.received if b == 10)
        lib.jp_client_arm(client, 1)
        drive_at = time.monotonic()
        lib.jp_client_drive(client, 50, -20)
        until(lambda: any(t > drive_at and b == 10 and d[4:] == bytes([1, 50, 236]) for t, _, b, _, d in drone.received))
        # Do not refresh inputs: the real transport must stop by itself.
        until(lambda: any(t > drive_at + .35 and b == 10 and d[4:] == b"\0\0\0" for t, _, b, _, d in drone.received))
        drone.drop_jump_ack = True
        assert lib.jp_client_jump(client, 1) == 0
        assert lib.jp_client_jump(client, 0) == -1
        until(lambda: any(b == 11 and d == bytes([3, 2, 3, 0, 1, 0, 0, 0]) for _, _, b, _, d in drone.received))
        lib.jp_client_halt(client)
        assert lib.jp_client_jump(client, 1) == -1
        halt_at = time.monotonic()
        lib.jp_client_drive(client, 100, 100)
        until(lambda: any(t > halt_at and b == 10 and d[4:] == b"\0\0\0" for t, _, b, _, d in drone.received))
        time.sleep(.3)
        assert sum(b == 11 and d[:4] == bytes([3, 2, 3, 0]) for _, _, b, _, d in drone.received) == 1
        print("OK: disarmed input gate, signed motion, 350 ms deadman, jump cooldown, STOP cancels retries")

        drone.send(frame(4, 126, 11, bytes([3, 1, 1, 0, 1, 0, 0, 0])))
        until(lambda: any(e == 5 and v == 1 for e, v, _ in received))
        lib.jp_client_arm(client, 1)
        critical_at = time.monotonic()
        lib.jp_client_drive(client, 70, 70)
        time.sleep(.15)
        assert all(d[4:] == b"\0\0\0" for t, _, b, _, d in drone.received if t > critical_at and b == 10)
        print("OK: critical battery interlock rejects rearming")

        drone.silent = True
        until(lambda: any(e == 1 and v == 3 for e, v, _ in received), timeout=4)
        assert lib.jp_client_jump(client, 1) == -1
        print("OK: lost-link watchdog, disarm and explicit reconnection requirement")
    finally:
        lib.jp_client_destroy(client)
        drone.close()

    refused = Drone(refuse=True)
    received.clear()
    client = lib.jp_client_create(callback, None)
    try:
        assert lib.jp_client_connect(client, b"127.0.0.1", refused.port) == 0
        until(lambda: any(e == 1 and v == 3 for e, v, _ in received))
        assert any(b"refus" in d for e, _, d in received if e == 1)
        print("OK: connection refusal reported without enabling motion")
    finally:
        lib.jp_client_destroy(client)
        refused.close()

    # A server that never accepts/responds must not block cancel on the main UI thread.
    tcp = socket.socket(); tcp.bind(("127.0.0.1", 0)); tcp.listen(1)
    client = lib.jp_client_create(callback, None)
    try:
        lib.jp_client_connect(client, b"127.0.0.1", tcp.getsockname()[1])
        time.sleep(.1)
        start = time.monotonic(); lib.jp_client_disconnect(client)
        assert time.monotonic() - start < .25
        print("OK: responsive cancellation during incomplete discovery")
    finally:
        lib.jp_client_destroy(client); tcp.close()

if __name__ == "__main__":
    main()
