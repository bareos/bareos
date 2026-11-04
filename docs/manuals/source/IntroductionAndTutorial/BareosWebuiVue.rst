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
* Vue WebUI preview: :file:`/bareos-webui-new`

The Vue WebUI uses the same Bareos Director console concepts as the classic
WebUI, so user access still depends on properly configured
:config:option:`Dir/Console` resources and matching ACLs.

Architecture overview
---------------------

The Vue WebUI consists of:

* the static SPA bundle installed below :file:`/usr/share/bareos-webui-new`
* an Apache configuration that exposes the bundle below
  :file:`/bareos-webui-new`
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
Every configuration file, with or without PAM settings, must meet the
:ref:`webui-proxy-config-permissions` requirements. Missing implicit configuration
still uses built-in defaults.

Installation and access
-----------------------

The Vue WebUI is installed separately from the classic PHP WebUI via the
:file:`bareos-webui-new` package and keeps its own Apache configuration file
:file:`bareos-webui-new.conf`.

After installation, open the preview at:

.. code-block:: text

   http://HOSTNAME/bareos-webui-new

To use it successfully, ensure that:

* the Vue WebUI bundle is installed
* the Apache configuration for :file:`/bareos-webui-new` is enabled
* :command:`bareos-webui-proxy` is installed and running
* the HTTP and WebSocket proxy targets on port **9104** are reachable from
  Apache
* suitable Bareos Director console credentials exist

Upgrading existing installations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. warning::

   Configuration access rights are now enforced for every existing proxy
   configuration, including configurations without PAM settings. Previously
   accepted modes such as ``0644`` for the INI file or ``0755`` for its
   containing directory prevent the upgraded proxy from starting, making the
   Vue WebUI unavailable.

Before upgrading an installation with an active configuration file, check the
proxy service account and apply the ownership and ``0640``/``0750`` permissions
described in :ref:`webui-proxy-config-permissions`. Apply this to
:file:`/etc/bareos-webui-proxy/bareos-webui-proxy.ini`, or the file selected by
``--config``, and its immediate containing directory. Preserve the existing
configuration contents; replacing the file with the shipped template is not
required.

If the upgrade has already completed, correct the access rights before
restarting :command:`bareos-webui-proxy`. Use the journal troubleshooting
commands in :ref:`webui-proxy-config-permissions` if startup fails.
Installations without an active implicit configuration file are unaffected
and continue to use built-in defaults.

Configuring multiple directors
------------------------------

By default, :command:`bareos-webui-proxy` connects to a single local Director.
To make multiple Directors selectable in the WebUI, copy the shipped config
template into place and add one section per Director:

.. code-block:: shell-session

   cp /usr/lib/bareos/defaultconfigs/bareos-webui-proxy.ini \
      /etc/bareos-webui-proxy/bareos-webui-proxy.ini

Set ownership and permissions as described in
:ref:`webui-proxy-config-permissions` before starting the proxy.

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
       Not allowed with PAM Console settings.
   * - ``pam_console_name``
     - *(unset)*
     - Name of the Director Console resource used for PAM authentication.
       Set together with ``pam_console_password``.
   * - ``pam_console_password``
     - *(unset)*
     - Shared password of the PAM Console resource, stored only on the proxy.
       Users enter their individual PAM passwords, not this secret.

PAM login
---------

Users log in through the normal Vue username/password form using their
individual PAM credentials. Administrators configure the shared Console
credentials on the proxy, so users never need to know or submit them.

On the Director, configure a PAM-enabled Console and a User resource for each
permitted PAM user:

.. code-block:: bareosconfig

   Console {
     Name = "PamConsole"
     Password = "<shared-console-secret>"
     UsePamAuthentication = yes
   }

   User {
     Name = "alice"
     Profile = "webui-admin"
   }

Choose a Profile appropriate for that user's permissions. Configure the
``bareos`` PAM service on the Director host, including authentication and
account rules, as described in :doc:`/TasksAndConcepts/PAM`.

On the proxy, add both Console settings to the corresponding Director section:

.. code-block:: ini

   [listen]
   address = localhost
   port = 9104

   [production]
   address = director.example.org
   director_name = bareos-dir
   tls_psk_disable = no
   pam_console_name = PamConsole
   pam_console_password = "<shared-console-secret>"

The Console name/password must match the Director resource. Quote passwords
containing ``#`` or ``;`` so they are not treated as comments. In quoted values,
escape a literal double quote as ``\"`` and a backslash as ``\\``.
Both settings must be nonempty; an incomplete pair or disabled TLS-PSK prevents
proxy startup. A Console without ``UsePamAuthentication = yes`` cannot be used
for this login mode.

Select ``production`` in the Vue login form, then enter ``alice`` and Alice's
PAM password. The Director authenticates Alice and applies her User/Profile
permissions. Interactive PAM prompts and MFA conversations are not supported.
Directors without the PAM settings continue to use Console name/password
login. Reconnects use the user's credentials retained in proxy session memory;
logout or session expiry removes them.

.. _webui-proxy-config-permissions:

Configuration file access rights
--------------------------------

Restrict the INI file to administrators and the proxy service account
(mode ``0640`` or stricter, with the proxy's effective service group).
The containing directory must have mode ``0750`` or stricter. Both must be
owned by root or the proxy's effective user, with no access for other users
and no group write access. Executable configuration files and special mode
bits are rejected. If group access is enabled, the group must match the proxy's
effective group. These checks are enforced before parsing every configuration
file, even without PAM settings; unsafe permissions prevent startup.
Starting without an implicit configuration file still uses built-in defaults.

For the usual ``bareos`` service group:

.. code-block:: shell-session

   chown root:bareos /etc/bareos-webui-proxy
   chmod 0750 /etc/bareos-webui-proxy
   chown root:bareos /etc/bareos-webui-proxy/bareos-webui-proxy.ini
   chmod 0640 /etc/bareos-webui-proxy/bareos-webui-proxy.ini

Check the installed service's account before choosing ownership:

.. code-block:: shell-session

   systemctl show bareos-webui-proxy -p User -p Group

The proxy exits with a fatal error for unsafe configuration access rights. The message
identifies the file or directory, its current octal mode and numeric owner/group,
the failed requirement, and the recommended ``chmod 0640`` or ``chmod 0750``
fix. Ownership/group errors also report the permitted service UID/GID.
For example, a world-readable INI produces an ``unsafe configuration file``
error with ``mode 0644`` and ``chmod 0640``. An overly accessible directory
produces ``unsafe configuration directory`` and ``chmod 0750``.
Neither message includes the Console password.

If the service cannot open the file or directory at all, the error instead
reports the affected path and operating-system reason, with a reminder to
check the service's read/search permissions. Inspect service errors with:

.. code-block:: shell-session

   journalctl -u bareos-webui-proxy -n 50 --no-pager

Configuration files and their immediate containing directory must not be
symbolic links. The loader checks permissions on the opened file and directory,
not a separate filename lookup. Non-PAM files have the same requirements.
Do not place the file in the SPA's static directory.
Restart :command:`bareos-webui-proxy` after
editing it. Use HTTPS between the browser and web server as well as TLS-PSK
between the proxy and Director.

What is already available
-------------------------

The technical preview already covers important day-to-day workflows, including:

* login with Bareos console credentials
* PAM login with server-configured Console credentials
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

Providing feedback
------------------

Because the Vue WebUI is a technical preview under active development, user
feedback is especially valuable at this stage.

Please open an issue on `GitHub Issues <https://github.com/bareos/bareos/issues/>`_
for any of the following:

* **Misbehaviour** — anything that does not work as expected or produces
  errors
* **Missing functionality** — features present in the classic PHP WebUI that
  are absent or incomplete in the Vue WebUI
* **New feature requests** — ideas for functionality you would like to see
  in the new WebUI that go beyond what the classic WebUI offers

When reporting, please include the Bareos version, browser, and a short
description of the steps to reproduce the issue.

Further information
-------------------

For developer-oriented details, see :ref:`section-dev-webui-vue`.
