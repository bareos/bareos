# Bareos-WebUI Selenium Test

This test checks the Bareos WebUI by using seleniums webdriver.

## Requirements

  * Python >= 2.7
  * Selenium >= 3.4.0
  * chromedriver for Chrome/Chromium testing, geckodriver for Firefox testing.

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

### Optional:

 * `BAREOS_CHROMEDRIVER_PATH`: Set the path to your chromedriver here.

## Running the test

```
BAREOS_BASE_URL=http://127.0.0.1/bareos-webui/
BAREOS_USERNAME=admin
BAREOS_PASSWORD=linuxlinux
BAREOS_CLIENT_NAME=bareos-fd
BAREOS_RESTOREFILE=/etc/passwd
BAREOS_LOG_PATH=/tmp/selenium-logs/
BAREOS_DELAY=1
python WebuiSeleniumTest.py -v
```

After setting the environment variables you can run the test. Use `-v`option of our test to show the progress and results of each test.

## Debugging

After the test fails you will see an exception that was thrown. If this does not help you, take a look inside the generated log file, located in the same path as your `WebuiSeleniumTest.py` file.
