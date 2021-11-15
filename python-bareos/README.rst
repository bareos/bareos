python-bareos
=============

`python-bareos` is a Python module to access a http://www.bareos.org backup system.

Packages for `python-bareos` are included in the Bareos core distribution and available via https://pypi.org/.

Documentation is available at https://docs.bareos.org/DeveloperGuide/PythonBareos.html

.. note::

   By default, the Bareos Director (>= 18.2.4) uses TLS-PSK when communicating through the network.

   Unfortunately the Python core module ``ssl``
   does not support TLS-PSK.
   For testing ``python-bareos`` should be used without TLS.
   The section `Transport Encryption (TLS-PSK)`_ describes
   how to use ``python-bareos`` with TLS-PSK
   and about the limitations.


Preparations
============

Create some named consoles for testing:

.. code-block:: shell-session

   root@host:~# bconsole
   *configure add console name=user1 password=secret profile=operator TlsEnable=no
   *configure add console name=user-tls password=secret profile=operator


This creates a console user with name `user1` and the profile `operator`.
The `operator` profile is a default profile that comes with the Bareos Director.
It does allow most commands. It only deny some dangerous commands (see ``show profile=operator``),
so it is well suited for this purpose.
Futhermore, TLS enforcement is disabled for this console user.

For testing with TLS-PSK, we also create the user `user-tls`.

Examples
========

Calling bareos-director console commands
----------------------------------------

.. code:: python

   >>> import bareos.bsock
   >>> directorconsole=bareos.bsock.DirectorConsole(address='localhost', port=9101, name='user1', password='secret')
   >>> print(directorconsole.call('help').decode("utf-8"))

This creates a console connection to a Bareos Director.
This connection can be used to `call` commands.
These are the same commands as available via ``bconsole``.

To connect to the default console instead, omit the `name` parameter:

.. code:: python

   >>> directorconsole = bareos.bsock.DirectorConsole(address='localhost', port=9101, password='defaultconsolepassword')

The result of the call method is a ``bytes`` object. In most cases, it has to be decoded to UTF-8.



Simple version of the bconsole in Python
----------------------------------------

.. code:: python

   >>> import bareos.bsock
   >>> directorconsole = bareos.bsock.DirectorConsole(address='localhost', port=9101, password='secret')
   >>> directorconsole.interactive()

Or use the included ``bconsole.py`` script:

.. code-block:: shell-session

   bconsole.py --debug --name=user1 --password=secret localhost


Use JSON objects of the API mode 2
----------------------------------

Requires: Bareos >= 15.2

The class `DirectorConsoleJson` is inherited from `DirectorConsole`
and uses the Director Console API mode 2 (JSON).

For general information about API mode 2 and what data structures to expect,
see https://docs.bareos.org/DeveloperGuide/api.html#api-mode-2-json

Example:

.. code:: python

   >>> import bareos.bsock
   >>> directorconsole = bareos.bsock.DirectorConsoleJson(address='localhost', port=9101, password='secret')
   >>> pools = directorconsole.call('list pools')
   >>> for pool in pools["pools"]:
   ...   print(pool["name"])
   ...
   Scratch
   Incremental
   Full
   Differential

The results the the `call` method is a ``dict`` object.

In case of an error, an exception, derived from ``bareos.exceptions.Error`` is raised.

Example:


.. code:: python

   >>> directorconsole.call("test it")
   Traceback (most recent call last):
   ...
   bareos.exceptions.JsonRpcErrorReceivedException: failed: test it: is an invalid command.



.. _section-python-bareos-tls-psk:

Transport Encryption (TLS-PSK)
==============================

Since Bareos >= 18.2.4, Bareos supports TLS-PSK (Transport-Layer-Security Pre-Shared-Key) to secure its network connections and uses this by default.

Unfortenatly, the Python core module `ssl` does not support TLS-PSK.
There is limited support by the extra module `sslpsk` (see https://github.com/drbild/sslpsk).

Fallback To Unencrypted Connections
-----------------------------------

In order to work in most cases, even if ``sslpsk`` is not available,
the `DirectorConsole` uses a fallback.
If connecting via TLS-PSK fails, it falls back to the old, unencrypted protocol version.
In this case, a warning is issued, but the connection will work nevertheless:

.. code:: python

   >>> import bareos.bsock
   /.../bareos/bsock/lowlevel.py:39: UserWarning: Connection encryption via TLS-PSK is not available, as the module sslpsk is not installed.
   >>> directorconsole=bareos.bsock.DirectorConsole(address='localhost', port=9101, name='user-tls', password='secret')
   socket error: Conversation terminated (-4)
   Failed to connect using protocol version 2. Trying protocol version 1.
   >>> print(directorconsole.call('help').decode("utf-8"))

To enforce a encrypted connection, use the ``tls_psk_require=True`` parameter:

.. code:: python

   >>> import bareos.bsock
   /.../bareos/bsock/lowlevel.py:39: UserWarning: Connection encryption via TLS-PSK is not available, as the module sslpsk is not installed.
   >>> directorconsole=bareos.bsock.DirectorConsole(address='localhost', port=9101, name='user-tls', password='secret', tls_psk_require=True)
   Traceback (most recent call last):
   [...]
   bareos.exceptions.ConnectionError: TLS-PSK is required, but sslpsk module not loaded/available.

In this case, an exception is raised, if the connection can not be established via TLS-PSK.

sslpsk
------

The extra module `sslpsk` (see https://github.com/drbild/sslpsk)
extends the core module `ssl` by TLS-PSK.

At the time of writing, the lasted version installable via pip is 1.0.0 (https://pypi.org/project/sslpsk/), which is not working with Python >= 3.

If `python-bareos` should use TLS-PSK with Python >= 3,
the latest version must by installed manually:

.. code:: shell

   git clone https://github.com/drbild/sslpsk.git
   cd sslpsk
   python setup.py build
   python setup.py install

`python-bareos` will detect, that `sslpsk` is available and will use it automatically.
This can be verified by following command:

.. code:: python

   >>> import bareos.bsock
   >>> bareos.bsock.DirectorConsole.is_tls_psk_available()
   True

Another limitation of the current `sslpsk` version is,
that it is not able to autodetect the TLS protocol version to use.

In order to use it, specify ``tls_version`` with an appropriate protocol version.
In most cases this should be ``tls_version=ssl.PROTOCOL_TLSv1_2``,
like in the following example:

.. code:: python

   >>> import ssl
   >>> import bareos.bsock
   >>> directorconsole = bareos.bsock.DirectorConsoleJson(address='localhost', user='user-tls', password='secret', tls_version=ssl.PROTOCOL_TLSv1_2)
   >>> print(directorconsole.call('help').decode("utf-8"))
