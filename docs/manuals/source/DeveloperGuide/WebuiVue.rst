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

The Vue WebUI uses independent, flat JSON translation catalogs that are
loaded directly by Vue I18n. They are stored in:

* :file:`webui-vue/src/i18n/locales/locales.json` for the locale list
* :file:`webui-vue/src/i18n/locales/en_EN.json` for the English source terms
* :file:`webui-vue/src/i18n/locales/<locale>.json` for translations

The English message is also the stable key, for example:

.. code-block:: json

   {
     "Log in": "Anmelden",
     "Password": "Passwort"
   }

This keeps calls such as ``t('Log in')`` readable and matches the term model
used by the existing WebUI POEditor project. Empty translations are ignored
at runtime and fall back to English.

Adding or changing translatable text
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Literal calls to ``t('…')`` and ``translate('…')`` are extracted
automatically. Text stored in data and translated later through
``t(value)`` must be marked where it is defined:

.. code-block:: javascript

   import { messageId } from '../i18n/messageId.js'

   const action = {
     label: messageId('Run action'),
   }

After changing translatable source text, update and check the catalogs:

.. code-block:: shell-session

   cd webui-vue
   npm run update:i18n
   npm run check:i18n

``update:i18n`` adds new keys, removes keys no longer used by Vue, preserves
existing translations and sorts every catalog. ``check:i18n`` makes no
changes and verifies source synchronization, locale parity, JSON value
types and interpolation placeholders such as ``{count}``.

Translating with POEditor
~~~~~~~~~~~~~~~~~~~~~~~~~

The existing Bareos WebUI project is continued for the Vue WebUI:
https://poeditor.com/join/project/ELnLNbvQJb.

The Bareos team maintains the project as follows:

1. Run ``npm run update:i18n``.
2. Import :file:`en_EN.json` into POEditor as Key-Value JSON and synchronize
   the project terms, removing terms which are no longer present.
3. Export each language as Key-Value JSON to its corresponding file in
   :file:`webui-vue/src/i18n/locales/`.
4. Run ``npm run update:i18n`` to add any untranslated keys omitted by the
   export, then run ``npm run check:i18n`` and the unit tests before
   committing.

When the project was converted to Vue JSON, matching translations were
seeded once from the classic PHP WebUI catalogs. PHP-only terms were not
copied. The Vue translation scripts and runtime do not depend on the
classic WebUI, so removing it does not affect Vue translations.

The temporary ``MESSAGE_OVERRIDES`` block in
:file:`webui-vue/src/i18n/index.js` still takes precedence over JSON
translations for those messages. Consequently, edits to those specific
terms in POEditor do not become visible until the override block is removed.

Adding a language
~~~~~~~~~~~~~~~~~

Add the locale and its display label to :file:`locales.json`, add the
corresponding JSON catalog import in :file:`locales/catalogs.js`, import the
language from POEditor, and update the locale mappings in
:file:`webui-vue/src/utils/locales.js`. Then run ``npm run check:i18n``.

Documentation guidance
----------------------

When documenting Vue WebUI work:

* add operator-facing documentation to the introduction/tutorial manual
* add implementation and workflow details here in the developer guide
* clearly mark the Vue WebUI as a technical preview while it coexists with the
  classic PHP WebUI
* avoid wording that implies the classic WebUI no longer exists unless that
  product decision has changed
