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
import shutil
import subprocess
import tempfile
import unittest

SOURCE_ROOT = Path(__file__).resolve().parents[3]
RUNNER = SOURCE_ROOT / "systemtests/tests/webui-vue-common/testrunner"


class PlaywrightArtifactsTest(unittest.TestCase):
    def test_failed_browser_artifact_survives_test_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile = root / "admin-smoke"
            profile.mkdir()
            temporary = profile / "tmp"
            temporary.mkdir()
            (temporary / "webui-vue-proxy.out").touch()
            (temporary / "webui-vue-frontend.out").touch()
            scripts = root / "scripts"
            scripts.mkdir()
            (scripts / "functions").write_text(
                "start_test() { :; }\n"
                'set_error() { echo "$1"; failed=1; }\n'
                'end_test() { exit "${failed:-0}"; }\n'
            )
            (root / "webui-vue").mkdir()
            playwright = root / "playwright"
            playwright.write_text(
                '#!/bin/sh\nmkdir -p "$PLAYWRIGHT_ARTIFACTS_DIR"\n'
                'printf screenshot > "$PLAYWRIGHT_ARTIFACTS_DIR/failure.png"\n'
                "exit 1\n"
            )
            playwright.chmod(0o755)
            (profile / "environment").write_text(
                f'tmp="{temporary}"\n'
                f'BAREOS_SCRIPTS_DIR="{scripts}"\n'
                f'CMAKE_SOURCE_DIR="{root}"\n'
                f'BackupDirectory="{temporary}"\n'
                f'WEBUI_VUE_PLAYWRIGHT_EXECUTABLE="{playwright}"\n'
                'BAREOS_WEBUI_BASE_URL="http://example.invalid"\n'
                'BAREOS_WEBUI_RESTORE_TEST_DESTINATION=""\n'
                'BAREOS_WEBUI_USERNAME="test"\nBAREOS_WEBUI_PASSWORD="test"\n'
                'GOOGLE_CHROME_EXECUTABLE="chrome"\n'
                'BAREOS_WEBUI_EXPECTED_DIRECTOR_TRANSPORT=""\n'
            )
            artifacts = root / "artifacts"
            result = subprocess.run(
                ["bash", str(RUNNER)],
                cwd=profile,
                env={**os.environ, "BAREOS_BROWSER_ARTIFACTS_DIR": str(artifacts)},
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            shutil.rmtree(temporary)
            screenshot = artifacts / "playwright/admin-smoke/failure.png"
            self.assertEqual(screenshot.read_bytes(), b"screenshot")


if __name__ == "__main__":
    unittest.main()
