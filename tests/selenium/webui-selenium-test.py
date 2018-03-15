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
    base_url = 'http://127.0.0.1/bareos-webui'
    username = 'admin'
    password = 'secret'
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
        self.logger.info("===================== TESTING =====================")
        if self.browser == 'chrome':
            #d = DesiredCapabilities.CHROME
            #d['loggingPrefs'] = { 'browser':'ALL' }
            # On OS X: Chromedriver path is 'usr/local/lib/chromium-browser/chromedriver'
            self.driver = webdriver.Chrome('/usr/local/lib/chromium-browser/chromedriver')
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

        self.login()

        self.wait_for_url_and_click('/director/')
        self.wait_for_url_and_click('/schedule/')
        self.wait_for_url_and_click('/schedule/status/')
        self.wait_for_url_and_click('/storage/')
        self.wait_for_url_and_click('/client/')
        self.wait_for_url_and_click('/restore/')
        self.wait_and_click(By.XPATH, '//a[contains(@href, "/dashboard/")]', By.XPATH, '//div[@id="modal-001"]//button[.="Close"]')

        self.close_alert_and_get_its_text()
        self.logout()

    def test_restore(self):
        
        # Login
        self.login()
        self.wait_for_url_and_click('/restore/')
        # Click on client dropdown menue and close the possible modal
        self.wait_and_click(By.XPATH, '(//button[@data-id="client"])', By.XPATH, '//div[@id="modal-001"]//button[.="Close"]')

        # Select correct client
        self.wait_and_click(By.LINK_TEXT, self.client)

        # Clicks on file and navigates through the tree
        # by using the arrow-keys.
        pathlist = self.restorefile.split('/')
        for i in pathlist[:-1]:
            self.wait_for_element(By.XPATH, '//a[contains(text(),"%s/")]' % i).send_keys(Keys.ARROW_RIGHT)
        self.wait_for_element(By.XPATH, '//a[contains(text(),"%s")]' % pathlist[-1]).click()

        # Submit restore
        self.wait_and_click(By.XPATH, '//input[@id="submit"]')

        # Confirms alert
        self.assertRegexpMatches(self.close_alert_and_get_its_text(), r'^Are you sure[\s\S]$')

        # switch to dashboard to prevent that modals are open before logout
        self.wait_and_click(By.XPATH, '//a[contains(@href, "/dashboard/")]', By.XPATH, '//div[@id="modal-002"]//button[.="Close"]')
        self.close_alert_and_get_its_text()

        # Logout
        self.logout()

    def test_run_default_job(self):
        self.login()
        self.wait_and_click(By.ID, 'menu-topnavbar-job')
        self.wait_and_click(By.LINK_TEXT, 'Run')
        # Open the job list
        self.wait_and_click(By.XPATH, '(//button[@data-id="job"])')
        # Select the first job
        self.wait_and_click(By.XPATH, '(//li[@data-original-index="1"])')
        # Start it
        self.wait_and_click(By.ID, 'submit')
        self.wait_and_click(By.ID, 'menu-topnavbar-dashboard')
        self.logout()

    def test_run_configured_job(self):
        
        driver = self.driver
        
        self.login()
        
        job_id = self.job_start_configured()

        self.logout()

    def test_job_canceling(self):
        
        driver = self.driver
        
        self.login()
        
        job_id = self.job_start_configured()
        self.job_cancel(job_id)
        
        self.logout()

    def job_start_configured(self):
        
        driver = self.driver
        self.wait_and_click(By.ID, 'menu-topnavbar-job')
        self.wait_and_click(By.LINK_TEXT, 'Run')

        Select(driver.find_element_by_id('job')).select_by_visible_text('backup-bareos-fd')
        Select(driver.find_element_by_id('client')).select_by_visible_text(self.client)
        Select(driver.find_element_by_id('level')).select_by_visible_text('Incremental')
        # Clears the priority field and enters 5.
        driver.find_element_by_id('priority').clear()
        driver.find_element_by_id('priority').send_keys('5')
        
        # Open the calendar
        self.wait_and_click(By.CSS_SELECTOR, "span.glyphicon.glyphicon-calendar")
        # Click the icon to delay jobstart by 1min two times
        self.wait_and_click(By.XPATH, '//td[3]/a/span')
        self.wait_and_click(By.XPATH, '//td[3]/a/span')
        # Close the calendar
        self.wait_and_click(By.CSS_SELECTOR, 'span.input-group-addon')
        # Submit the job
        self.wait_and_click(By.ID, 'submit')
        # After the now-generated site has loaded, save the job id
        self.wait_for_spinner_absence()
        # Count how many digits are at the end of the url / how long the id is
        job_id = driver.current_url.split('/')[-1]
        # If the current URL doesn't end with a digit we didn't start the job properly.
        if not job_id.isdigit():
            raise BadJobException
            
        return job_id

    def job_cancel(self, id):
        # Go to job list
        self.wait_and_click(By.ID, 'menu-topnavbar-job')
        # Click on the object that has id in its url
        self.wait_for_url_and_click('/bareos-webui/job/details/%s' % id)
        # Click on cancel button
        self.wait_and_click(By.XPATH, '//*[@title="Cancel"]')

    def test_rerun_job(self):
        self.login()
        self.wait_and_click(By.ID, "menu-topnavbar-client")
        self.wait_and_click(By.LINK_TEXT, self.client)
        # Select first backup in list
        self.wait_and_click(By.XPATH, '//tr[@data-index="0"]/td[1]/a')
        # Press on rerun button
        self.wait_and_click(By.CSS_SELECTOR, "span.glyphicon.glyphicon-repeat")
        self.wait_and_click(By.ID, "menu-topnavbar-dashboard", By.XPATH, "//div[@id='modal-002']/div/div/div[3]/button")
        self.logout()

    def test_client_disabling(self):
        self.login()
        logger = logging.getLogger()
        # Disables client 1 and goes back to the dashboard.
        self.wait_and_click(By.ID, 'menu-topnavbar-client')
        self.wait_and_click(By.LINK_TEXT, self.client)
        self.wait_and_click(By.ID, 'menu-topnavbar-client')

        # Checks if client is enabled
        if self.client_status(self.client)=='Enabled':
            # Disables client
            self.wait_and_click(By.XPATH, '//tr[contains(td[1], "%s")]/td[5]/a[@title="Disable"]' % self.client)
            # Switches to dashboard, if prevented by open modal: close modal
            self.wait_and_click(By.ID, 'menu-topnavbar-dashboard',By.CSS_SELECTOR, 'div.modal-footer > button.btn.btn-default')

        # Throw exception if client is already disabled
        else:
            raise ClientStatusException(self.client, 'disabled')

        self.wait_and_click(By.ID, 'menu-topnavbar-client')

        # Checks if client is disabled so that it can be enabled
        if self.client_status(self.client)=='Disabled':
            # Enables client
            self.wait_and_click(By.XPATH, '//tr[contains(td[1], "%s")]/td[5]/a[@title="Enable"]' % self.client)
            # Switches to dashboard, if prevented by open modal: close modal
            self.wait_and_click(By.ID, 'menu-topnavbar-dashboard', By.CSS_SELECTOR, 'div.modal-footer > button.btn.btn-default')
        # Throw exception if client is already enabled
        else:
            raise ClientStatusException(self.client, 'enabled')

        self.logout()

    def login(self):
        driver = self.driver

        driver.get(self.base_url + '/auth/login')
        Select(driver.find_element_by_name('director')).select_by_visible_text('localhost-dir')

        driver.find_element_by_name('consolename').clear()
        driver.find_element_by_name('consolename').send_keys(self.username)
        driver.find_element_by_name('password').clear()
        driver.find_element_by_name('password').send_keys(self.password)
        driver.find_element_by_xpath('(//button[@type="button"])[2]').click()
        driver.find_element_by_link_text('English').click()
        driver.find_element_by_xpath('//input[@id="submit"]').click()
        # while ('/dashboard/' not in self.driver.current_url):
        #     sleep(self.waittime)
        self.wait_for_spinner_absence()

    def logout(self):
        self.wait_and_click(By.CSS_SELECTOR, 'a.dropdown-toggle')
        self.wait_and_click(By.LINK_TEXT, 'Logout')
        sleep(self.sleeptime)

    def client_status(self, client):
        self.client = client
        # Checks the clients status on /bareos-webui/clients
        status = self.driver.find_element(By.XPATH, '//tr[contains(td[1], "%s")]/td[4]/span' % self.client).text
        return status

    def wait_for_spinner_absence(self):
        logger = logging.getLogger()
        element = None
        try:
            element = WebDriverWait(self.driver, self.maxwait).until(EC.invisibility_of_element_located((By.ID, 'spinner')))
        except TimeoutException:
            raise ElementTimeoutException("spinner")
        return element

    def wait_for_element(self, by, value):
        logger = logging.getLogger()
        element = None
        logger.info('waiting for %s %s', by, value)
        try:
            element = self.wait.until(EC.element_to_be_clickable((by, value)))
        except TimeoutException:
            raise ElementTimeoutException(value)
        return element

    def wait_for_url_and_click(self, url):
        value='//a[contains(@href, "%s")]' % url
        self.wait_and_click(By.XPATH, value)

    def wait_and_click(self, by, value, modal_by=None, modal_value=None):
        logger = logging.getLogger()
        element = None
        starttime = datetime.now()
        seconds = 0.0
        while seconds < self.maxwait:
            self.wait_for_spinner_absence()
            if modal_by and modal_value:
                try:
                    self.driver.find_element(modal_by, modal_value).click()
                except:
                    logger.info('skipped modal: %s %s not found', modal_by, modal_value)
                else:
                    logger.info('closing modal %s %s', modal_by, modal_value)
            logger.info('waiting for %s %s (%ss)', by, value, seconds)
            element = self.wait_for_element(by, value)
            try:
                element.click()
            except WebDriverException as e:
                logger.info('WebDriverException: %s', e)
                sleep(self.waittime)
            except NoSuchElementException as e:
                logger.info("NoSuchElementException while clicking: %s", e)
                sleep(self.waittime)
            else:
                return element
            seconds = (datetime.now() - starttime).total_seconds()
        logger.error('failed to click %s %s', by, value)
        raise FailedClickException(by, value)

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

class ClientStatusException(Exception):
    '''Raise when a client does not have the expected status'''
    def __init__(self,client, status, msg=None):
        if status=='enabled':
            msg = '%s is enabled and cannot be enabled again.' % client
        if status=='disabled':
            msg = '%s is disabled and cannot be disabled again.' % client
        super(ClientStatusException, self).__init__(msg)

class BadJobException(Exception):
    '''Raise when a started job doesn't result in ID'''
    def __init__(self, msg=None):
        msg = 'Job ID could not be saved after starting the job.'
        super(BadJobException, self).__init__(msg)

class ElementTimeoutException(Exception):
    '''Raise when waiting on an element times out'''
    def __init__(self, value):
        msg = 'Waiting for element %s returned a TimeoutException.' % value
        super(ElementTimeoutException, self).__init__(msg)

class FailedClickException(Exception):
    '''Raise when wait_and_click fails'''
    def __init__(self, value):
        msg = 'Waiting and trying to click %s failed.' % value
        super(FailedClickException, self).__init__(msg)

if __name__ == '__main__':
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
