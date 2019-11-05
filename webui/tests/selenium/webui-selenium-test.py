#!/usr/bin/env python

# -*- coding: utf-8 -*-

# selenium.common.exceptions.ElementNotInteractableException: requires >= selenium-3.4.0

from   datetime import datetime
import logging
import os
import sys
from   time import sleep
import unittest

from   selenium import webdriver
from   selenium.common.exceptions import *
from   selenium.webdriver.common.by import By
from   selenium.webdriver.common.desired_capabilities import DesiredCapabilities
from   selenium.webdriver.common.keys import Keys
from   selenium.webdriver.support import expected_conditions as EC
from   selenium.webdriver.support.ui import Select, WebDriverWait

#
# try to import the SauceClient,
# required for builds inside https://travis-ci.org,
# but not available on all platforms.
#
try:
    from sauceclient import SauceClient
except ImportError:
    pass



class BadJobException(Exception):
    '''Raise when a started job doesn't result in ID'''
    def __init__(self, msg=None):
        msg = 'Job ID could not be saved after starting the job.'
        super(BadJobException, self).__init__(msg)

class ClientStatusException(Exception):
    '''Raise when a client does not have the expected status'''
    def __init__(self, client, status, msg=None):
        if status == 'enabled':
            msg = '%s is enabled and cannot be enabled again.' % client
        if status == 'disabled':
            msg = '%s is disabled and cannot be disabled again.' % client
        super(ClientStatusException, self).__init__(msg)

class ClientNotFoundException(Exception):
    '''Raise when the expected client is not found'''
    def __init__(self, client, msg=None):
        msg = 'The client %s was not found.' % client
        super(ClientNotFoundException, self).__init__(msg)

class ElementCoveredException(Exception):
    '''Raise when an element is covered by something'''
    def __init__(self, value):
        msg = 'Click on element %s failed as it was covered by another element.' % value
        super(ElementCoveredException, self).__init__(msg)

class ElementTimeoutException(Exception):
    '''Raise when waiting on an element times out'''
    def __init__(self, value):
        if value != 'spinner':
            msg = 'Waiting for element %s returned a TimeoutException.' % value
        else:
            msg = 'Waiting for the spinner to disappear returned a TimeoutException.' % value
        super(ElementTimeoutException, self).__init__(msg)

class ElementNotFoundException(Exception):
    '''Raise when an element is not found'''
    def __init__(self, value):
        msg = 'Element %s was not found.' % value
        super(ElementNotFoundException, self).__init__(msg)

class FailedClickException(Exception):
    '''Raise when wait_and_click fails'''
    def __init__(self, value):
        msg = 'Waiting and trying to click %s failed.' % value
        super(FailedClickException, self).__init__(msg)

class LocaleException(Exception):
    '''Raise when wait_and_click fails'''
    def __init__(self, dirCounter, langCounter):
        if dirCounter != langCounter:
            msg = 'The available languages in login did not meet expectations.\n Expected '+str(dirCounter)+' languages but got '+str(langCounter)+'.'
        else:
            msg = 'The available languages in login did not meet expectations.\n'
        super(LocaleException, self).__init__(msg)

class WrongCredentialsException(Exception):
    '''Raise when wait_and_click fails'''
    def __init__(self, username, password):
        msg = 'Username "%s" or password "%s" is wrong.' % (username, password)
        super(WrongCredentialsException, self).__init__(msg)



class SeleniumTest(unittest.TestCase):

    browser = 'chrome'
    chromedriverpath = None
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
    # Travis SauceLab integration
    travis = False
    sauce_username = None
    access_key = None


    def __get_dict_from_env(self):
        result = {}

        for key in [
                'TRAVIS_BRANCH',
                'TRAVIS_BUILD_NUMBER',
                'TRAVIS_BUILD_WEB_URL',
                'TRAVIS_COMMIT',
                'TRAVIS_COMMIT_MESSAGE',
                'TRAVIS_JOB_NUMBER',
                'TRAVIS_JOB_WEB_URL',
                'TRAVIS_PULL_REQUEST',
                'TRAVIS_PULL_REQUEST_BRANCH',
                'TRAVIS_PULL_REQUEST_SHA',
                'TRAVIS_PULL_REQUEST_SLUG',
                'TRAVIS_REPO_SLUG',
                'TRAVIS_TAG'
            ]:
            result[key] = os.environ.get(key)
        result['GIT_BRANCH_URL'] = 'https://github.com/bareos/bareos/tree/' + os.environ.get('TRAVIS_BRANCH')
        result['GIT_COMMIT_URL'] = 'https://github.com/bareos/bareos/tree/' + os.environ.get('TRAVIS_COMMIT')

        return result


    def __setUpTravis(self):
        self.desired_capabilities = {}
        buildnumber = os.environ['TRAVIS_BUILD_NUMBER']
        jobnumber = os.environ['TRAVIS_JOB_NUMBER']
        self.desired_capabilities['tunnel-identifier'] = jobnumber
        self.desired_capabilities['build'] = "{} {}".format(os.environ.get('TRAVIS_REPO_SLUG'), buildnumber)
        #self.desired_capabilities['name'] = "Travis Build Nr. {}: {}".format(buildnumber, self.__get_name_of_test())
        self.desired_capabilities['name'] = "{}: {}".format(os.environ.get('TRAVIS_BRANCH'), self.__get_name_of_test())
        self.desired_capabilities['tags'] = [os.environ.get('TRAVIS_BRANCH')]
        self.desired_capabilities['custom-data'] = self.__get_dict_from_env()
        self.desired_capabilities['platform'] = "macOS 10.13"
        self.desired_capabilities['browserName'] = "chrome"
        self.desired_capabilities['version'] = "latest"
        self.desired_capabilities['captureHtml'] = True
        self.desired_capabilities['extendedDebugging'] = True
        sauce_url = "http://%s:%s@localhost:4445/wd/hub"
        self.driver = webdriver.Remote(
            desired_capabilities=self.desired_capabilities,
            command_executor=sauce_url % (self.sauce_username, self.access_key)
        )


    def setUp(self):
        # Configure the logger, for information about the timings set it to INFO
        # Selenium driver itself will write additional debug messages when set to DEBUG
        logging.basicConfig(
            filename='%s/webui-selenium-test.log' % (self.logpath),
            format='%(levelname)s %(module)s.%(funcName)s: %(message)s',
            level=logging.INFO
        )
        self.logger = logging.getLogger()

        if self.travis:
            self.__setUpTravis()
        else:
            if self.browser == 'chrome':
                self.chromedriverpath = self.getChromedriverpath()
                self.driver = webdriver.Chrome(self.chromedriverpath)
                # chrome webdriver option: disable experimental feature
                opt = webdriver.ChromeOptions()
                opt.add_experimental_option('w3c',False)
                # chrome webdriver option: specify user data directory
                opt.add_argument('--user-data-dir=/tmp/chrome-user-data-'+SeleniumTest.profile+'-'+SeleniumTest.testname)
                # test in headless mode?
                if self.chromeheadless:
                    opt.add_argument('--headless')
                self.driver = webdriver.Chrome(chrome_options=opt)
                # set explicit window size
                self.driver.set_window_size(1920,1080)
            elif self.browser == "firefox":
                d = DesiredCapabilities.FIREFOX
                d['loggingPrefs'] = {'browser': 'ALL'}
                fp = webdriver.FirefoxProfile()
                fp.set_preference('webdriver.log.file', self.logpath + '/firefox_console.log')
                self.driver = webdriver.Firefox(capabilities=d, firefox_profile=fp)
            else:
                raise RuntimeError('Browser {} not found.'.format(str(self.browser)))


        # used as timeout for selenium.webdriver.support.expected_conditions (EC)
        self.wait = WebDriverWait(self.driver, self.maxwait)

        # take base url, but remove last /
        self.base_url = self.base_url.rstrip('/')
        self.verificationErrors = []
        self.logger.info("===================== TESTING =====================")

    #
    # Tests
    #
    def disabled_test_login(self):
        self.login()
        self.logout()

    def test_client_disabling(self):
        # This test navigates to clients, ensures client is enabled,
        # disables it, closes a possible modal, goes to dashboard and reenables client.
        self.login()
        # Clicks on client menue tab
        self.wait_and_click(By.ID, 'menu-topnavbar-client')
        # Tries to click on client...
        try:
            self.wait_and_click(By.LINK_TEXT, self.client)
        # Raises exception if client not found
        except ElementTimeoutException:
            raise ClientNotFoundException(self.client)
        # And goes back to dashboard tab.
        self.wait_and_click(By.ID, 'menu-topnavbar-dashboard')
        # Back to the clients
        # Disables client 1 and goes back to the dashboard.
        self.wait_and_click(By.ID, 'menu-topnavbar-client')
        self.wait_and_click(By.LINK_TEXT, self.client)
        self.wait_and_click(By.ID, 'menu-topnavbar-client')

        if self.client_status(self.client) == 'Enabled':
            # Disables client
            self.wait_and_click(By.XPATH, '//tr[contains(td[1], "%s")]/td[5]/a[@title="Disable"]' % self.client)
            if self.profile == 'readonly':
                self.wait_and_click(By.LINK_TEXT, 'Back')
            else:
                # Switches to dashboard, if prevented by open modal: close modal
                self.wait_and_click(By.ID, 'menu-topnavbar-dashboard', By.CSS_SELECTOR, 'div.modal-footer > button.btn.btn-default')

        self.wait_and_click(By.ID, 'menu-topnavbar-client')

        if self.client_status(self.client) == 'Disabled':
            # Enables client
            self.wait_and_click(By.XPATH, '//tr[contains(td[1], "%s")]/td[5]/a[@title="Enable"]' % self.client)
            if self.profile == 'readonly':
                self.wait_and_click(By.LINK_TEXT, 'Back')
            else:
                # Switches to dashboard, if prevented by open modal: close modal
                self.wait_and_click(By.ID, 'menu-topnavbar-dashboard', By.CSS_SELECTOR, 'div.modal-footer > button.btn.btn-default')

        self.logout()

    def disabled_test_job_canceling(self):
        self.login()
        job_id = self.job_start_configured()
        self.job_cancel(job_id)
        if self.profile == 'readonly':
            self.wait_and_click(By.LINK_TEXT, 'Back')
        self.logout()

    def disabled_test_languages(self):
        # Goes to login page, matches found languages against predefined list, throws exception if no match
        driver = self.driver
        driver.get(self.base_url + '/auth/login')
        self.wait_and_click(By.XPATH, '//button[@data-id="locale"]')
        expected_elements = ['Chinese', 'Czech', 'Dutch/Belgium', 'English', 'French', 'German', 'Italian', 'Russian', 'Slovak', 'Spanish', 'Turkish']
        found_elements = []
        for element in self.driver.find_elements_by_xpath('//ul[@aria-expanded="true"]/li[@data-original-index>"0"]/a/span[@class="text"]'):
            found_elements.append(element.text)
        # Compare the counted languages against the counted directories
        for element in expected_elements:
            if element not in found_elements:
                self.logger.info('The webui misses %s' % element)

    def disabled_test_menue(self):
        self.login()
        self.wait_and_click(By.ID, 'menu-topnavbar-director')
        self.wait_and_click(By.ID, 'menu-topnavbar-schedule')
        self.wait_and_click(By.XPATH, '//a[contains(@href, "/schedule/status/")]')
        self.wait_and_click(By.ID, 'menu-topnavbar-storage')
        self.wait_and_click(By.ID, 'menu-topnavbar-client')
        self.wait_and_click(By.ID, 'menu-topnavbar-restore')
        self.wait_and_click(By.ID, 'menu-topnavbar-dashboard', By.XPATH, '//div[@id="modal-001"]//button[.="Close"]')
        self.close_alert_and_get_its_text()
        self.logout()

    def test_rerun_job(self):
        self.login()
        self.wait_and_click(By.ID, "menu-topnavbar-client")
        self.wait_and_click(By.LINK_TEXT, self.client)
        # Select first backup in list
        self.wait_and_click(By.XPATH, '//tr[@data-index="0"]/td[1]/a')
        # Press on rerun button
        self.wait_and_click(By.CSS_SELECTOR, "span.glyphicon.glyphicon-repeat")
        if self.profile == 'readonly':
            self.wait_and_click(By.LINK_TEXT, 'Back')
        else:
            self.wait_and_click(By.ID, "menu-topnavbar-dashboard", By.XPATH, "//div[@id='modal-002']/div/div/div[3]/button")
        self.logout()

    def test_restore(self):
        # Login
        self.login()
        self.wait_and_click(By.ID, 'menu-topnavbar-restore')
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
        self.wait_and_click(By.XPATH, '//button[@id="btn-form-submit"]')
        # Confirm modals
        self.wait_and_click(By.XPATH, '//div[@id="modal-003"]//button[.="OK"]')
        if self.profile == 'readonly':
            self.wait_and_click(By.LINK_TEXT, 'Back')
        else:
            self.wait_and_click(By.XPATH, '//div[@id="modal-002"]//button[.="Close"]')
        # Logout
        self.logout()

    def test_run_configured_job(self):
        self.login()
        self.job_start_configured()
        if self.profile == 'readonly':
            self.wait_and_click(By.LINK_TEXT, 'Back')
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
        if self.profile == 'readonly':
            self.wait_and_click(By.LINK_TEXT, 'Back')
        else:
            self.wait_and_click(By.ID, 'menu-topnavbar-dashboard')
        self.logout()

    #
    # Methods used for testing
    #

    def client_status(self, client):
        # Wait until site and status element are loaded, check client, if not found raise exception
        self.wait.until(EC.presence_of_element_located((By.XPATH, '//tr[contains(td[1], "%s")]/td[4]/span' % client)))
        try:
            status = self.driver.find_element(By.XPATH, '//tr[contains(td[1], "%s")]/td[4]/span' % client).text
        except NoSuchElementException:
            raise ClientNotFoundException(client)
        return status

    def job_cancel(self, id):
        # Wait and click cancel button
        self.wait_for_element(By.ID, "//a[@id='btn-1'][@title='Cancel']")
        self.wait_and_click(By.ID, "//a[@id='btn-1'][@title='Cancel']")

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
        # Click the icon to delay jobstart by 1h six times
        self.wait_and_click(By.XPATH, '//a[@title="Increment Hour"]')
        self.wait_and_click(By.XPATH, '//a[@title="Increment Hour"]')
        self.wait_and_click(By.XPATH, '//a[@title="Increment Hour"]')
        self.wait_and_click(By.XPATH, '//a[@title="Increment Hour"]')
        self.wait_and_click(By.XPATH, '//a[@title="Increment Hour"]')
        self.wait_and_click(By.XPATH, '//a[@title="Increment Hour"]')
        # Close the calendar
        self.wait_and_click(By.CSS_SELECTOR, 'span.input-group-addon')
        # Submit the job
        self.wait_and_click(By.ID, 'submit')

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
        try:
            driver.find_element_by_xpath('//div[@role="alert"]')
        except:
            self.wait_for_spinner_absence()
        else:
            raise WrongCredentialsException(self.username, self.password)

    def logout(self):
        self.wait_and_click(By.CSS_SELECTOR, 'span.glyphicon.glyphicon-user')
        self.wait_and_click(By.LINK_TEXT, 'Logout')
        sleep(self.sleeptime)

    #
    # Methods used for waiting and clicking
    #

    def getChromedriverpath(self):
        if SeleniumTest.chromedriverpath is None:
            for chromedriverpath in ['/usr/local/sbin/chromedriver', '/usr/local/bin/chromedriver']:
                if os.path.isfile(chromedriverpath):
                    return chromedriverpath
        else:
            return SeleniumTest.chromedriverpath
        raise IOError('Chrome Driver file not found.')

    def wait_and_click(self, by, value, modal_by=None, modal_value=None):
        logger = logging.getLogger()
        element = None
        starttime = datetime.now()
        seconds = 0.0
        while seconds < self.maxwait:
            self.wait_for_spinner_absence()
            if modal_by and modal_value:
                try:
                    #self.driver.switchTo().activeElement(); # required??? 
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
            else:
                return element
            seconds = (datetime.now() - starttime).total_seconds()
        logger.error('failed to click %s %s', by, value)
        raise FailedClickException(value)

    def wait_for_element(self, by, value):
        logger = logging.getLogger()
        element = None
        logger.info('waiting for %s %s', by, value)
        try:
            element = self.wait.until(EC.element_to_be_clickable((by, value)))
        except TimeoutException:
            self.driver.save_screenshot('screenshot.png')
            raise ElementTimeoutException(value)
        if element is None:
            try:
                self.driver.find_element(by, value)
            except NoSuchElementException:
                self.driver.save_screenshot('screenshot.png')
                raise ElementNotFoundException(value)
            else:
                self.driver.save_screenshot('screenshot.png')
                raise ElementCoveredException(value)
        return element

    def wait_for_spinner_absence(self):
        element = None
        try:
            element = WebDriverWait(self.driver, self.maxwait).until(EC.invisibility_of_element_located((By.ID, 'spinner')))
        except TimeoutException:
            raise ElementTimeoutException("spinner")
        return element

    def close_alert_and_get_its_text(self, accept=True):
        logger = logging.getLogger()
        try:
            alert = self.driver.switch_to_alert()
            alert_text = alert.text
        except NoAlertPresentException:
            return None
        if accept:
            alert.accept()
        else:
            alert.dismiss()
        logger.debug('alert message: %s' % (alert_text))
        return alert_text

    def tearDown(self):
        logger = logging.getLogger()
        if self.travis:
            print("Link to test {}: https://app.saucelabs.com/jobs/{}".format(self.__get_name_of_test(), self.driver.session_id))
            sauce_client = SauceClient(self.sauce_username, self.access_key)
            if sys.exc_info() == (None, None, None):
                sauce_client.jobs.update_job(self.driver.session_id, passed=True)
            else:
                sauce_client.jobs.update_job(self.driver.session_id, passed=False)
        try:
            self.driver.quit()
        except WebDriverException as e:
            logger.warn('{}: ignored'.format(str(e)))

        self.assertEqual([], self.verificationErrors)

    def __get_name_of_test(self):
        return self.id().split('.', 1)[1]



def get_env():
    '''
    Get attributes as environment variables,
    if not available or set use defaults.
    '''

    chromedriverpath = os.environ.get('BAREOS_WEBUI_CHROMEDRIVER_PATH')
    if chromedriverpath:
        SeleniumTest.chromedriverpath = chromedriverpath

    chromeheadless = os.environ.get('BAREOS_WEBUI_CHROME_HEADLESS')
    if chromeheadless:
        SeleniumTest.chromeheadless = chromeheadless
    else:
        SeleniumTest.chromeheadless = True

    browser = os.environ.get('BAREOS_WEBUI_BROWSER')
    if browser:
        SeleniumTest.browser = browser

    base_url = os.environ.get('BAREOS_WEBUI_BASE_URL')
    if base_url:
        SeleniumTest.base_url = base_url.rstrip('/')

    username = os.environ.get('BAREOS_WEBUI_USERNAME')
    if username:
        SeleniumTest.username = username

    password = os.environ.get('BAREOS_WEBUI_PASSWORD')
    if password:
        SeleniumTest.password = password

    profile = os.environ.get('BAREOS_WEBUI_PROFILE')
    if profile:
        SeleniumTest.profile = profile
        print("using profile:" + profile)

    testname = os.environ.get('BAREOS_WEBUI_TESTNAME')
    if testname:
        SeleniumTest.testname = testname

    client = os.environ.get('BAREOS_WEBUI_CLIENT_NAME')
    if client:
        SeleniumTest.client = client

    restorefile = os.environ.get('BAREOS_WEBUI_RESTOREFILE')
    if restorefile:
        SeleniumTest.restorefile = restorefile

    logpath = os.environ.get('BAREOS_WEBUI_LOG_PATH')
    if logpath:
        SeleniumTest.logpath = logpath

    sleeptime = os.environ.get('BAREOS_WEBUI_DELAY')
    if sleeptime:
        SeleniumTest.sleeptime = float(sleeptime)
    if os.environ.get('TRAVIS_COMMIT'):
        SeleniumTest.travis = True
        SeleniumTest.sauce_username = os.environ.get('SAUCE_USERNAME')
        SeleniumTest.access_key = os.environ.get('SAUCE_ACCESS_KEY')



if __name__ == '__main__':
    get_env()
    unittest.main()

