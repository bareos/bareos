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

from contextlib import contextmanager
import os
from pathlib import Path
import re
import unittest

from selenium.common.exceptions import WebDriverException
from urllib3.exceptions import MaxRetryError


class ScreenshotResult(unittest.TextTestResult):
    def capture_screenshot(self, test, label=None):
        driver = getattr(test, "driver", None)
        if driver is None:
            self.stream.writeln(f"Screenshot unavailable for {test.id()}: no browser")
            return

        root = Path(os.environ.get("BAREOS_BROWSER_ARTIFACTS_DIR", "."))
        profile = os.environ.get("TestName", "standalone")
        filename = re.sub(r"[^a-zA-Z0-9_.-]", "_", label or test.id())
        path = root / "selenium" / profile / f"{filename}-{os.getpid()}.png"
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            if not driver.save_screenshot(str(path)):
                self.stream.writeln(f"Screenshot could not be saved: {path}")
                return
        except (OSError, WebDriverException, MaxRetryError) as error:
            self.stream.writeln(f"Screenshot unavailable for {test.id()}: {error}")
            return
        self.stream.writeln(f"Screenshot saved: {path}")


class ScreenshotRunner(unittest.TextTestRunner):
    resultclass = ScreenshotResult


class ScreenshotTestCase(unittest.TestCase):
    """Capture before unittest handles exceptions and tears down the browser.

    Python 3.10 reports result callbacks only after tearDown and cleanups.
    The setup/test execution hooks and subTest context run before both.
    """

    @contextmanager
    def _capture_errors(self, label=None):
        try:
            yield
        except (unittest.SkipTest, unittest.case._ShouldStop):
            raise
        except Exception:
            outcome = self._outcome
            if (
                outcome is not None
                and not outcome.expecting_failure
                and isinstance(outcome.result, ScreenshotResult)
            ):
                outcome.result.capture_screenshot(self, label)
            raise

    def _callSetUp(self):
        with self._capture_errors():
            super()._callSetUp()

    def _callTestMethod(self, method):
        with self._capture_errors():
            super()._callTestMethod(method)

    def _callTearDown(self):
        with self._capture_errors():
            super()._callTearDown()

    def _callCleanup(self, function, *args, **kwargs):
        with self._capture_errors():
            super()._callCleanup(function, *args, **kwargs)

    @contextmanager
    def subTest(self, *args, **kwargs):
        with super().subTest(*args, **kwargs):
            label = self._subtest.id() if self._subtest is not None else self.id()
            with self._capture_errors(label):
                yield
