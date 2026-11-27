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

* :file:`webui-vue/install/apache/bareos-webui-new.conf.in`

The SPA is installed below :file:`${CMAKE_INSTALL_FULL_DATAROOTDIR}/bareos-webui-new`
and exposed by Apache below :file:`/bareos-webui-new`.

Runtime architecture
--------------------

Unlike the classic PHP WebUI, the Vue WebUI does not talk to the Director
through PHP.
Instead it uses :command:`bareos-webui-proxy` for both HTTP session handling
and WebSocket-based director communication.

The default Apache configuration:

* serves the SPA from :file:`/bareos-webui-new`
* rewrites SPA routes back to :file:`index.html`
* proxies :file:`/ws` to :command:`bareos-webui-proxy` on port **9104**
* proxies :file:`/api/` to :command:`bareos-webui-proxy` on port **9104**

The HTTP side provides :file:`/api/session` for session lookup/deletion and
:file:`/api/session/directors/{selector}/login` for Director login.
The WebSocket side is then used for the live director connection once the
session has been established.

The proxy configuration behavior is:

* without ``--config``, it first tries
  :file:`/etc/bareos-webui-proxy/bareos-webui-proxy.ini`
* if that file is missing, it uses built-in defaults
* with explicit ``--config``, the specified file is required

Any existing configuration file must satisfy
:ref:`webui-proxy-config-permissions`, regardless of whether it enables PAM.
The loader checks the opened file and immediate containing directory before
parsing. Unsafe access rights cause startup failure with an actionable diagnostic.

The default configuration template is installed as
:file:`bareos-webui-proxy.ini` in :file:`${configtemplatedir}`.

PAM login
---------

The existing Vue login form and API both accept only the user's username and
password. For a PAM-enabled Director target, configure ``pam_console_name`` and
``pam_console_password`` in that target's proxy INI section. The proxy uses
these server-side credentials for TLS-PSK and CRAM-MD5 Console authentication.
It interprets the submitted username/password as the PAM user's credentials:

.. code-block:: json

   {
     "username": "<pam-user>",
     "password": "<pam-password>"
   }

Post this payload to :file:`/api/session/directors/{selector}/login`.
The Director's Console must have ``UsePamAuthentication = yes``; login fails
if it does not request PAM, rather than granting the Console's own access.
A matching Director User resource determines the authenticated user's access.
PAM usernames and passwords must be nonempty and contain no NUL or
record-separator characters. Separate ``pam_username``/``pam_password`` API
fields are not accepted.
Malformed requests return HTTP 400 and authentication failures return HTTP 401.

This implements bconsole's direct-credentials mode, not interactive PAM
prompts. Configuring PAM with ``tls_psk_disable = yes`` is a startup error.
Targets without the PAM Console settings retain ordinary Console login.
Protect browser/API traffic with HTTPS as well; Director TLS protects only
the proxy-to-Director connection.

After successful login, use the session cookie for subsequent requests.
WebSocket sessions use ``{"type":"session","director":"<selector>","mode":"raw"}``
for a text console, or ``"mode":"json"`` for JSON commands.
The proxy retains the user's credentials in server-side session memory for
reconnects and explicit credential reuse. Each Director connection uses that
target's configured Console credentials, including when reusing the same user
credentials across Directors. Shared Console credentials remain in the
restricted server-side INI file, not browser requests or session records.
Session expiry, logout, or removing a Director removes its stored user
credentials. Session responses identify the user but never expose passwords
or the shared Console credentials. See :ref:`section-webui-vue` for setup.

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
