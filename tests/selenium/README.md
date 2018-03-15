# Bareos-WebUI Selenium Test

This test checks the Bareos WebUI by using seleniums webdriver.

## Requirements

  * Python >= 2.7
  * Selenium >= 3.4.0
  * chromedriver or geckodriver

## Setting up the test

To run the test you must set certain environment variables:

 * `BAREOS_BROWSER`: The test takes either 'firefox' or 'chrome', default: `firefox`
 * `BAREOS_BASE_URL`: The base url of the bareos-webui, default: `http://127.0.0.1/bareos-webui/`.
 * `BAREOS_USERNAME`: Login user name, default: `admin`
 * `BAREOS_PASSWORD`: Login password, default: `secret`
 * `BAREOS_CLIENT_NAME`: The client to use. Default is `bareos-fd`
 * `BAREOS_RESTOREFILE`: The third test is designed to restore a certain file. The default path is `/usr/sbin/bconsole`
 * `BAREOS_LOG_PATH`: Directory to create selenium log files. The default path is the current directory.
 * `BAREOS_DELAY`: Delay between action is seconds. Useful for debugging. Default is `0.0`

## Running the test

`
BAREOS_BASE_URL=http://127.0.0.1/bareos-webui/
BAREOS_USERNAME=admin
BAREOS_PASSWORD=linuxlinux
BAREOS_CLIENT_NAME=bareos-fd
BAREOS_RESTOREFILE=/etc/passwd
BAREOS_LOG_PATH=/tmp/selenium-logs/
BAREOS_DELAY=1
python webui-selenium-test.py -v
`

If you meet all the requirements and set the environment variables you can run the test with `python webui-selenium-test.py`.
