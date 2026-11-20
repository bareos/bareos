#!/usr/bin/env python3
#
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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
#   along with this program; if not, write to the Free Software Foundation,
#   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

import importlib.util
from pathlib import Path
import shutil
import unittest
from unittest.mock import Mock, patch
from urllib.parse import quote

from selenium import webdriver
from selenium.common.exceptions import TimeoutException, WebDriverException
from selenium.webdriver.chrome.service import Service
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait

SPEC = importlib.util.spec_from_file_location(
    "webui_php_selenium", Path(__file__).with_name("webui-php-selenium-test.py")
)
WEBUI = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WEBUI)


@unittest.skipUnless(shutil.which("chromedriver"), "requires Chrome and chromedriver")
class LoginHelperTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        options = webdriver.ChromeOptions()
        options.add_argument("--headless=new")
        options.add_argument("--disable-dev-shm-usage")
        cls.driver = webdriver.Chrome(
            service=Service(shutil.which("chromedriver")), options=options
        )
        cls.addClassCleanup(cls.driver.quit)

    def setUp(self):
        WEBUI.SeleniumTest.shared_session_logged_in = False
        self.addCleanup(setattr, WEBUI.SeleniumTest, "shared_session_logged_in", False)
        self.case = WEBUI.SeleniumTest("test_job_list_page")
        self.case.driver = self.driver
        self.case.username = "fixture-user"
        self.case.password = "fixture-password"
        self.case.is_shared_session_test = Mock(return_value=True)
        self.case.maxwait = 0.3
        self.case.wait = WebDriverWait(self.driver, 0.3, poll_frequency=0.05)
        self.case.enter_input = Mock()
        self.case.wait_and_click = Mock()

    def login(self, *, insecure=True, error=False, authenticated=True):
        page = '<select id="locale"><option>English</option></select>'
        if insecure:
            page += '<div class="alert alert-warning" role="alert">Insecure HTTP</div>'
        if error:
            page += (
                '<div class="col-md-6 alert alert-danger" role="alert">'
                "Invalid credentials</div>"
            )
        if authenticated:
            page += '<a id="menu-topnavbar-dashboard">Dashboard</a>'
        self.driver.get("data:text/html;charset=utf-8," + quote(page))
        # Isolate submission, but use the real browser for login-result detection.
        with patch.object(self.driver, "get"):
            self.case.login()

    def test_successful_login_with_and_without_http_warning(self):
        for insecure in (True, False):
            with self.subTest(insecure=insecure):
                WEBUI.SeleniumTest.shared_session_logged_in = False
                self.login(insecure=insecure)
                self.assertTrue(WEBUI.SeleniumTest.shared_session_logged_in)

    def test_invalid_credentials_with_http_warning(self):
        with self.assertRaises(WEBUI.WrongCredentialsException):
            self.login(error=True, authenticated=False)
        self.assertFalse(WEBUI.SeleniumTest.shared_session_logged_in)

    def test_missing_authenticated_page_is_not_success(self):
        with self.assertRaises(TimeoutException):
            self.login(authenticated=False)
        self.assertFalse(WEBUI.SeleniumTest.shared_session_logged_in)

    def test_browser_errors_are_not_ignored(self):
        find_element = self.driver.find_element

        def find_element_or_fail(by, value):
            if (by, value) == (By.ID, "locale"):
                return find_element(by, value)
            raise WebDriverException("browser disconnected")

        with (
            patch.object(self.driver, "find_element", side_effect=find_element_or_fail),
            patch.object(self.case, "wait_for_spinner_absence"),
            self.assertRaises(WebDriverException),
        ):
            self.login()
        self.assertFalse(WEBUI.SeleniumTest.shared_session_logged_in)


if __name__ == "__main__":
    unittest.main()
