# Bareos REST API using fastAPI and python-bareos

Experimental and subject to enhancements and changes.

This is an experimental backend for development purposes.

It provides a REST API using fastapi and python-bareos to connect to
a Bareos director.

* https://fastapi.tiangolo.com/
* https://www.openapis.org/

## Guide

### Installation

Copy the rest-api directory to any place outside the source-tree and change into it.

Change into the copied directory with _requirements.txt_, _bareos-restapi.py_ and _api.ini_
* We recommend to create a dedicated Python environment for the installation:

```
python3 -m venv env
# Activate the virtual environment inside the directory
source env/bin/activate
# Install dependencies into the virtual environment
pip install -r requirements.txt
```

Note: _requirements.txt_ contains a list of needed Python-modules, which will get installed by pip. The module _sslpsk==1.0.0_ is commented, because it requires an installed gcc and header files to be installed. Uncomment, if you want encrypted communication between the API and the Bareos director.

### Configuration
Configure your Bareos Server in _api.ini_ adapting these 3 lines of configuration, so that it points to your director:
```
[Director]
Name=bareos-dir
Address=127.0.0.1
Port=9101
```

Note: you will need a _named console_ (user/password) to acces the Bareos director using this API. Read more about Consoles here:
https://docs.bareos.org/Configuration/Director.html#console-resource

TODO: add possibility to connect to a choice of directors

### Start the backend server

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

TODO: response model are not defined / documented yet.
