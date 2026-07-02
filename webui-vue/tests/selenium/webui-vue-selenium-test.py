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

"""Minimal smoke test for the Bareos Vue WebUI using Selenium.

Environment variables (all optional):
  BAREOS_WEBUI_BASE_URL         Base URL, default: http://127.0.0.1/bareos-webui
  BAREOS_WEBUI_USERNAME         Login username, default: admin
  BAREOS_WEBUI_PASSWORD         Login password, default: admin
  BAREOS_WEBUI_CHROME_HEADLESS  Set to 'no' to show browser window, default: yes
  BAREOS_WEBUI_CHROMEDRIVER_PATH  Path to chromedriver executable
"""

import logging
import os
import sys
import unittest

from selenium import webdriver
from selenium.common.exceptions import TimeoutException, NoSuchElementException
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.chrome.service import Service as ChromeService
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.ui import WebDriverWait


logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
)
logger = logging.getLogger(__name__)

TIMEOUT = 15


class SeleniumVueTest(unittest.TestCase):
    base_url = "http://127.0.0.1/bareos-webui"
    username = "admin"
    password = "admin"
    headless = True
    chromedriverpath = None

    driver = None

    @classmethod
    def setUpClass(cls):
        options = Options()
        if cls.headless:
            options.add_argument("--headless=new")
        options.add_argument("--no-sandbox")
        options.add_argument("--disable-dev-shm-usage")
        options.add_argument("--window-size=1920,1080")

        if cls.chromedriverpath:
            service = ChromeService(executable_path=cls.chromedriverpath)
            cls.driver = webdriver.Chrome(service=service, options=options)
        else:
            cls.driver = webdriver.Chrome(options=options)

        cls.driver.implicitly_wait(TIMEOUT)

    @classmethod
    def tearDownClass(cls):
        if cls.driver:
            cls.driver.close()

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def wait_for(self, by, value, timeout=TIMEOUT):
        return WebDriverWait(self.driver, timeout).until(
            EC.visibility_of_element_located((by, value))
        )

    def wait_for_url(self, pattern, timeout=TIMEOUT):
        WebDriverWait(self.driver, timeout).until(EC.url_contains(pattern))

    def by_testid(self, testid):
        return self.driver.find_element(By.CSS_SELECTOR, f'[data-testid="{testid}"]')

    def login(self, username=None, password=None, expect_success=True):
        username = username or self.username
        password = password or self.password

        self.driver.get(self.base_url + "/")
        self.wait_for(By.CSS_SELECTOR, '[data-testid="login-form"]')

        # Labels associate via for/id; find by input type as fallback
        self.driver.find_element(By.CSS_SELECTOR, 'input[type="text"]').clear()
        self.driver.find_element(By.CSS_SELECTOR, 'input[type="text"]').send_keys(username)
        self.driver.find_element(By.CSS_SELECTOR, 'input[type="password"]').clear()
        self.driver.find_element(By.CSS_SELECTOR, 'input[type="password"]').send_keys(password)
        self.driver.find_element(
            By.XPATH, '//button[normalize-space(.)="Login"]'
        ).click()

        if not expect_success:
            self.wait_for(By.CSS_SELECTOR, '[data-testid="login-error"]')
            return

        try:
            self.wait_for_url("#/dashboard")
        except TimeoutException:
            try:
                err = self.by_testid("login-error").text.strip()
            except NoSuchElementException:
                err = "unknown error"
            raise AssertionError(f"Login failed: {err}")

        self.wait_for(By.CSS_SELECTOR, '[data-testid="director-status-label"]')
        status = self.by_testid("director-status-label").text
        self.assertIn("Connected", status)

    # ------------------------------------------------------------------
    # Tests
    # ------------------------------------------------------------------

    def test_login_and_dashboard(self):
        """Login and verify the dashboard is shown."""
        self.login()
        self.wait_for(By.XPATH, '//*[normalize-space(.)="Running Jobs"]')
        self.assertIn(
            "Most recent job status per job name",
            self.driver.page_source,
        )

    def test_login_error_for_wrong_password(self):
        """Wrong credentials must show a login error."""
        self.login(password=self.password + "-wrong", expect_success=False)
        self.assertIn("#/login", self.driver.current_url)
        error_text = self.by_testid("login-error").text.lower()
        self.assertTrue(
            any(
                kw in error_text
                for kw in ("authentication failed", "connection error", "could not log in")
            ),
            f"Unexpected error text: {error_text!r}",
        )

    def test_jobs_page(self):
        """Jobs page loads and lists at least one job."""
        self.login()
        self.by_testid("nav-jobs").click()
        self.wait_for_url("#/jobs")
        self.wait_for(By.CSS_SELECTOR, "tbody tr")

    def test_console_connects(self):
        """Console popup connects to the director."""
        self.login()
        self.driver.get(self.base_url + "/#/console-popup")
        output = self.wait_for(By.CSS_SELECTOR, '[data-testid="console-output"]')
        WebDriverWait(self.driver, TIMEOUT).until(
            lambda d: "Connected to bareos-dir" in output.text
        )

    def test_restore_page_loads(self):
        """Restore page is reachable and shows source client selector."""
        self.login()
        self.by_testid("nav-restore").click()
        self.wait_for_url("#/restore")
        self.wait_for(
            By.CSS_SELECTOR, '[data-testid="restore-source-client"]', timeout=20
        )


def configure_from_env():
    base_url = os.environ.get("BAREOS_WEBUI_BASE_URL")
    if base_url:
        SeleniumVueTest.base_url = base_url.rstrip("/")

    username = os.environ.get("BAREOS_WEBUI_USERNAME")
    if username:
        SeleniumVueTest.username = username

    password = os.environ.get("BAREOS_WEBUI_PASSWORD")
    if password:
        SeleniumVueTest.password = password

    headless_env = os.environ.get("BAREOS_WEBUI_CHROME_HEADLESS", "yes")
    SeleniumVueTest.headless = headless_env.lower() not in {"no", "false", "0", "off"}

    chromedriverpath = os.environ.get("BAREOS_WEBUI_CHROMEDRIVER_PATH")
    if chromedriverpath:
        SeleniumVueTest.chromedriverpath = chromedriverpath


if __name__ == "__main__":
    configure_from_env()
    unittest.main(verbosity=2)
