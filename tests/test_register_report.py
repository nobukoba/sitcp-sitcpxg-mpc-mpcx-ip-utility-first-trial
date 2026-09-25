"""Integration checks using synthetic registers and a local RBCP device."""
import pathlib
import socket
import struct
import subprocess
import tempfile
import threading
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class Device:
    def __init__(self, xg=True, rate=10000, fault=None):
        self.memory = {}
        self.requests = []
        self.fault = fault
        for base in (0xFFFFFF00, 0xFFFFFC00):
            for offset in range(80):
                self.memory[base + offset] = offset
        self.put(0xFFFFFF08, b'XTCP' if xg else b'SiTC')
        self.put(0xFFFFFF18, bytes([127, 0, 0, 1]))
        self.put(0xFFFFFC18, bytes([192, 0, 2, 10]))
        self.put(0xFFFFFF40, struct.pack('!H', rate))
        self.put(0xFFFFFC40, struct.pack('!H', 1000))
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(('127.0.0.1', 0))
        self.socket.settimeout(0.05)
        self.port = self.socket.getsockname()[1]
        self.running = True
        self.thread = threading.Thread(target=self.serve)
        self.thread.start()

    def put(self, address, data):
        self.memory.update((address + i, value) for i, value in enumerate(data))

    def serve(self):
        while self.running:
            try:
                packet, peer = self.socket.recvfrom(512)
            except socket.timeout:
                continue
            version, command, ident, length, address = struct.unpack('!BBBBI', packet[:8])
            self.requests.append((command, address, length))
            if command == 0x80:
                self.put(address, packet[8:])
            data = bytes(self.memory.get(address + i, 0) for i in range(length))
            status = command | 8
            if command == 0x80 and address == 0xFFFFFC20:
                if self.fault == 'write_bus':
                    status |= 1
                if self.fault == 'write_timeout':
                    continue
                if self.fault == 'verify_mismatch':
                    self.memory[address] ^= 1
            if command == 0x80 and address == 0xFFFFFCFF and packet[8:] == b'\x00':
                if self.fault == 'enable_timeout':
                    continue
            if address <= 0xFFFFFF48 < address + length and self.fault == 'bus':
                status |= 1
            if self.fault == 'rate' and address <= 0xFFFFFF41 < address + length:
                status |= 1
            if self.fault == 'eeprom' and address <= 0xFFFFFC40 < address + length:
                status |= 1
            if self.fault == 'block' and length == 8 and address == 0xFFFFFF40:
                status |= 1
            if address == 0xFFFFFF48 and self.fault == 'short':
                data = data[:-1]
            self.socket.sendto(struct.pack('!BBBBI', version, status, ident, length, address) + data, peer)

    def close(self):
        self.running = False
        self.thread.join()
        self.socket.close()

    def run(self, program, *args):
        return subprocess.run(
            [str(ROOT / 'bin' / program), *args, '--port', str(self.port), '--timeout', '0.1'],
            text=True, capture_output=True, timeout=10)


class ReportTests(unittest.TestCase):
    def check_dump(self, output, device):
        runtime, eeprom = output.split('EEPROM (0xFFFFFC00', 1)
        self.assertIn('current IP          : 127.0.0.1', runtime)
        self.assertIn('EEPROM IP           : 192.0.2.10', eeprom)
        for base in (0xFFFFFF00, 0xFFFFFC00):
            for offset in range(0, 80, 16):
                expected = ' '.join(f'{device.memory[base + offset + i]:02X}' for i in range(16))
                self.assertIn(f'{base + offset:08X}: {expected}', output)

    def test_read_views(self):
        for xg in (False, True):
            for program, args in (
                ('mpc-mpcx-ip-reader', ['127.0.0.1']),
                ('mpc-mpcx-ip-command', ['read', '127.0.0.1']),
                ('mpc-mpcx-ip-command', ['ip-read', '127.0.0.1']),
            ):
                with self.subTest(xg=xg, program=program, args=args):
                    device = Device(xg=xg)
                    try:
                        result = device.run(program, *args)
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.check_dump(result.stdout, device)
                        if xg:
                            self.assertIn('10000 (0x2710) Mbps', result.stdout)
                            self.assertIn('1000 (0x03E8) Mbps', result.stdout)
                            self.assertIn('10795 (0x2A2B)', result.stdout)
                        else:
                            self.assertNotIn('transmission rate', result.stdout)
                        self.assertTrue(all(req[0] == 0xC0 for req in device.requests))
                        self.assertFalse(any(req[1] == 0xFFFFFF50 for req in device.requests))
                    finally:
                        device.close()

    def test_invalid_rate(self):
        device = Device(rate=65535)
        try:
            result = device.run('mpc-mpcx-ip-reader', '127.0.0.1')
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('65535 (0xFFFF) (outside documented range', result.stdout)
            self.assertNotIn('65535 (0xFFFF) Mbps', result.stdout)
        finally:
            device.close()

    def test_incomplete_runtime_fails(self):
        for fault in ('short', 'bus'):
            device = Device(fault=fault)
            try:
                result = device.run('mpc-mpcx-ip-reader', '127.0.0.1')
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn('READ OK', result.stdout)
            finally:
                device.close()

    def test_partial_register_reports(self):
        for fault, address in (('bus', '0xFFFFFF48'), ('rate', '0xFFFFFF41'),
                               ('eeprom', '0xFFFFFC40')):
            for program, args in (
                ('mpc-mpcx-ip-reader', ['127.0.0.1']),
                ('mpc-mpcx-ip-command', ['read', '127.0.0.1']),
                ('mpc-mpcx-ip-command', ['ip-read', '127.0.0.1']),
            ):
                device = Device(fault=fault)
                try:
                    result = device.run(program, *args)
                    self.assertEqual(result.returncode, 3, result.stderr)
                    self.assertIn(address, result.stderr)
                    self.assertIn('PARTIAL', result.stdout)
                    self.assertIn('raw EEPROM FC00..FC4F:', result.stdout)
                    self.assertIn('current IP          : 127.0.0.1', result.stdout)
                    self.assertIn('??', result.stdout)
                    if fault == 'rate':
                        runtime = result.stdout.split('EEPROM (0xFFFFFC00')[0]
                        self.assertIn('transmission rate   : unavailable', runtime)
                        self.assertNotIn('10000 (0x2710)', runtime)
                        self.assertIn('FFFFFF40: 27 ?? 42 43 44 45 46 47', runtime)
                    if fault == 'eeprom':
                        self.assertIn('FFFFFC40: ?? E8 42 43', result.stdout)
                    self.assertTrue(all(req[0] == 0xC0 for req in device.requests))
                finally:
                    device.close()

    def test_block_error_byte_recovery(self):
        device = Device(fault='block')
        try:
            result = device.run('mpc-mpcx-ip-reader', '127.0.0.1')
            self.assertEqual(result.returncode, 0, result.stderr)
            self.check_dump(result.stdout, device)
            self.assertIn('COMPLETE', result.stdout)
        finally:
            device.close()

    def test_partial_diagnostics_do_not_block_writer(self):
        device = Device(fault='rate')
        try:
            result = device.run('mpc-mpcx-ip-command', 'ip-write', '127.0.0.1', '192.0.2.20')
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.count('PARTIAL'), 2)
            self.assertIn('WRITE/VERIFY OK', result.stdout)
            self.assertEqual(device.memory[0xFFFFFCFF], 255)
        finally:
            device.close()

    def test_writer_before_after(self):
        for xg in (False, True):
            device = Device(xg=xg)
            try:
                with tempfile.TemporaryDirectory() as temp:
                    payload = pathlib.Path(temp) / 'synthetic.bin'
                    payload.write_bytes(bytes([0x2C] * 7 + [0] * 15) if xg else bytes(22))
                    result = device.run('mpc-mpcx-ip-writer', '127.0.0.1', str(payload))
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.count('raw runtime FF00..FF4F:'), 2)
                self.assertEqual(result.stdout.count('raw EEPROM FC00..FC4F:'), 2)
                self.assertEqual(device.memory[0xFFFFFCFF], 255)
                self.assertIn('WRITE/VERIFY OK', result.stdout)
            finally:
                device.close()

    def test_ip_writer_before_after(self):
        device = Device()
        try:
            result = device.run('mpc-mpcx-ip-command', 'ip-write', '127.0.0.1', '192.0.2.20')
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.count('raw runtime FF00..FF4F:'), 2)
            self.assertIn('192.0.2.20', result.stdout.split('after:', 1)[1])
            self.assertEqual(device.memory[0xFFFFFCFF], 255)
        finally:
            device.close()


if __name__ == '__main__':
    unittest.main()
