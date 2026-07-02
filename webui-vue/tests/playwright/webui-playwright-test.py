#!/usr/bin/env python3
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

"""Minimal smoke test for the Bareos Vue WebUI using Playwright (Python).

Environment variables (all optional):
  BAREOS_WEBUI_BASE_URL      Base URL, default: http://127.0.0.1/bareos-webui
  BAREOS_WEBUI_USERNAME      Login username, default: admin
  BAREOS_WEBUI_PASSWORD      Login password, default: admin
  BAREOS_WEBUI_PROFILE       'admin' or 'readonly', default: admin
  BAREOS_WEBUI_CHROME_HEADLESS  Set to 'no' to show the browser, default: yes
  BAREOS_WEBUI_SLOW_MO       Milliseconds to slow down actions, default: 0
"""

import logging
import os
import re
import sys
import unittest

from playwright.sync_api import TimeoutError as PlaywrightTimeoutError
from playwright.sync_api import sync_playwright, expect


logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
)
logger = logging.getLogger(__name__)


class PlaywrightTest(unittest.TestCase):
    base_url = "http://127.0.0.1/bareos-webui"
    username = "admin"
    password = "admin"
    profile = "admin"
    headless = True
    slow_mo = 0

    _playwright = None
    _browser = None

    @classmethod
    def setUpClass(cls):
        cls._playwright = sync_playwright().start()
        cls._browser = cls._playwright.chromium.launch(
            headless=cls.headless,
            slow_mo=cls.slow_mo,
            args=["--no-sandbox"],
        )

    @classmethod
    def tearDownClass(cls):
        cls._browser.close()
        cls._playwright.stop()

    def setUp(self):
        self.page = self._browser.new_page(base_url=self.base_url)
        self.page.set_default_timeout(15000)

    def tearDown(self):
        self.page.close()

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def login(self, username=None, password=None, expect_success=True):
        username = username or self.username
        password = password or self.password
        page = self.page

        page.goto("/")
        expect(page.locator('[data-testid="login-form"]')).to_be_visible()
        page.get_by_label("Username").fill(username)
        page.get_by_label("Password").fill(password)
        page.get_by_role("button", name="Login").click()

        if not expect_success:
            expect(page.locator('[data-testid="login-error"]')).to_be_visible()
            return

        login_error = page.locator('[data-testid="login-error"]')
        try:
            page.wait_for_url(re.compile(r"#/dashboard$"), timeout=15000)
        except PlaywrightTimeoutError:
            if login_error.is_visible():
                msg = login_error.text_content() or "Unknown error"
                raise AssertionError(f"Login failed: {msg.strip()}")
            raise

        expect(
            page.locator('[data-testid="director-status-label"]')
        ).to_contain_text("Connected")

    # ------------------------------------------------------------------
    # Tests
    # ------------------------------------------------------------------

    def test_login_and_dashboard(self):
        """Login and verify the dashboard is shown."""
        self.login()
        page = self.page

        expect(page.get_by_text("Running Jobs", exact=True)).to_be_visible()
        expect(
            page.locator(".q-card").filter(
                has_text="Most recent job status per job name"
            )
        ).to_be_visible()

    def test_login_error_for_wrong_password(self):
        """Wrong credentials must show a login error."""
        self.login(password=self.password + "-wrong", expect_success=False)
        expect(page := self.page).to_have_url(re.compile(r"#/login$"))
        expect(
            page.locator('[data-testid="login-error"]')
        ).to_contain_text(
            re.compile(
                r"Authentication failed|Connection error|Could not log in",
                re.IGNORECASE,
            )
        )

    def test_jobs_page(self):
        """Jobs page loads and lists at least one job."""
        self.login()
        self.page.locator('[data-testid="nav-jobs"]').click()
        self.page.wait_for_url(re.compile(r"#/jobs"))
        expect(self.page.locator("tbody tr").first).to_be_visible()

    def test_console_connects(self):
        """Console popup connects to the director."""
        self.login()
        self.page.goto("/#/console-popup")
        expect(
            self.page.locator('[data-testid="console-output"]')
        ).to_contain_text("Connected to bareos-dir")

    def test_restore_page_loads(self):
        """Restore page is reachable and shows source client selector."""
        self.login()
        self.page.locator('[data-testid="nav-restore"]').click()
        self.page.wait_for_url(re.compile(r"#/restore"))
        expect(
            self.page.locator('[data-testid="restore-source-client"]')
        ).to_be_visible(timeout=20000)


def configure_from_env():
    """Apply environment variable overrides to PlaywrightTest class defaults."""
    base_url = os.environ.get("BAREOS_WEBUI_BASE_URL")
    if base_url:
        PlaywrightTest.base_url = base_url.rstrip("/")

    username = os.environ.get("BAREOS_WEBUI_USERNAME")
    if username:
        PlaywrightTest.username = username

    password = os.environ.get("BAREOS_WEBUI_PASSWORD")
    if password:
        PlaywrightTest.password = password

    profile = os.environ.get("BAREOS_WEBUI_PROFILE")
    if profile:
        PlaywrightTest.profile = profile

    headless_env = os.environ.get("BAREOS_WEBUI_CHROME_HEADLESS", "yes")
    PlaywrightTest.headless = headless_env.lower() not in {"no", "false", "0", "off"}

    slow_mo = os.environ.get("BAREOS_WEBUI_SLOW_MO")
    if slow_mo:
        try:
            PlaywrightTest.slow_mo = int(slow_mo)
        except ValueError:
            logger.warning("BAREOS_WEBUI_SLOW_MO must be an integer, ignoring.")


if __name__ == "__main__":
    configure_from_env()
    unittest.main(verbosity=2)
