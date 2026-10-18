# SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
# SPDX-License-Identifier: LGPL-3.0-or-later

import contextlib
import importlib.util
import io
import os
import struct
import sys
import tempfile
import unittest
from unittest import mock

VALIDATOR_PATH = os.path.join(os.path.dirname(__file__), "validate_arm64_architecture.py")
SPEC = importlib.util.spec_from_file_location("validate_arm64_architecture", VALIDATOR_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("Unable to load validator from %s" % VALIDATOR_PATH)
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


def write_pe(path, machine):
    data = bytearray(0x80)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x40)
    data[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<H", data, 0x44, machine)
    with open(path, "wb") as stream:
        stream.write(data)


class Arm64ArchitectureValidatorTests(unittest.TestCase):
    def test_host_api_requires_independent_arm64_interpreter(self):
        kernel32 = mock.Mock()
        def query(_handle, process, native):
            process._obj.value = validator.IMAGE_FILE_MACHINE_UNKNOWN
            native._obj.value = validator.IMAGE_FILE_MACHINE_ARM64
            return 1
        kernel32.IsWow64Process2.side_effect = query
        with tempfile.TemporaryDirectory() as root:
            executable = os.path.join(root, "python.exe")
            for machine, expected in ((validator.IMAGE_FILE_MACHINE_ARM64, True),
                                      (validator.IMAGE_FILE_MACHINE_AMD64, False),
                                      (validator.IMAGE_FILE_MACHINE_I386, False)):
                with self.subTest(machine=machine):
                    write_pe(executable, machine)
                    with mock.patch.object(sys, "platform", "win32"), \
                         mock.patch.object(sys, "executable", executable), \
                         mock.patch.object(validator.ctypes, "WinDLL", return_value=kernel32, create=True):
                        result = validator.read_native_process_oracle()
                    self.assertEqual(result["native_arm64_ok"], expected)
                    self.assertEqual(result["executable_machine"], machine)
            with open(executable, "w") as stream:
                stream.write("invalid executable")
            with mock.patch.object(sys, "platform", "win32"), \
                 mock.patch.object(sys, "executable", executable), \
                 mock.patch.object(validator.ctypes, "WinDLL", return_value=kernel32, create=True):
                self.assertFalse(validator.read_native_process_oracle()["native_arm64_ok"])

    def test_baseline_records_drift_but_still_requires_packages(self):
        with tempfile.TemporaryDirectory() as root:
            lock_path = os.path.join(root, "packages.lock")
            with open(lock_path, "w") as stream:
                stream.write("example-package=1.2.3,UNVERIFIED-NO-SIGNED-SNAPSHOT\n")
            completed = mock.Mock(returncode=0, stdout="example-package 1.2.4\n")
            with mock.patch.object(validator.subprocess, "run", return_value=completed):
                verified, failures = validator.verify_package_lock(lock_path, require_exact_versions=False)
            self.assertEqual(failures, [])
            self.assertFalse(verified[0]["version_matches"])
            completed.returncode = 1
            with mock.patch.object(validator.subprocess, "run", return_value=completed):
                _, failures = validator.verify_package_lock(lock_path, require_exact_versions=False)
            self.assertIn("Package not installed", failures[0])

    def test_package_lock_reports_missing_file(self):
        verified, failures = validator.verify_package_lock("missing-package-lock.txt")
        self.assertEqual(verified, [])
        self.assertIn("Unable to read package lock", failures[0])

    def test_payload_scan_accepts_arm64_and_rejects_amd64(self):
        with tempfile.TemporaryDirectory() as root:
            write_pe(os.path.join(root, "native.dll"), validator.IMAGE_FILE_MACHINE_ARM64)
            results, failures = validator.scan_payload_architecture(root)
            self.assertEqual(len(results), 1)
            self.assertEqual(failures, [])

            write_pe(os.path.join(root, "foreign.pyd"), validator.IMAGE_FILE_MACHINE_AMD64)
            results, failures = validator.scan_payload_architecture(root)
            self.assertEqual(len(results), 2)
            self.assertEqual(len(failures), 1)

    def test_required_native_host_fails_closed(self):
        oracle = {
            "checked": True,
            "process_machine": validator.IMAGE_FILE_MACHINE_UNKNOWN,
            "native_machine": validator.IMAGE_FILE_MACHINE_AMD64,
            "is_wow_or_emulated": False,
            "native_arm64_ok": False,
            "reason": None,
        }
        with mock.patch.object(validator, "read_native_process_oracle", return_value=oracle):
            with mock.patch.object(sys, "argv", ["validator", "--require-native-arm64"]):
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(validator.main(), 1)

    def test_package_lock_detects_version_drift(self):
        with tempfile.NamedTemporaryFile("w", delete=False) as lock:
            lock.write("example-package=1.2.3,UNVERIFIED-NO-SIGNED-SNAPSHOT\n")
            lock_path = lock.name
        try:
            completed = mock.Mock(returncode=0, stdout="example-package 1.2.4\n")
            with mock.patch.object(validator.subprocess, "run", return_value=completed):
                verified, failures = validator.verify_package_lock(lock_path)
            self.assertEqual(verified[0]["version"], "1.2.4")
            self.assertEqual(len(failures), 1)
        finally:
            os.unlink(lock_path)

    def test_package_lock_reports_missing_pacman(self):
        with tempfile.NamedTemporaryFile("w", delete=False) as lock:
            lock.write("example-package=1.2.3,UNVERIFIED-NO-SIGNED-SNAPSHOT\n")
            lock_path = lock.name
        try:
            with mock.patch.object(
                validator.subprocess, "run", side_effect=FileNotFoundError("pacman")
            ):
                verified, failures = validator.verify_package_lock(lock_path)
            self.assertEqual(verified, [])
            self.assertIn("Unable to run pacman", failures[0])
        finally:
            os.unlink(lock_path)

    def test_package_lock_reports_malformed_entry(self):
        with tempfile.NamedTemporaryFile("w", delete=False) as lock:
            lock.write("malformed-entry\n")
            lock_path = lock.name
        try:
            verified, failures = validator.verify_package_lock(lock_path)
            self.assertEqual(verified, [])
            self.assertEqual(failures, ["Malformed package lock entry: malformed-entry"])
        finally:
            os.unlink(lock_path)

    def test_package_lock_rejects_unsafe_package_name(self):
        with tempfile.NamedTemporaryFile("w", delete=False) as lock:
            lock.write("-unsafe=1.2.3,UNVERIFIED-NO-SIGNED-SNAPSHOT\n")
            lock_path = lock.name
        try:
            verified, failures = validator.verify_package_lock(lock_path)
            self.assertEqual(verified, [])
            self.assertEqual(failures, ["Invalid package name in lock: '-unsafe'"])
        finally:
            os.unlink(lock_path)

    def test_package_lock_rejects_unexpected_pacman_output(self):
        with tempfile.NamedTemporaryFile("w", delete=False) as lock:
            lock.write("example-package=1.2.3,UNVERIFIED-NO-SIGNED-SNAPSHOT\n")
            lock_path = lock.name
        try:
            completed = mock.Mock(returncode=0, stdout="warning only\n")
            with mock.patch.object(validator.subprocess, "run", return_value=completed):
                verified, failures = validator.verify_package_lock(lock_path)
            self.assertEqual(verified, [])
            self.assertIn("Unexpected pacman output", failures[0])
        finally:
            os.unlink(lock_path)

    def test_package_lock_rejects_unverified_real_hash(self):
        with tempfile.NamedTemporaryFile("w", delete=False) as lock:
            lock.write("example-package=1.2.3,abc123\n")
            lock_path = lock.name
        try:
            completed = mock.Mock(returncode=0, stdout="example-package 1.2.3\n")
            with mock.patch.object(validator.subprocess, "run", return_value=completed):
                _verified, failures = validator.verify_package_lock(lock_path)
            self.assertIn("hash verification is not implemented", failures[0])
        finally:
            os.unlink(lock_path)

    def test_require_payload_rejects_missing_root(self):
        with mock.patch.object(sys, "argv", ["validator", "--require-payload"]):
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(validator.main(), 1)

    def test_require_payload_rejects_directory_without_valid_pe(self):
        with tempfile.TemporaryDirectory() as root:
            with open(os.path.join(root, "stub.exe"), "w", encoding="utf-8") as stream:
                stream.write("not a PE")
            with mock.patch.object(
                sys, "argv", ["validator", "--payload-root", root, "--require-payload"]
            ):
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(validator.main(), 1)


if __name__ == "__main__":
    unittest.main()
