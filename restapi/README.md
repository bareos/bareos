# Bareos REST API using fastAPI and python-bareos

Experimental and subject to enhancements and changes.

This is an experimental backend for development purposes.

It provides a REST API using fastapi and python-bareos to connect to
a Bareos director.

* https://fastapi.tiangolo.com/
* https://www.openapis.org/

## Guide

### Installation

We recommend to create a dedicated Python environment for the installation in an own directory:

```
python3 -m venv env
# Activate the virtual environment inside the directory
source env/bin/activate
# Install dependencies into the virtual environment
pip install bareos-restapi
```

Note: The optional module _sslpsk_ can be installed manually, if you want encrypted communication between the API and the Bareos director.

### Configuration

The module directory contains a sample ini-file. Change into this directory (something like */lib/python3.x/site-packages/bareosRestapiModels* on most operating systems) and copy _api.ini.sample_ to _api.ini_ and adapt it to your director. Make sure to generate an own secret-key for the _JWT_ section.

Your director configuration in _api.ini_ should look like:
```
[Director]
Name=bareos-dir
Address=127.0.0.1
Port=9101
```

Note: you will need a *named console* (user/password) to acces the Bareos director using this API. Read more about Consoles here:
https://docs.bareos.org/Configuration/Director.html#console-resource


### Start the backend server

From inside the module directory run:

```
uvicorn bareos-restapi:app --reload
```

* Serve the Swagger UI to explore the REST API: http://127.0.0.1:8000/docs
* Alternatively you can use the redoc format: http://127.0.0.1:8000/redoc


### Browse
The Swagger UI contains documentation and online-tests. Use "authorize" to connect to your Bareos director using a named console. Read here to learn how to configure
a named console: https://docs.bareos.org/Configuration/Director.html#console-resource

The Swagger documentation also contains CURL statements for all available endpoints.

## Future work

The API will be extended by some methods provided by the Bareos console, that are not yet implemented. It is also planned to add delete / update options for configuration in the director and REST API. If you are interested in support and / or funding enhancements, please visit https://www.bareos.com

TODO: 
- define and document response model
- add possibility to connect to a choice of directors
- add start-script with ini-file name as parameter
