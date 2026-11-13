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

import os
from pathlib import Path
import re
import unittest

from selenium.common.exceptions import WebDriverException


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
        except (OSError, WebDriverException) as error:
            self.stream.writeln(f"Screenshot unavailable for {test.id()}: {error}")
            return
        self.stream.writeln(f"Screenshot saved: {path}")

    def addFailure(self, test, err):
        self.capture_screenshot(test)
        super().addFailure(test, err)

    def addError(self, test, err):
        self.capture_screenshot(test)
        super().addError(test, err)

    def addSubTest(self, test, subtest, err):
        if err is not None:
            self.capture_screenshot(test, subtest.id())
        super().addSubTest(test, subtest, err)


class ScreenshotRunner(unittest.TextTestRunner):
    resultclass = ScreenshotResult
