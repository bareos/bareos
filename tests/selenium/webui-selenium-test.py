# -*- coding: utf-8 -*-
import logging, os, re, sys, time, unittest
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.common.desired_capabilities import DesiredCapabilities
from selenium.common.exceptions import WebDriverException, ElementNotInteractableException, ElementNotVisibleException, TimeoutException, NoAlertPresentException, NoSuchElementException
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.support.ui import Select, WebDriverWait

class WebuiSeleniumTest(unittest.TestCase):

    def setUp(self):
        if browser == "chrome":
            d = DesiredCapabilities.CHROME
            d['loggingPrefs'] = { 'browser':'ALL' }
            self.driver = webdriver.Chrome('/usr/local/sbin/chromedriver')
        if browser == "firefox":
            d = DesiredCapabilities.FIREFOX
            d['loggingPrefs'] = { 'browser':'ALL' }
            fp = webdriver.FirefoxProfile()
            fp.set_preference('webdriver.log.file', os.getcwd() + '/firefox_console.log')
            self.driver = webdriver.Firefox()
        self.driver.implicitly_wait(4)
        self.base_url = "http://%s" % targethost
        self.verificationErrors = []
        self.accept_next_alert = True
    
    def test_login(self):
        self.login()
        time.sleep(0.5+t)
        self.logout()

    def test_menue(self):
        driver = self.driver
        # On windows we have a different baseurl
        if os.getenv('DIST') == "windows":
            driver.get(self.base_url + "/")
        else:
            driver.get(self.base_url + "/bareos-webui/")

        self.login()
        self.wait_for_url('/bareos-webui/director/').click()
        self.wait_for_url("/bareos-webui/schedule/").click()
        self.wait_for_url("/bareos-webui/schedule/status/").click()
        self.wait_for_url("/bareos-webui/storage/").click()
        self.wait_for_url("/bareos-webui/client/").click()
        self.wait_for_url("/bareos-webui/restore/").click()
        self.logout()

    def test_restore(self):
        pathlist = restorefile.split('/')
        driver = self.driver

        # LOGGING IN:
        self.login()

        # CHANGING TO RESTORE TAB:
        self.wait_for_url("/bareos-webui/restore/").click()
        
        # SELECTING CLIENT:
        # Selects the correct client
        self.wait_for_element(By.XPATH, "(//button[@type='button'])[3]").click()
        self.driver.find_element(By.LINK_TEXT, client).click()
        
        # FILE-SELECTION:
        # Clicks on file and navigates through the tree
        # by using the arrow-keys.
        for i in pathlist[:-1]:
            self.wait_for_element(By.XPATH, "//a[contains(text(),'%s/')]" % i).send_keys(Keys.ARROW_RIGHT)
        else:
            self.wait_for_element(By.XPATH, "//a[contains(text(),'%s')]" % pathlist[-1]).click()

        # CONFIRMATION:
        # Clicks on 'submit'
        self.wait_for_element(By.XPATH, "//input[@id='submit']").click()
        # Confirms alert that has text "Are you sure ?"
        self.assertRegexpMatches(self.close_alert_and_get_its_text(), r"^Are you sure[\s\S]$")

        # LOGOUT:
        self.logout()


    def login(self):
        driver = self.driver

        # on windows we have a different baseurl
        if os.getenv('DIST') == "windows":
            driver.get(self.base_url + "/auth/login")
        else:
            driver.get(self.base_url + "/bareos-webui/auth/login")
        Select(driver.find_element_by_name("director")).select_by_visible_text("localhost-dir")

        driver.find_element_by_name("consolename").clear()
        driver.find_element_by_name("consolename").send_keys(username)
        driver.find_element_by_name("password").clear()
        driver.find_element_by_name("password").send_keys(password)
        driver.find_element_by_xpath("(//button[@type='button'])[2]").click()
        driver.find_element_by_link_text("English").click()
        driver.find_element_by_xpath("//input[@id='submit']").click()
        while ("/bareos-webui/dashboard/" not in self.driver.current_url):
            time.sleep(t*0.1)

    def logout(self):
        time.sleep(t)
        self.wait_for_element(By.CSS_SELECTOR, "a.dropdown-toggle").click()
        self.wait_for_element(By.LINK_TEXT, "Logout").click()
        time.sleep(2)

    def wait_for_url(self, what):
        value="//a[contains(@href, '%s')]" % what
        time.sleep(t*0.5)
        return self.wait_for_element(By.XPATH, value)
        
    def wait_for_element(self, by, value):
        i=10
        element=None
        while i>0 and element is None:
            try:
                time.sleep(0.5+(t*0.5))
                logging.info("Loading %s", value)
                tmp_element = self.driver.find_element(by, value)
                if tmp_element.is_displayed():
                    element = tmp_element
            except ElementNotInteractableException:
                logging.info("Element %s not interactable", value)
            except NoSuchElementException:
                logging.info("Element %s not existing", value)
            except ElementNotVisibleException:
                logging.info("Element %s not visible", value)
            i=i-1
        if(i==0):
            logging.info("Timeout while loading %s .", value)
        else:
            logging.info("Element loaded after %s seconds." % (11-i))
        return element

    def is_alert_present(self):
        try: self.driver.switch_to_alert()
        except NoAlertPresentException as e: return False
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
    # Configure the logger, for information about the timings set it to INFO
    # Selenium driver itself will write additional debug messages when set to DEBUG
    logging.basicConfig(filename='webui-selenium-test.log', level=logging.INFO)
    logger = logging.getLogger()

    # Get attributes as environment variables,
    # if not available or set use defaults.
    browser = os.environ.get('BROWSER')
    if not browser:
        browser = 'firefox'
    client = os.environ.get('CLIENT')
    if not client:
        client = 'bareos-fd'
    restorefile = os.environ.get('RESTOREFILE')
    if not restorefile:
        restorefile = '/usr/sbin/bconsole'
    username = os.environ.get('USERNAME')
    if not username:
        username = "citest"
    password = os.environ.get('PASSWORD')
    if not password:
        password = "citestpass"
    t = os.environ.get('SLEEPTIME')
    if t:
        t = float(t)
    if not t:
        t = 1.0
    targethost = os.environ.get('VM_IP')
    if not targethost:
        targethost = "127.0.0.1"
    unittest.main()