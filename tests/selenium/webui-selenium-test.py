# -*- coding: utf-8 -*-
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.support.ui import Select
from selenium.common.exceptions import NoSuchElementException
from selenium.common.exceptions import NoAlertPresentException
import unittest, time, re, sys, os
from selenium.webdriver.common.desired_capabilities import DesiredCapabilities   


class Bareos(unittest.TestCase):
    def setUp(self):
        d = DesiredCapabilities.FIREFOX
        d['loggingPrefs'] = { 'browser':'ALL' }
        fp = webdriver.FirefoxProfile()
        fp.set_preference('webdriver.log.file', '/tmp/firefox_console')
        self.driver = webdriver.Firefox(capabilities=d,firefox_profile=fp)
        self.driver.implicitly_wait(30)
        self.base_url = "http://%s" % targethost
        self.verificationErrors = []
        self.accept_next_alert = True

    def test_bareos(self):
        driver = self.driver
        # on windows we have a different baseurl
        if os.getenv('DIST') == "windows":
            driver.get(self.base_url + "/")
        else:
            driver.get(self.base_url + "/bareos-webui/")
        Select(driver.find_element_by_name("director")).select_by_visible_text("localhost-dir")
        driver.find_element_by_name("consolename").clear()
        driver.find_element_by_name("consolename").send_keys(username)
        driver.find_element_by_name("password").clear()
        driver.find_element_by_name("password").send_keys(password)
        driver.find_element_by_id("submit").click()
        driver.find_element_by_link_text("Director").click()
        driver.find_element_by_link_text("Messages").click()
        driver.find_element_by_link_text("Schedules").click()
        driver.find_element_by_link_text("Scheduler status").click()
        driver.find_element_by_link_text("Storages").click()
        driver.find_element_by_link_text("Clients").click()
        driver.find_element_by_link_text("Restore").click()
        driver.find_element_by_partial_link_text(username).click()
        driver.find_element_by_link_text("Logout").click()

    def is_element_present(self, how, what):
        try: self.driver.find_element(by=how, value=what)
        except NoSuchElementException, e: return False
        return True

    def is_alert_present(self):
        try: self.driver.switch_to_alert()
        except NoAlertPresentException, e: return False
        return True

    def close_alert_and_get_its_text(self):
        try:
            alert = self.driver.switch_to_alert()
            alert_text = alert.text
            if self.accept_next_alert:
                alert.accept()
            else:
                alert.dismiss()
            return alert_text
        finally: self.accept_next_alert = True

    def tearDown(self):
        self.driver.quit()
        self.assertEqual([], self.verificationErrors)

if __name__ == "__main__":

    # get username from environment if set
    # otherwise use defaults
    username = os.environ.get('USERNAME')
    password = os.environ.get('PASSWORD')
    if not username:
        username = "citest"
    if not password:
        password = "citestpass"

    targethost = os.environ.get('VM_IP')
    unittest.main()
