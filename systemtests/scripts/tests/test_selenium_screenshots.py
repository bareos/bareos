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
import textwrap
import unittest
from unittest.mock import patch

from urllib3.exceptions import MaxRetryError

from selenium_screenshots import ScreenshotResult, ScreenshotTestCase


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
                    probe = textwrap.dedent("""
                        import runpy
                        import sys
                        from pathlib import Path
                        import unittest
                        from selenium_screenshots import ScreenshotRunner

                        module = runpy.run_path(sys.argv[1])
                        base = module.get("SeleniumTest", module.get("SeleniumVueTest"))

                        class BrowserCase(base):
                            @classmethod
                            def setUpClass(cls):
                                pass

                            @classmethod
                            def tearDownClass(cls):
                                pass

                            def setUp(self):
                                self.driver = self

                            def save_screenshot(self, path):
                                Path(path).write_bytes(b"screenshot")
                                return True

                            def tearDown(self):
                                self.driver = None

                            def test_failure(self):
                                self.fail("packaged failure")

                        result = ScreenshotRunner().run(
                            unittest.TestSuite([BrowserCase("test_failure")])
                        )
                        assert len(result.failures) == 1
                        assert not result.errors
                        assert len(list(Path(".").glob("selenium/**/*.png"))) == 1
                        """)
                    result = subprocess.run(
                        [sys.executable, "-c", probe, script.name],
                        cwd=directory,
                        env=environment,
                        capture_output=True,
                        text=True,
                        timeout=15,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn("Screenshot saved:", result.stderr)

    def test_failures_and_errors_capture_before_browser_teardown(self):
        events = []

        class BrowserCase(ScreenshotTestCase):
            def setUp(self):
                self.driver = self
                self.addCleanup(self.close_browser)
                if self._testMethodName == "test_cleanup_error":
                    self.addCleanup(self.fail_cleanup)
                if self._testMethodName == "test_setup_error":
                    raise ValueError("setup failed")

            def save_screenshot(self, path):
                events.append(("screenshot", self._testMethodName))
                Path(path).write_bytes(b"screenshot")
                return True

            def tearDown(self):
                events.append(("tearDown", self._testMethodName))
                if self._testMethodName == "test_teardown_error":
                    raise ValueError("teardown failed")
                if self._testMethodName == "test_cleanup_error":
                    return
                self.driver = None

            def fail_cleanup(self):
                raise ValueError("cleanup failed")

            def close_browser(self):
                events.append(("cleanup", self._testMethodName))
                self.driver = None

            def test_assertion(self):
                self.fail("assertion failed")

            def test_exception(self):
                raise ValueError("unexpected error")

            def test_success(self):
                pass

            def test_setup_error(self):
                self.fail("setUp should have failed")

            def test_teardown_error(self):
                pass

            def test_cleanup_error(self):
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
            self.assertEqual(len(result.errors), 4)
            screenshots = sorted(Path(directory).glob("selenium/admin/*.png"))
            self.assertEqual(len(screenshots), 7)
            self.assertTrue(
                all(path.read_bytes() == b"screenshot" for path in screenshots)
            )
            self.assertIn("Screenshot saved:", output.getvalue())
            for method in (
                "test_assertion",
                "test_exception",
                "test_setup_error",
                "test_subtests",
            ):
                with self.subTest(method=method):
                    capture = events.index(("screenshot", method))
                    self.assertLess(capture, events.index(("cleanup", method)))
                    if method != "test_setup_error":
                        self.assertLess(capture, events.index(("tearDown", method)))

    def test_screenshot_failure_preserves_original_test_failure(self):
        class BrowserCase(ScreenshotTestCase):
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
        class BrowserCase(ScreenshotTestCase):
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

    def test_disconnected_browser_preserves_failure_and_continues(self):
        completed = []

        class BrowserCase(ScreenshotTestCase):
            def setUp(self):
                self.driver = self

            def save_screenshot(self, path):
                raise MaxRetryError(None, "/screenshot", "browser disconnected")

            def test_failure(self):
                self.fail("original failure")

            def test_remaining(self):
                completed.append("remaining test")

        with tempfile.TemporaryDirectory() as directory:
            output = io.StringIO()
            with patch.dict(os.environ, {"BAREOS_BROWSER_ARTIFACTS_DIR": directory}):
                result = unittest.TextTestRunner(
                    stream=output, resultclass=ScreenshotResult
                ).run(unittest.defaultTestLoader.loadTestsFromTestCase(BrowserCase))

            self.assertEqual(len(result.failures), 1)
            self.assertEqual(len(result.errors), 0)
            self.assertEqual(result.testsRun, 2)
            self.assertEqual(completed, ["remaining test"])
            self.assertIn("original failure", output.getvalue())
            self.assertIn("Screenshot unavailable", output.getvalue())
            self.assertIn("browser disconnected", output.getvalue())

    def test_subtests_preserve_failfast_skips_and_expected_failures(self):
        class BrowserCase(ScreenshotTestCase):
            def setUp(self):
                self.driver = self

            def save_screenshot(self, path):
                Path(path).write_bytes(b"screenshot")
                return True

            @unittest.expectedFailure
            def test_expected_failure(self):
                self.fail("expected failure")

            def test_skip(self):
                self.skipTest("skip browser action")

            def test_subtests(self):
                with self.subTest(group="outer"):
                    with self.subTest(value=1):
                        self.fail("subtest assertion")
                    with self.subTest(value=2):
                        raise ValueError("subtest error")

        for failfast in (False, True):
            with self.subTest(failfast=failfast):
                with tempfile.TemporaryDirectory() as directory:
                    output = io.StringIO()
                    with patch.dict(
                        os.environ, {"BAREOS_BROWSER_ARTIFACTS_DIR": directory}
                    ):
                        result = unittest.TextTestRunner(
                            stream=output,
                            resultclass=ScreenshotResult,
                            failfast=failfast,
                        ).run(
                            unittest.defaultTestLoader.loadTestsFromTestCase(
                                BrowserCase
                            )
                        )

                    self.assertEqual(len(result.failures), 1)
                    self.assertEqual(len(result.errors), 0 if failfast else 1)
                    self.assertEqual(len(result.skipped), 1)
                    self.assertEqual(len(result.expectedFailures), 1)
                    self.assertEqual(
                        len(list(Path(directory).glob("selenium/**/*.png"))),
                        1 if failfast else 2,
                    )
                    self.assertIn("value=1", result.failures[0][0].id())
                    self.assertNotIn("(None)", result.failures[0][0].id())


if __name__ == "__main__":
    unittest.main()
