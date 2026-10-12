"""Compare IP setting writes with independently captured official GUI packets."""
import json
import pathlib
import unittest
from test_register_report import Device

REFERENCE = json.loads((pathlib.Path(__file__).parent /
                        'reference/official_utility_ip.json').read_text())

def region(device, base, length=80):
    return bytes(device.memory.get(base + i, 0) for i in range(length))

class UtilityIpTests(unittest.TestCase):
    def run_ip(self, device, current=False):
        args = ['127.0.0.1', '127.0.0.1' if current else '192.0.2.42']
        if current:
            args.append('--current')
        return device.run('sitcp-sitcpxg-ip-writer', *args)

    def test_write_packets_match_official_and_preserve_other_bytes(self):
        for xg in (False, True):
            for current in (False, True):
                with self.subTest(xg=xg, current=current):
                    device = Device(xg=xg)
                    try:
                        # Use the actual captured synthetic image, not a generated expected map.
                        device.put(0xFFFFFC00, bytes.fromhex(REFERENCE['eeprom_before']))
                        device.put(0xFFFFFF00, bytes.fromhex(REFERENCE['runtime_before']))
                        before = {base: region(device, base) for base in (0xFFFFFC00, 0xFFFFFF00)}
                        result = self.run_ip(device, current)
                        self.assertEqual(result.returncode, 0, result.stderr)
                        key = 'runtime_writes' if current else 'eeprom_writes'
                        expected = [(int(r['address'], 16), bytes.fromhex(r['payload']))
                                    for r in REFERENCE[key]]
                        self.assertEqual(device.write_packets, expected)
                        for base, original in before.items():
                            wanted = bytearray(original)
                            if not current and base == 0xFFFFFC00:
                                wanted[0x18:0x1c] = bytes([192, 0, 2, 42])
                            self.assertEqual(region(device, base), bytes(wanted))
                        data_write = next(i for i, r in enumerate(device.requests)
                                          if r[0] == 0x80)
                        base = 0xFFFFFF00 if current else 0xFFFFFC00
                        self.assertEqual(device.requests[:data_write].count((0xC0, base, 64)),
                                         2 if current else 1)
                    finally:
                        device.close()

    def test_unreadable_settings_abort_without_writes(self):
        for current in (False, True):
            for fault in ('utility_read_short', 'utility_read_bus'):
                device = Device(fault=fault)
                try:
                    result = self.run_ip(device, current)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertEqual(device.write_packets, [])
                finally:
                    device.close()

    def test_eeprom_failures_restore_protection_and_do_not_retry(self):
        for fault in ('utility_prefix_timeout', 'utility_parameters_timeout',
                      'utility_parameters_bus', 'utility_verify_mismatch'):
            device = Device(fault=fault)
            try:
                result = self.run_ip(device)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn('Success!', result.stdout)
                self.assertEqual(device.write_packets[-1], (0xFFFFFCFF, b'\xff'))
                addresses = [address for address, _ in device.write_packets]
                self.assertEqual(addresses.count(0xFFFFFC00), 1)
                self.assertEqual(addresses.count(0xFFFFFC18),
                                 0 if fault == 'utility_prefix_timeout' else 1)
            finally:
                device.close()

    def test_runtime_timeout_scope_and_verification(self):
        for fault in ('utility_prefix_timeout', 'utility_parameters_timeout',
                      'utility_parameters_bus', 'utility_verify_mismatch'):
            device = Device(fault=fault)
            try:
                result = self.run_ip(device, current=True)
                self.assertEqual(result.returncode == 0, fault == 'utility_parameters_timeout', result.stderr)
                addresses = [address for address, _ in device.write_packets]
                self.assertEqual(addresses.count(0xFFFFFF00), 1)
                self.assertEqual(addresses.count(0xFFFFFF18),
                                 0 if fault == 'utility_prefix_timeout' else 1)
                self.assertNotIn(0xFFFFFCFF, addresses)
            finally:
                device.close()

if __name__ == '__main__':
    unittest.main()
