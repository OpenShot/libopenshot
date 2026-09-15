"""Exercise native logger configuration in fresh processes through SWIG."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class NativeLoggerTests(unittest.TestCase):
    def run_logger(self, variables, body=''):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'native-é.log'
            env = {key: value for key, value in os.environ.items()
                   if not key.startswith(('OPENSHOT_LOG_', 'LIBOPENSHOT_LOG_'))
                   and key != 'LIBOPENSHOT_DEBUG'}
            env.update(variables, LIBOPENSHOT_LOG_FILE=str(path))
            module_dir = env.get('OPENSHOT_TEST_MODULE_DIR')
            if module_dir:
                env['PYTHONPATH'] = module_dir + os.pathsep + env.get('PYTHONPATH', '')
            result = subprocess.run([sys.executable, '-c', '''
import os
_dll_handles = []
if os.name == 'nt' and hasattr(os, 'add_dll_directory'):
    # Python 3.8+ does not search PATH for extension-module dependencies.
    # Include the project DLLs and the dependency directories supplied by CI.
    _dll_dirs = [os.environ.get('OPENSHOT_TEST_DLL_DIR', ''),
                 os.environ.get('OPENSHOT_TEST_AUDIO_DLL_DIR', '')]
    _dll_dirs.extend(os.environ.get('PATH', '').split(os.pathsep))
    _seen = set()
    for _directory in _dll_dirs:
        _directory = _directory.strip('"')
        if not _directory or not os.path.isdir(_directory):
            continue
        _directory = os.path.abspath(_directory)
        _key = os.path.normcase(_directory)
        if _key not in _seen:
            _dll_handles.append(os.add_dll_directory(_directory))
            _seen.add(_key)
import openshot
logger = openshot.Logger.Instance()
assert openshot.ZmqLogger is openshot.Logger
''' + body + '''
logger.Log("native-debug-record", openshot.Logger.LevelDebug)
logger.Log("native-info-record", openshot.Logger.LevelInfo)
logger.Log("native-warning-record", openshot.Logger.LevelWarning)
logger.Log("native-error-record", openshot.Logger.LevelError)
logger.Log("native-critical-record", openshot.Logger.LevelCritical)
logger.Close()
'''], env=env, text=True, capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stderr)
            return path.read_text(), result.stderr

    def test_default_and_legacy_console(self):
        for variables, debug_console in (({}, False), ({'LIBOPENSHOT_DEBUG': '0'}, True)):
            with self.subTest(variables=variables):
                file, console = self.run_logger(variables)
                self.assertNotIn('native-debug-record', file)
                self.assertEqual('native-debug-record' in console, debug_console)
                self.assertIn('native-info-record', file)
                self.assertIn('native-info-record', console)

    def test_component_and_destination_precedence(self):
        file, console = self.run_logger({
            'OPENSHOT_LOG_FILE_LEVEL': 'off',
            'LIBOPENSHOT_LOG_LEVEL': 'debug',
            'LIBOPENSHOT_LOG_CONSOLE_LEVEL': 'error'})
        self.assertIn('native-debug-record', file)
        self.assertNotIn('native-info-record', console)
        self.assertIn('native-error-record', console)

    def test_invalid_level_fallback(self):
        file, console = self.run_logger({
            'OPENSHOT_LOG_LEVEL': 'error',
            'LIBOPENSHOT_LOG_FILE_LEVEL': 'invalid',
            'LIBOPENSHOT_DEBUG': '1'})
        self.assertNotIn('native-info-record', file)
        self.assertNotIn('native-debug-record', console)
        self.assertIn('native-error-record', file)
        self.assertEqual(console.count('ignoring invalid'), 1)

    def test_every_environment_variable_filters_the_correct_output(self):
        for prefix in ('OPENSHOT', 'LIBOPENSHOT'):
            for suffix, debug_file, debug_console in (
                    ('LEVEL', True, True), ('FILE_LEVEL', True, False),
                    ('CONSOLE_LEVEL', False, True)):
                variable = prefix + '_LOG_' + suffix
                with self.subTest(variable=variable):
                    file, console = self.run_logger({variable: 'debug'})
                    self.assertEqual('native-debug-record' in file, debug_file)
                    self.assertEqual('native-debug-record' in console, debug_console)

    def test_all_level_thresholds(self):
        levels = ('debug', 'info', 'warning', 'error', 'critical', 'off')
        for threshold, level in enumerate(levels):
            with self.subTest(level=level):
                file, console = self.run_logger({'LIBOPENSHOT_LOG_LEVEL': level})
                for index, message_level in enumerate(levels[:-1]):
                    message = 'native-' + message_level + '-record'
                    self.assertEqual(message in file, index >= threshold)
                    self.assertEqual(message in console, index >= threshold)

    def test_api_overrides_environment_and_preserves_crash_output(self):
        file, console = self.run_logger({'LIBOPENSHOT_LOG_LEVEL': 'off'}, '''
logger.SetFileLevel("debug")
logger.SetConsoleLevel("error")
logger.LogToFile("---- Unhandled Exception: Stack Trace ----\\ncrash-evidence\\n---- End of Stack Trace ----\\n")
try:
    logger.SetFileLevel("invalid")
except RuntimeError:
    pass
else:
    raise AssertionError("invalid level accepted")
''')
        self.assertIn('native-debug-record', file)
        self.assertIn('crash-evidence', file)
        self.assertIn('native-error-record', console)
        self.assertNotIn('native-debug-record', console)


if __name__ == '__main__':
    unittest.main()
