.. _section-webui-vue:

Bareos WebUI Vue
================

.. index::
   single: WebUI Vue
   single: WebUI; Vue

The Vue-based Bareos WebUI is a new single-page application located in
:file:`webui-vue/`.
It was introduced as a technical preview in Bareos 25.1.
Starting with Bareos 26, it is the default interface selected by the
:file:`bareos-webui` package name. The classic PHP-based
:ref:`section-webui` remains available separately.

Coexistence with the classic WebUI
----------------------------------

The current Bareos documentation distinguishes between two WebUI variants:

* :ref:`section-webui` documents the classic PHP-based WebUI.
* This page documents the Vue-based WebUI.

The two interfaces are designed to exist in parallel.
In a typical Apache deployment they use separate URLs:

* classic WebUI: :file:`/bareos-webui`
* Vue WebUI: :file:`/bareos/`

.. note::

   The default Apache configuration shipped with the Vue WebUI redirects
   :file:`/` and the former :file:`/bareos-webui-new/` URL to
   :file:`/bareos/`.

The Vue WebUI uses the same Bareos Director console concepts as the classic
WebUI, so user access still depends on properly configured
:config:option:`Dir/Console` resources and matching ACLs.

Architecture overview
---------------------

The Vue WebUI consists of:

* the static SPA bundle installed below :file:`/usr/share/bareos-webui-vue`
* an Apache configuration that exposes the bundle below
  :file:`/bareos/`
* the :command:`bareos-webui-proxy` service, which accepts both HTTP session
  requests and WebSocket connections and forwards them to the Bareos Director

The default Apache configuration proxies both :file:`/bareos/ws` and
:file:`/bareos/api/` to :command:`bareos-webui-proxy` on port **9104**.
The HTTP endpoints are used for session login, restore, and logout, while the
WebSocket endpoint is used for the interactive director connection.

By default, :command:`bareos-webui-proxy` starts without an explicit config
file. If :file:`/etc/bareos-webui-proxy/bareos-webui-proxy.ini` exists, it is
loaded. Otherwise built-in defaults are used.
The package ships a default template
:file:`bareos-webui-proxy.ini` in the Bareos config template directory
(for example :file:`/usr/lib/bareos/defaultconfigs/`), so administrators can
copy and adapt it into :file:`/etc/bareos-webui-proxy/` when needed.
When ``--config`` is used explicitly, the given file must exist.

Installation and access
-----------------------

The Vue WebUI is installed separately from the classic PHP WebUI via the
:file:`bareos-webui-vue` package and keeps its own Apache configuration file
:file:`bareos-webui-vue.conf`. In Bareos 26, installing
:file:`bareos-webui` selects the Vue WebUI.
The classic PHP WebUI package is named :file:`bareos-webui-php`; its URL and
installation directory remain unchanged.

After installation, open the WebUI at:

.. code-block:: text

   https://HOSTNAME/bareos/

To use it successfully, ensure that:

* the Vue WebUI bundle is installed
* the Apache configuration for :file:`/bareos/` is enabled
* :command:`bareos-webui-proxy` is installed and running
* the HTTP and WebSocket proxy targets on port **9104** are reachable from
  Apache
* suitable Bareos Director console credentials exist

The Vue package includes example Director console and limited profile
resources in :file:`/usr/lib/bareos/defaultconfigs/bareos-dir.d/`.
To use the console example, copy it to
:file:`/etc/bareos/bareos-dir.d/console/admin.conf` without overwriting an
existing resource, replace the password placeholder with a unique, strong
password, and reload the Director.
The example uses the Director's default TLS-PSK settings.

HTTPS configuration
-------------------

Use HTTPS to protect console credentials and other WebUI traffic.
Configure a trusted certificate and enforce HTTPS at the web server or
reverse proxy; HTTP is not automatically disabled or redirected.

For Apache, enable ``mod_headers`` so the packaged ``X-Forwarded-Proto``
directive takes effect and the proxy can mark HTTPS session cookies
``Secure``.

SELinux and AppArmor
--------------------

If Apache cannot reach the proxy, check SELinux/AppArmor audit logs and
adjust the relevant policy. On SELinux, check
``httpd_can_network_connect``. Do not disable either security system.

Upgrading from Bareos 25
------------------------

For package migration and upgrade instructions, see
:ref:`section-WebuiMigration26`.
If the PHP WebUI is installed, upgrade it together with the Vue package so
the package manager can transfer ownership of the example configuration files.

Configuring multiple directors
------------------------------

By default, :command:`bareos-webui-proxy` connects to a single local Director.
To make multiple Directors selectable in the WebUI, copy the shipped config
template into place and add one section per Director:

.. code-block:: shell-session

   cp /usr/lib/bareos/defaultconfigs/bareos-webui-proxy.ini \
      /etc/bareos-webui-proxy/bareos-webui-proxy.ini

Then edit :file:`/etc/bareos-webui-proxy/bareos-webui-proxy.ini`:

.. code-block:: ini

   [listen]
   address = localhost
   port = 9104

   [bareos-dir]
   address = localhost

   [site-b]
   address = dr.example.com
   director_name = bareos-dir

Each section name (e.g. ``bareos-dir``, ``site-b``) is shown as a selectable
Director in the WebUI login and scope menu.
For available options, including ``director_name``, see
:ref:`Director section options <webui-vue-director-options>`.

After changing the config, restart the proxy:

.. code-block:: shell-session

   systemctl restart bareos-webui-proxy

Once logged in to multiple Directors, the scope menu at the top of the page
allows switching between them or viewing data from all Directors at once.

``[listen]`` section options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 35 15 50
   :header-rows: 1

   * - Option
     - Default
     - Description
   * - ``address``
     - ``localhost``
     - Address the proxy binds to.
   * - ``port``
     - ``9104``
     - Port the proxy listens on.
   * - ``log_level``
     - ``info``
     - Minimum log level: ``debug``, ``info``, ``warn``, ``error``.
       The ``--log-level`` CLI flag takes precedence if specified.
   * - ``session_idle_timeout_minutes``
     - ``30``
     - Minutes of inactivity before a session is invalidated.
   * - ``session_absolute_lifetime_hours``
     - ``8``
     - Maximum session lifetime in hours regardless of activity.
   * - ``max_unauthenticated_connections``
     - ``100``
     - Maximum concurrent connections that have not yet completed login.

.. _webui-vue-director-options:

``[<director>]`` section options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 35 15 50
   :header-rows: 1

   * - Option
     - Default
     - Description
   * - ``address``
     - ``localhost``
     - Hostname or IP address of the Bareos Director.
   * - ``port``
     - ``9101``
     - Port the Bareos Director listens on.
   * - ``director_name``
     - *(section name)*
     - Real Director name as announced in the greeting banner.
       Set this when the section name differs from the Director name.
   * - ``tls_psk_disable``
     - ``no``
     - Set to ``yes`` to disable TLS-PSK for this Director connection.

What is already available
-------------------------

The Vue WebUI covers important day-to-day workflows, including:

* login with Bareos console credentials
* dashboard views
* job overview and job details
* restore workflows, including plugin restore options
* client, fileset, schedule, storage, and volume views
* Director status and message views
* a console popup for command execution
* multi-director capable views where supported by the page

Known differences and current limitations
-----------------------------------------

Compared to the classic PHP WebUI, the Vue WebUI should currently be
viewed as a parallel implementation under active development.

Important differences are:

* documentation and operational guidance are still catching up
* some workflows are intentionally presented differently than in the classic
  WebUI instead of being a 1:1 port
* the Vue WebUI depends on :command:`bareos-webui-proxy`, which is a separate
  runtime component

Further information
-------------------

For developer-oriented details, see :ref:`section-dev-webui-vue`.
