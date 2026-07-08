.. _section-webui-vue:

Bareos WebUI Vue
================

.. index::
   single: WebUI Vue
   single: WebUI; Vue
   single: WebUI; technical preview

The Vue-based Bareos WebUI is a new single-page application located in
:file:`webui-vue/`.
It provides a modern WebUI implementation that coexists with the classic
PHP-based :ref:`section-webui`.

.. warning::

   The Vue-based WebUI is currently a technical preview.
   It is intended for evaluation, early adoption, and iterative development.
   The classic PHP-based WebUI remains available in parallel and should still
   be considered the default WebUI for production environments unless the Vue
   WebUI preview has been explicitly evaluated for the intended use case.

Coexistence with the classic WebUI
----------------------------------

The current Bareos documentation distinguishes between two WebUI variants:

* :ref:`section-webui` documents the classic PHP-based WebUI.
* This page documents the new Vue-based WebUI technical preview.

The two interfaces are designed to exist in parallel.
In a typical Apache deployment they use separate URLs:

* classic WebUI: :file:`/bareos-webui`
* Vue WebUI preview: :file:`/bareos-webui-vue`

The Vue WebUI uses the same Bareos Director console concepts as the classic
WebUI, so user access still depends on properly configured
:config:option:`Dir/Console` resources and matching ACLs.

Architecture overview
---------------------

The Vue WebUI consists of:

* the static SPA bundle installed below :file:`/usr/share/bareos-webui-vue`
* an Apache configuration that exposes the bundle below
  :file:`/bareos-webui-vue`
* the :command:`bareos-webui-proxy` service, which accepts both HTTP session
  requests and WebSocket connections and forwards them to the Bareos Director

The default Apache configuration proxies both :file:`/ws` and
:file:`/api/` to :command:`bareos-webui-proxy` on port **9104**.
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

The exact package layout depends on the target platform.
Where packaging support is available, the Vue WebUI is installed separately
from the classic PHP WebUI and keeps its own Apache configuration file
:file:`bareos-webui-vue.conf`.

For source builds, the installable bundle is generated with:

.. code-block:: shell-session

   cmake --build <builddir> --target bareos-webui-vue-build

After installation, open the preview at:

.. code-block:: text

   http://HOSTNAME/bareos-webui-vue

To use it successfully, ensure that:

* the Vue WebUI bundle is installed
* the Apache configuration for :file:`/bareos-webui-vue` is enabled
* :command:`bareos-webui-proxy` is installed and running
* the HTTP and WebSocket proxy targets on port **9104** are reachable from
  Apache
* suitable Bareos Director console credentials exist

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

**``[listen]`` section options**

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
   * - ``session_idle_timeout_minutes``
     - ``30``
     - Minutes of inactivity before a session is invalidated.
   * - ``session_absolute_lifetime_hours``
     - ``8``
     - Maximum session lifetime in hours regardless of activity.
   * - ``max_unauthenticated_connections``
     - ``100``
     - Maximum concurrent connections that have not yet completed login.

**``[<director>]`` section options**

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

The technical preview already covers important day-to-day workflows, including:

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

Compared to the classic PHP WebUI, the Vue WebUI preview should currently be
viewed as a parallel implementation under active development.

Important differences are:

* documentation and operational guidance are still catching up
* some workflows are intentionally presented differently than in the classic
  WebUI instead of being a 1:1 port
* the preview depends on :command:`bareos-webui-proxy`, which is a separate
  runtime component
* the classic WebUI should remain available while the preview is being
  evaluated

When to use which WebUI
-----------------------

Use the classic PHP WebUI when:

* you want the long-established, primary WebUI
* your operations depend on currently documented production procedures
* you prefer the most conservative choice

Use the Vue WebUI preview when:

* you want to evaluate the new interface
* you want to test current preview workflows in parallel with the classic UI
* you are helping validate the future WebUI direction

For now, the recommended approach is to keep both interfaces available and let
users compare them against their operational needs.

Further information
-------------------

For developer-oriented details, see :ref:`section-dev-webui-vue`.
