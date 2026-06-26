.. _section-dev-webui-vue:

WebUI Vue
=========

The Vue-based WebUI lives in :file:`webui-vue/` and is currently documented as
a technical preview that exists in parallel with the classic PHP-based WebUI
in :file:`webui/`.

Coexistence model
-----------------

The repository currently contains two different WebUI implementations:

* :file:`webui/` for the classic PHP-based WebUI
* :file:`webui-vue/` for the Vue-based technical preview

Documentation, packaging, and operational guidance should reflect that both
variants currently coexist.
Changes to the Vue WebUI should not implicitly assume that it has already
replaced the classic WebUI everywhere.

Repository layout
-----------------

The most relevant parts of the Vue WebUI are:

* :file:`webui-vue/src/` for the application source
* :file:`webui-vue/src/pages/` for page-level route components
* :file:`webui-vue/src/components/` for reusable UI components
* :file:`webui-vue/src/composables/` for shared page and data-loading logic
* :file:`webui-vue/src/stores/` for Pinia stores
* :file:`webui-vue/src/generated/` for generated version and translation data
* :file:`webui-vue/tests/unit/` for unit tests
* :file:`webui-vue/tests/e2e/` for Playwright-based end-to-end tests

Build and installation
----------------------

The installable SPA bundle is generated from the repository root with:

.. code-block:: shell-session

   cmake --build <builddir> --target bareos-webui-vue-build

The build logic is implemented in:

* :file:`webui-vue/CMakeLists.txt`
* :file:`webui-vue/build-dist.cmake`

The installed Apache configuration is generated from:

* :file:`webui-vue/install/apache/bareos-webui-vue.conf.in`

The SPA is installed below :file:`${CMAKE_INSTALL_FULL_DATAROOTDIR}/bareos-webui-vue`
and exposed by Apache below :file:`/bareos-webui-vue`.

Runtime architecture
--------------------

Unlike the classic PHP WebUI, the Vue WebUI does not talk to the Director
through PHP.
Instead it uses :command:`bareos-webui-proxy` for both HTTP session handling
and WebSocket-based director communication.

The default Apache configuration:

* serves the SPA from :file:`/bareos-webui-vue`
* rewrites SPA routes back to :file:`index.html`
* proxies :file:`/ws` to :command:`bareos-webui-proxy` on port **9104**
* proxies :file:`/api/` to :command:`bareos-webui-proxy` on port **9104**

The HTTP side currently provides the session endpoints
:file:`/api/session`, :file:`/api/session/login`, and
:file:`/api/session/logout`.
The WebSocket side is then used for the live director connection once the
session has been established.

The proxy configuration behavior is:

* without ``--config``, it first tries
  :file:`/etc/bareos-webui-proxy/bareos-webui-proxy.ini`
* if that file is missing, it uses built-in defaults
* with explicit ``--config``, the specified file is required

The default configuration template is installed as
:file:`bareos-webui-proxy.ini` in :file:`${configtemplatedir}`.

Testing
-------

Typical local validation commands are:

.. code-block:: shell-session

   cd webui-vue && npm run build
   cd webui-vue && npm run test:unit
   ctest --test-dir cmake-build --output-on-failure -R '^webui-vue:'

The browser-based system tests use the shared WebUI Vue test setup under:

* :file:`systemtests/tests/webui-vue-common/`

Translations
------------

The Vue WebUI currently reuses the legacy WebUI locale sources and generated
catalog data.

Relevant files are:

* :file:`webui-vue/scripts/generate-webui-i18n.mjs`
* :file:`webui-vue/src/generated/webui-locales.js`
* :file:`webui-vue/src/generated/webui-messages.js`
* :file:`webui/module/Application/language/`

To regenerate the committed Vue translation catalog files, run:

.. code-block:: shell-session

   cd webui-vue && npm run generate:i18n

Documentation guidance
----------------------

When documenting Vue WebUI work:

* add operator-facing documentation to the introduction/tutorial manual
* add implementation and workflow details here in the developer guide
* clearly mark the Vue WebUI as a technical preview while it coexists with the
  classic PHP WebUI
* avoid wording that implies the classic WebUI no longer exists unless that
  product decision has changed
