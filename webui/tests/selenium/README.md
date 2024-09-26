# Bareos-WebUI Selenium Test

This test checks the Bareos WebUI by using seleniums webdriver.

## Requirements

  * Python >= 2.7
  * Selenium >= 3.4.0
  * chromedriver for Chrome/Chromium testing, geckodriver for Firefox testing.

## Setting up the test

To run the test you must set certain environment variables:

 * `BAREOS_WEBUI_BROWSER`: The test takes either 'firefox' or 'chrome'
 * `BAREOS_WEBUI_BASE_URL`: The base url of the bareos-webui, default: `http://127.0.0.1/bareos-webui/`.
 * `BAREOS_WEBUI_USERNAME`: Login user name, default: `admin`
 * `BAREOS_WEBUI_PASSWORD`: Login password, default: `secret`
 * `BAREOS_WEBUI_PROFILE`: if set to "readonly", only actions that are available in readonly modus are used.
 * `BAREOS_WEBUI_CLIENT_NAME`: The client to use. Default is `bareos-fd`
 * `BAREOS_WEBUI_RESTOREFILE`: The third test is designed to restore a certain file. The default path is `/usr/sbin/bconsole`
 * `BAREOS_WEBUI_LOG_PATH`: Directory to create selenium log files. The default path is the current directory.
 * `BAREOS_WEBUI_DELAY`: Delay between action is seconds. Useful for debugging. Default is `0.0`

### Optional:

 * `BAREOS_WEBUI_BROWSER_RESOLUTION`: e.g. "1200x800"
 * `BAREOS_WEBUI_CHROMEDRIVER_PATH`: Set the path to your chromedriver here.
 * `BAREOS_WEBUI_CHROME_HEADLESS`: By default, tests with chrome are performed headless. Setting it to "no" will start (and end) a browser window.
 * `BAREOS_WEBUI_TESTNAME`: internally, just for logging.

## Running the test

```
export BAREOS_WEBUI_BASE_URL=http://127.0.0.1/bareos-webui/
export BAREOS_WEBUI_BROWSER=chrome
export BAREOS_WEBUI_CHROME_HEADLESS=no
export BAREOS_WEBUI_CHROMEDRIVER_PATH=/usr/bin/chromedriver
export BAREOS_WEBUI_CLIENT_NAME=bareos-fd
export BAREOS_WEBUI_DELAY=1
export BAREOS_WEBUI_LOG_PATH=/tmp/selenium-logs/
export BAREOS_WEBUI_PASSWORD=admin
export BAREOS_WEBUI_PROFILE=admin
export BAREOS_WEBUI_RESTOREFILE=/etc/passwd
export BAREOS_WEBUI_USERNAME=readonly

python webui-selenium-test.py -v
```

After setting the environment variables you can run the test. Use `-v`option of our test to show the progress and results of each test.

A single test can be performed by:

```
python webui-selenium-test.py -v SeleniumTest.test_run_default_job
```


## Debugging

After the test fails you will see an exception that was thrown. If this does not help you, take a look inside the generated log file, located in the same path as your `webui-selenium-test.py` file.
