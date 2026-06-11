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
