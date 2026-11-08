#!/usr/bin/env python3
#   BAREOS® - Backup Archiving REcovery Open Sourced
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
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import MagicMock, patch

spec = importlib.util.spec_from_file_location(
    "webui_selenium", Path(__file__).with_name("webui-php-selenium-test.py")
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class LoginCompatibilityTest(unittest.TestCase):
    def setUp(self):
        self.case = module.SeleniumTest("test_run_default_job")
        self.case.driver = MagicMock()
        self.case.wait = MagicMock()
        self.case.wait.until.side_effect = lambda condition: condition(self.case.driver)
        self.case.is_shared_session_test = MagicMock(return_value=False)
        self.case.enter_input = MagicMock()
        self.case.wait_and_click = MagicMock()
        self.case.wait_for_spinner_absence = MagicMock()

    def run_login(self, url, alert_visible):
        self.case.driver.current_url = url
        alert = MagicMock()
        alert.is_displayed.return_value = alert_visible
        self.case.driver.find_element.return_value = alert
        legacy_conditions = SimpleNamespace(
            presence_of_element_located=module.EC.presence_of_element_located,
            url_contains=module.EC.url_contains,
            visibility_of_element_located=module.EC.visibility_of_element_located,
        )
        with patch.object(module, "WebDriverWait"), patch.object(module, "Select"):
            with patch.object(module, "EC", legacy_conditions):
                self.case.login()

    def test_dashboard_succeeds(self):
        self.run_login("http://example.test/dashboard", False)
        self.case.wait_for_spinner_absence.assert_called_once()

    def test_invalid_credentials_are_reported(self):
        with self.assertRaises(module.WrongCredentialsException):
            self.run_login("http://example.test/auth/login", True)
        self.case.wait_for_spinner_absence.assert_not_called()

    def test_waits_until_login_finishes(self):
        def finish_login(condition):
            self.assertFalse(condition(self.case.driver))
            self.case.driver.current_url = "http://example.test/dashboard"
            return condition(self.case.driver)

        self.case.wait.until.side_effect = finish_login
        self.run_login("http://example.test/auth/login", False)
        self.case.wait_for_spinner_absence.assert_called_once()


if __name__ == "__main__":
    unittest.main()
