#!/usr/bin/env python

# -*- coding: utf-8 -*-

# selenium.common.exceptions.ElementNotInteractableException: requires >= selenium-3.4.0

import logging, os, re, sys, unittest
from   datetime import datetime, timedelta
from   selenium import webdriver
from   selenium.common.exceptions import *
       #WebDriverException, ElementNotInteractableException, ElementNotVisibleException, TimeoutException, NoAlertPresentException, NoSuchElementException
from   selenium.webdriver.common.by import By
from   selenium.webdriver.common.desired_capabilities import DesiredCapabilities
from   selenium.webdriver.common.keys import Keys
from   selenium.webdriver.support import expected_conditions as EC
from   selenium.webdriver.support.ui import Select, WebDriverWait
#from selenium.webdriver.remote.remote_connection import LOGGER
from   time import sleep



class WebuiSeleniumTest(unittest.TestCase):

    browser = 'firefox'
    base_url = "http://127.0.0.1/bareos-webui"
    username = "admin"
    password = "secret"
    client = 'bareos-fd'
    restorefile = '/usr/sbin/bconsole'
    # path to store logging files
    logpath = os.getcwd()
    # slow down test for debugging
    sleeptime = 0.0
    # max seconds to wait for an element
    maxwait = 10
    # time to wait before trying again
    waittime = 0.1


    def setUp(self):
        # Configure the logger, for information about the timings set it to INFO
        # Selenium driver itself will write additional debug messages when set to DEBUG
        #logging.basicConfig(filename='webui-selenium-test.log', level=logging.DEBUG)
        #logging.basicConfig(filename='webui-selenium-test.log', level=logging.INFO)
        logging.basicConfig(
                filename='%s/webui-selenium-test.log' % (self.logpath),
                format='%(levelname)s %(module)s.%(funcName)s: %(message)s',
                level=logging.INFO
        )
        self.logger = logging.getLogger()

        if self.browser == "chrome":
            #d = DesiredCapabilities.CHROME
            #d['loggingPrefs'] = { 'browser':'ALL' }
            self.driver = webdriver.Chrome('/usr/lib/chromium-browser/chromedriver')
        if self.browser == "firefox":
            d = DesiredCapabilities.FIREFOX
            d['loggingPrefs'] = { 'browser':'ALL' }
            fp = webdriver.FirefoxProfile()
            fp.set_preference('webdriver.log.file', self.logpath + '/firefox_console.log')
            self.driver = webdriver.Firefox(capabilities=d, firefox_profile=fp)

        self.driver.implicitly_wait(1)

        # used as timeout for selenium.webdriver.support.expected_conditions (EC)
        self.wait = WebDriverWait(self.driver, self.maxwait)

        # take base url, but remove last /
        self.base_url = base_url.rstrip('/')

        self.verificationErrors = []



    def test_login(self):
        self.login()
        self.logout()



    def test_menue(self):
        driver = self.driver

        self.driver.get(self.base_url + "/")

        self.login()
        self.wait_for_url_and_click('/director/')
        self.wait_for_url_and_click("/schedule/")
        self.wait_for_url_and_click("/schedule/status/")
        self.wait_for_url_and_click("/storage/")
        self.wait_for_url_and_click("/client/")
        self.wait_for_url_and_click("/restore/")
        self.wait_and_click(By.XPATH, "//a[contains(@href, '/dashboard/')]", By.XPATH, "//div[@id='modal-001']//button[.='Close']")
        #self.assertRegexpMatches(self.close_alert_and_get_its_text(), r"^Oops, something went wrong, probably too many files.")
        self.close_alert_and_get_its_text()
        self.logout()



    def test_restore(self):

        # LOGGING IN:
        self.login()

        # CHANGING TO RESTORE TAB:
        self.wait_for_url_and_click("/restore/")
        self.wait_and_click(By.XPATH, "(//button[@data-id='client'])", By.XPATH, "//div[@id='modal-001']//button[.='Close']")
        
        # SELECTING CLIENT:
        # Selects the correct client
        self.wait_and_click(By.LINK_TEXT, self.client)
        
        # FILE-SELECTION:
        # Clicks on file and navigates through the tree
        # by using the arrow-keys.
        pathlist = self.restorefile.split('/')
        for i in pathlist[:-1]:
            self.wait_for_element(By.XPATH, "//a[contains(text(),'%s/')]" % i).send_keys(Keys.ARROW_RIGHT)
        self.wait_for_element(By.XPATH, "//a[contains(text(),'%s')]" % pathlist[-1]).click()

        # CONFIRMATION:
        # Clicks on 'submit'
        self.wait_and_click(By.XPATH, "//input[@id='submit']")
        # Confirms alert that has text "Are you sure ?"
        self.assertRegexpMatches(self.close_alert_and_get_its_text(), r"^Are you sure[\s\S]$")
        # switch to dashboard to prevent that modals are open
        self.wait_and_click(By.XPATH, "//a[contains(@href, '/dashboard/')]", By.XPATH, "//div[@id='modal-002']//button[.='Close']")
        #self.assertRegexpMatches(self.close_alert_and_get_its_text(), r"^Oops, something went wrong, probably too many files.")
        self.close_alert_and_get_its_text()

        # LOGOUT:
        self.logout()


    def login(self):
        driver = self.driver

        driver.get(self.base_url + "/auth/login")
        Select(driver.find_element_by_name("director")).select_by_visible_text("localhost-dir")

        driver.find_element_by_name("consolename").clear()
        driver.find_element_by_name("consolename").send_keys(self.username)
        driver.find_element_by_name("password").clear()
        driver.find_element_by_name("password").send_keys(self.password)
        driver.find_element_by_xpath("(//button[@type='button'])[2]").click()
        driver.find_element_by_link_text("English").click()
        driver.find_element_by_xpath("//input[@id='submit']").click()
        while ("/dashboard/" not in self.driver.current_url):
            sleep(self.waittime)

    def logout(self):

        self.wait_and_click(By.CSS_SELECTOR, "a.dropdown-toggle")
        self.wait_and_click(By.LINK_TEXT, "Logout")
        sleep(self.sleeptime)



    def wait_for_url(self, what):
        value="//a[contains(@href, '%s')]" % what
        return self.wait_for_element(By.XPATH, value)


        
    def wait_for_element(self, by, value, starttime = None):
        logger = logging.getLogger()
        element = None
        #if starttime is None:
             #starttime = datetime.now()
        #seconds = (datetime.now() - starttime).total_seconds()
        #logger.info("waiting for %s %s (%ds)", by, value, seconds)
        #while (seconds < self.maxwait) and (element is None):
            #try:
                #tmp_element = self.driver.find_element(by, value)
                #if tmp_element.is_displayed():
                    #element = tmp_element
            #except ElementNotInteractableException:
                #sleep(self.waittime)
                #logger.info("%s %s not interactable", by, value)
            #except NoSuchElementException:
                #sleep(self.waittime)
                #logger.info("%s %s not existing", by, value)
            #except ElementNotVisibleException:
                #sleep(self.waittime)
                #logger.info("%s %s not visible", by, value)
            #seconds = (datetime.now() - starttime).total_seconds()
        #if element:
            #logger.info("%s %s loaded after %ss." % (by, value, seconds))
            #sleep(self.sleeptime)
        #else:
            #logger.warning("Timeout while loading %s %s (%d s)", by, value, seconds)
        logger.info("waiting for %s %s", by, value)
        element = self.wait.until(EC.element_to_be_clickable((by, value)))
        return element


    def wait_for_url_and_click(self, url):
        logger = logging.getLogger()
        value="//a[contains(@href, '%s')]" % url
        element = self.wait_and_click(By.XPATH, value)
        # wait for page to be loaded
        starttime = datetime.now()
        seconds = 0.0
        while seconds < self.maxwait:
            if (url in self.driver.current_url):
                logger.info("%s is loaded (%d s)", url, seconds)
                return element
            logger.info("waiting for url %s to be loaded", url)
            sleep(self.waittime)
            seconds = (datetime.now() - starttime).total_seconds()
        logger.warning("Timeout while waiting for url %s (%d s)", url, seconds)


    def wait_and_click(self, by, value, modal_by=None, modal_value=None):
        logger = logging.getLogger()
        element = None
        starttime = datetime.now()
        seconds = 0.0
        while seconds < self.maxwait:
            if modal_by and modal_value:
                try:
                    self.driver.find_element(modal_by, modal_value).click()
                except:
                    logger.info("skipped modal: %s %s not found", modal_by, modal_value)
                else:
                    logger.info("closing modal %s %s", modal_by, modal_value)
            logger.info("waiting for %s %s (%ss)", by, value, seconds)
            element = self.wait_for_element(by, value, starttime)
            try:
                element.click()
            except WebDriverException as e:
                #logger.info("WebDriverException while clicking %s %s", by, value)
                logger.info("WebDriverException: %s", e)
                #logger.exception(e)
                sleep(self.waittime)
            else:
                return element
            seconds = (datetime.now() - starttime).total_seconds()
        logger.error("failed to click %s %s", by, value)
        return



    def close_alert_and_get_its_text(self, accept=True):
        logger = logging.getLogger()

        try:
            alert = self.driver.switch_to_alert()
            alert_text = alert.text
        except NoAlertPresentException:
            return

        if accept:
            alert.accept()
        else:
            alert.dismiss()

        logger.debug( 'alert message: %s' % (alert_text))

        return alert_text



    def tearDown(self):
        self.driver.quit()
        self.assertEqual([], self.verificationErrors)
        


if __name__ == "__main__":
    # Get attributes as environment variables,
    # if not available or set use defaults.
    browser = os.environ.get('BAREOS_BROWSER')
    if browser:
        WebuiSeleniumTest.browser = browser

    base_url = os.environ.get('BAREOS_BASE_URL')
    if base_url:
        WebuiSeleniumTest.base_url = base_url.rstrip('/')

    username = os.environ.get('BAREOS_USERNAME')
    if username:
        WebuiSeleniumTest.username = username

    password = os.environ.get('BAREOS_PASSWORD')
    if password:
        WebuiSeleniumTest.password = password

    client = os.environ.get('BAREOS_CLIENT_NAME')
    if client:
        WebuiSeleniumTest.client = client

    restorefile = os.environ.get('BAREOS_RESTOREFILE')
    if restorefile:
        WebuiSeleniumTest.restorefile = restorefile

    logpath = os.environ.get('BAREOS_LOG_PATH')
    if logpath:
        WebuiSeleniumTest.logpath = logpath

    sleeptime = os.environ.get('BAREOS_DELAY')
    if sleeptime:
        WebuiSeleniumTest.sleeptime = float(sleeptime)

    unittest.main()
