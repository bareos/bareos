#!/usr/bin/env python3
#
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.

import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from selenium_screenshots import ScreenshotResult


class ScreenshotResultTest(unittest.TestCase):
    def test_packaged_scripts_start_without_source_tree(self):
        root = Path(__file__).resolve().parents[3]
        helper = root / "systemtests/scripts/selenium_screenshots.py"
        for script in (
            root / "webui/tests/selenium/webui-php-selenium-test.py",
            root / "webui-vue/tests/selenium/webui-selenium-test.py",
        ):
            with self.subTest(script=script.name):
                with tempfile.TemporaryDirectory() as directory:
                    shutil.copy2(script, directory)
                    shutil.copy2(helper, directory)
                    environment = dict(os.environ)
                    environment.pop("PYTHONPATH", None)
                    result = subprocess.run(
                        [sys.executable, str(Path(directory) / script.name), "--help"],
                        cwd=directory,
                        env=environment,
                        capture_output=True,
                        text=True,
                        timeout=15,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn("usage:", result.stdout.lower())

    def test_failures_and_errors_capture_before_browser_teardown(self):
        class BrowserCase(unittest.TestCase):
            def setUp(self):
                self.driver = self

            def save_screenshot(self, path):
                Path(path).write_bytes(b"screenshot")
                return True

            def tearDown(self):
                self.driver = None

            def test_assertion(self):
                self.fail("assertion failed")

            def test_exception(self):
                raise ValueError("unexpected error")

            def test_success(self):
                pass

            def test_subtests(self):
                for value in (1, 2):
                    with self.subTest(value=value):
                        self.fail("subtest failed")

        with tempfile.TemporaryDirectory() as directory:
            output = io.StringIO()
            with patch.dict(
                os.environ,
                {"BAREOS_BROWSER_ARTIFACTS_DIR": directory, "TestName": "admin"},
            ):
                result = unittest.TextTestRunner(
                    stream=output, resultclass=ScreenshotResult
                ).run(unittest.defaultTestLoader.loadTestsFromTestCase(BrowserCase))

            self.assertEqual(len(result.failures), 3)
            self.assertEqual(len(result.errors), 1)
            screenshots = sorted(Path(directory).glob("selenium/admin/*.png"))
            self.assertEqual(len(screenshots), 4)
            self.assertTrue(
                all(path.read_bytes() == b"screenshot" for path in screenshots)
            )
            self.assertIn("Screenshot saved:", output.getvalue())

    def test_screenshot_failure_preserves_original_test_failure(self):
        class BrowserCase(unittest.TestCase):
            driver = None

            def test_failure(self):
                self.fail("original failure")

        output = io.StringIO()
        result = unittest.TextTestRunner(
            stream=output, resultclass=ScreenshotResult
        ).run(unittest.defaultTestLoader.loadTestsFromTestCase(BrowserCase))
        self.assertEqual(len(result.failures), 1)
        self.assertIn("original failure", output.getvalue())
        self.assertIn("Screenshot unavailable", output.getvalue())

    def test_screenshot_write_failure_preserves_original_test_failure(self):
        class BrowserCase(unittest.TestCase):
            driver = None

            def setUp(self):
                self.driver = self

            def save_screenshot(self, path):
                raise OSError("disk unavailable")

            def test_failure(self):
                self.fail("original failure")

        with tempfile.TemporaryDirectory() as directory:
            output = io.StringIO()
            with patch.dict(os.environ, {"BAREOS_BROWSER_ARTIFACTS_DIR": directory}):
                result = unittest.TextTestRunner(
                    stream=output, resultclass=ScreenshotResult
                ).run(unittest.defaultTestLoader.loadTestsFromTestCase(BrowserCase))
            self.assertEqual(len(result.failures), 1)
            self.assertEqual(len(result.errors), 0)
            self.assertIn("original failure", output.getvalue())
            self.assertIn("disk unavailable", output.getvalue())


if __name__ == "__main__":
    unittest.main()
