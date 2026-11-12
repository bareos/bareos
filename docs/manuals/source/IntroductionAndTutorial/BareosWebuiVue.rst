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

The Apache configuration redirects :file:`/` and the former
:file:`/bareos-webui-new/` URL to :file:`/bareos/`.

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
:file:`bareos-webui-vue` package, which provides :file:`bareos-webui`, and keeps
its own Apache configuration file :file:`bareos-webui-vue.conf`.
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

HTTPS configuration
-------------------

Use HTTPS before transmitting console credentials or backup data.
Certificate provisioning, renewal and TLS termination are administrator
responsibilities. Bareos package hooks do not generate certificates or
activate default SSL sites. Operating-system Apache/SSL dependencies may
provide their own default TLS configuration; this is not proof of trusted
HTTPS for the WebUI.

Configure an administrator-managed HTTPS virtual host or reverse proxy
using a certificate trusted by your users and covering the hostname used
to access the WebUI. Follow :ref:`section-webui-https` for Apache setup
and verification. Ensure the proxy forwards both the API and WebSocket
requests and communicates the original connection scheme correctly.
For Apache, enable ``mod_headers`` so the packaged
``X-Forwarded-Proto`` directive takes effect (``a2enmod headers`` on
Debian/Ubuntu and SUSE). This is necessary for the proxy to mark HTTPS
session cookies ``Secure`` correctly.

HTTP access is not automatically disabled or redirected to HTTPS.
Both WebUIs show a warning on insecure HTTP connections. Configure HTTPS
enforcement at the web server or reverse proxy according to your site's
policy; the warning is not a substitute for transport encryption.

SELinux and AppArmor
--------------------

On a fresh enforcing-SELinux installation, Apache may be denied access to
the Vue proxy on localhost port 9104. A loopback address does not bypass
network policy. The WebUI packages do not enable
``httpd_can_network_connect`` automatically. Check its state and the
web-server domain, then make an explicit administrator policy decision as
described in :ref:`section-webui-selinux`. The boolean grants broad HTTPD
network access, not localhost-only access.

Before attributing a failure to confinement, verify that
:command:`bareos-webui-proxy` is running and listening on its configured
address. Inspect the Apache error log and SELinux AVC/AppArmor ``DENIED``
records. The connection-error hint is conditional, not a diagnosis.
Wrong credentials and Director ACL errors require separate investigation.
For AppArmor, inspect and adapt the enforcing web-server profile; see
:ref:`section-webui-apparmor`. Do not disable either security system.

Upgrading from Bareos 25
------------------------

The package rename uses Bareos **25.1.3** as a bridge release.
The PHP rename must be available in that release before this upgrade path
can be used: first upgrade Bareos 25 to 25.1.3 or later and verify that
:file:`bareos-webui-php` is installed, then upgrade to Bareos 26.
The bridge's :file:`bareos-webui` compatibility package depends on the renamed
PHP package. Keep it installed during the upgrade, particularly on Debian
systems where its dependency prevents PHP from becoming automatically
removable. It contains no PHP application code.

For prerelease packages, ``25.1.3~pre24`` (including its commit hash and
packaging release suffix) is still legacy. ``25.1.3~pre25`` and later
prereleases contain the bridge. Verify the compatibility dependency and that
:file:`bareos-webui-php` owns the PHP application files before upgrading.

On Debian-based systems the old :file:`bareos-webui-new` preview package
becomes a transitional package depending on :file:`bareos-webui-vue`.
On RPM-based systems the Vue package replaces the old preview package.
Installed PHP and Vue interfaces can therefore be retained independently.
A fresh Bareos 26 installation of :file:`bareos-webui` selects only Vue.

Do not assume that a direct upgrade from an earlier Bareos 25 release will
preserve the PHP WebUI automatically. If you skip the bridge release, back up
your WebUI configuration, explicitly install :file:`bareos-webui-php` as part
of the upgrade, and verify both the retained configuration and PHP access
before removing legacy packages.

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
Set ``director_name`` only when the real Bareos Director name differs from the
section name — this is the name the Director announces in its greeting banner.

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
* the classic WebUI can remain available while Vue workflows are evaluated

When to use which WebUI
-----------------------

Use the classic PHP WebUI when:

* you want the long-established PHP WebUI
* your operations depend on currently documented production procedures
* you prefer the most conservative choice

Use the Vue WebUI when:

* you want to evaluate the new interface
* you want to use Vue workflows alongside or instead of the classic UI

For now, the recommended approach is to keep both interfaces available and let
users compare them against their operational needs.

Further information
-------------------

For developer-oriented details, see :ref:`section-dev-webui-vue`.
