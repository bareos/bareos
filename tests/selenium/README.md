# Bareos-WebUI Selenium Test

This test checks the Bareos WebUI by using seleniums webdriver.


## Setting up the test

To run the test you must set certain environment variables:

 * **BROWSER**: The test takes either 'firefox' or 'chrome', where 'firefox' is the default.
 * **USERNAME** and **PASSWORD**: These should contain the login information for the WebUI.
 * **VM_IP**: This should be the IP adress of the system where the Bareos WebUI runs on.
 * **RESTOREFILE**: The third test is designed to restore a certain file. The default path is '/usr/sbin/bconsole".
 * **CLIENT**: Here you need to set what Client the restore test should select.

## Running the test

To run all tests included you need a system that runs the WebUI, a client for restore-testing, chromedriver or geckodriver as well as any Python >= 2.7.

If you meet all the requirements and set the environment variables you can run the test with `python webui-selenium-test.py`.

## Debugging

If the test should fail you will find additional informations in the webui-selenium-test.log file. You might want to adjust **SLEEPTIME** environment variable to be set above 1 as it increases the time waited between commands.