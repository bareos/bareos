# OIDC Authentication for Bareos WebUI (webui-vue)

## Problem Statement

The webui-vue currently authenticates users **exclusively** via username/password
against the Bareos Director using CRAM-MD5 over a WebSocket connection. There is
no support for federated identity. Adding OIDC support allows organisations to use
an existing Identity Provider (Keycloak, Okta, Azure AD, Authentik, etc.) for SSO.

Password login is **preserved** alongside OIDC — administrators not using an IdP
are unaffected.

---

## Architecture Overview

The Director owns all OIDC policy. The proxy is a stateless bridge — no
SQLite, no admin account, no JWT validation, no credential storage.

```
┌──────────────────────────────────────────────────────────────────────┐
│  Browser (webui-vue SPA)                                             │
│  ┌──────────────┐  1. fetch /oidc-config.json                       │
│  │  LoginPage   │──────────────────────────────► Apache (static)    │
│  │              │◄─────────────────────────────  {enabled, ...}     │
│  │  [Password]  │  2. PKCE auth code flow                           │
│  │  [Login SSO] │─────────────────────────────► IdP (Keycloak/…)   │
│  └──────────────┘◄──────────────── redirect /callback?code=…        │
│          │         3. exchange code → access_token (at IdP)         │
│          │         4. WS connect /ws                                 │
│          │──── { type:"oidc_auth", access_token:"…" } ─────────────►│
└──────────┼───────────────────────────────────────────────────────────┘
           │
    Apache reverse-proxy (/ws)
           │
┌──────────▼───────────────────────────────────────────────────────────┐
│  bareos-webui-proxy (C++) — stateless bridge                         │
│  On "oidc_auth" WS message:                                          │
│    → open Director TCP connection as the "webui-oidc" console        │
│    → run new OIDC console auth protocol (send name, recv prompt,     │
│      send JWT, recv auth_ok/auth_error)                              │
│    → relay result back to browser                                    │
│  No JWT validation, no credentials stored, no SQLite                 │
└──────────────────────────────────────────────────────────────────────┘
           │  new OIDC console auth protocol
┌──────────▼───────────────────────────────────────────────────────────┐
│  Bareos Director (extended)                                          │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  OidcConsoleAuthenticator:                                   │   │
│  │  a) Send "send-oidc-token" prompt to client                  │   │
│  │  b) Receive raw JWT access token                             │   │
│  │  c) Fetch & cache JWKS from IdP           (oidc_jwks.cc)    │   │
│  │  d) Verify signature, exp, iss, aud       (oidc_jwt.cc)     │   │
│  │  e) Extract groups → resolve profile  (oidc_group_mapping.cc)│   │
│  │  f) Set ua_->user_acl = resolved profile                    │   │
│  │  g) Set session identity = UserClaim value (e.g. "alice")   │   │
│  └──────────────────────────────────────────────────────────────┘   │
│  New config resources: OidcProfile { Issuer; GroupMapping; ... }    │
└──────────────────────────────────────────────────────────────────────┘
```

### Director configuration example

```
OidcProfile {
  Name           = corporate-idp
  Issuer         = "https://idp.example.com/realms/bareos"
  Audience       = "bareos-webui"
  UserClaim      = preferred_username   # shown in audit logs as "alice"
  GroupClaim     = groups
  GroupMapping { Group = "bareos-admins";    Profile = webui-admin    }
  GroupMapping { Group = "bareos-operators"; Profile = operator       }
  DefaultProfile = webui-readonly       # omit to deny unmatched users
}

Console {
  Name                  = webui-oidc
  UseOidcAuthentication = yes
  OidcProfile           = corporate-idp
}
```

No per-user Bareos console resources needed. The `webui-oidc` Console is a
single gateway; the resolved profile determines ACLs per session.

---

## Component Breakdown

### 1. OIDC configuration discovery

The SPA fetches `/oidc-config.json` on boot. The proxy writes this file on
startup from env vars. A 404 means OIDC is disabled and only password login
is available.

```json
{
  "enabled":   true,
  "issuer":    "https://idp.example.com/realms/bareos",
  "client_id": "bareos-webui",
  "scopes":    "openid email profile"
}
```

No client secret is included — the SPA uses PKCE only.

### 2. Director changes (`core/src/dird/`)

This is the bulk of the work. The Director handles all OIDC trust decisions.

**New source files**:

| File | Responsibility |
|---|---|
| `oidc_jwks.h/.cc` | Fetch `{issuer}/.well-known/openid-configuration` → `jwks_uri` → JWKS JSON. Cache keys by `kid`. Auto-refresh on miss or TTL (1h). Thread-safe. Uses libcurl (already a Director dep). |
| `oidc_jwt.h/.cc` | Base64url-decode JWT header+payload+signature. Look up key by `kid`. Verify RS256/ES256 via OpenSSL EVP. Validate `exp`/`nbf`/`iss`/`aud`. Return parsed claims map on success. Stateless pure function — easy to unit test with mock JWKS. |
| `oidc_group_mapping.h/.cc` | Given OIDC group list + `OidcProfile*`: walk `GroupMapping` entries in config order, return first matching `ProfileResource*`. Fall back to `DefaultProfile`. Return `nullptr` to deny if no match and no default. |

**Modified files**:

- `dird_conf.h/.cc` — new `OidcProfile` resource type (fields: `Issuer`,
  `Audience`, `UserClaim`, `GroupClaim`, `GroupMapping` sub-items,
  `DefaultProfile`). New fields on `ConsoleResource`: `UseOidcAuthentication`
  (bool, default `false`) and `OidcProfile` (resource pointer). Modelled on
  the existing `UsePamAuthentication` pattern.

- `authenticate_console.cc` — new `OidcConsoleAuthenticator` class alongside
  existing `ConsoleAuthenticator` and `PamConsoleAuthenticator`:
  1. Detects `UseOidcAuthentication = yes` on the Console resource
  2. Sends `"send-oidc-token"` prompt to client
  3. Receives raw JWT access token string
  4. Calls `OidcJwtValidate()` against JWKS cache
  5. Calls `ResolveOidcProfile()` with extracted groups
  6. Sets `ua_->user_acl` from resolved profile
  7. Sets session identity to `UserClaim` value (e.g. `"alice"`) for audit logs

### 3. Proxy changes (`core/src/webui-proxy/`)

Greatly simplified vs. earlier options — no new C++ dependencies.

**`proxy_session.cc`** — add `"oidc_auth"` branch in the first-message dispatch:
1. Extract `access_token` from the WS JSON message
2. Open Director TCP connection as the OIDC console (e.g. `webui-oidc`)
3. Run OIDC console auth protocol: send name → recv `"send-oidc-token"` →
   send JWT → recv `auth_ok` / `auth_error`
4. Relay result to browser; on success enter the normal command loop

Password auth branch (`"auth"`) is completely unchanged.

**New env vars** (written to `oidc-config.json` on startup):

| Variable | Description |
|---|---|
| `OIDC_ENABLED` | `1` to activate OIDC support |
| `OIDC_ISSUER` | Written into `oidc-config.json` |
| `OIDC_CLIENT_ID` | Written into `oidc-config.json` |
| `OIDC_SCOPES` | Written into `oidc-config.json` |
| `OIDC_CONFIG_OUT` | Path to write `oidc-config.json` on startup |

No `OIDC_ADMIN_USER`, `OIDC_ADMIN_PASSWORD`, `OIDC_USER_DB`, or group mapping
files — all policy lives in the Director config.

### 4. Frontend changes (`webui-vue/src/`)

**New dependency**: `oidc-client-ts` — handles PKCE, token storage, silent
renewal, and logout redirect.

**New files**:
- `src/stores/oidc.js` — Pinia store wrapping `UserManager`; exposes
  `startLogin()`, `completeLogin()`, `getAccessToken()`, `logout()`.
  Lazily initialised from the fetched `/oidc-config.json`.
- `src/pages/OidcCallbackPage.vue` — handles `/callback?code=…&state=…`
  redirect from IdP; completes token exchange, connects WebSocket, pushes to
  `/dashboard`.

**Modified files**:
- `src/router/index.js` — add `/callback` route (public, no `requiresAuth`).
- `src/stores/auth.js` — add `authMethod` (`'password'` | `'oidc'`);
  `loginWithOidc(username, director)` stores identity without a password.
- `src/stores/director.js` — add `connectWithOidc(accessToken)`: opens WS and
  sends `{ type: "oidc_auth", access_token }` as the first message.
- `src/pages/LoginPage.vue` — on mount, fetch `/oidc-config.json`; if
  `enabled: true`, show "Login with SSO" button. Password form always visible.
- `src/App.vue` — on page reload, if `authMethod === 'oidc'`, get access token
  from `oidcStore` (triggering silent renewal if needed) and reconnect.
- `src/layouts/MainLayout.vue` — logout calls `oidcStore.logout()` which
  triggers `UserManager.signoutRedirect()` to end the IdP session.

**Login flow**:
1. User clicks "Login with SSO" → PKCE redirect to IdP
2. IdP authenticates user, redirects to `/callback?code=…`
3. `OidcCallbackPage` exchanges code for tokens at IdP
4. `director.connectWithOidc(access_token)` opens WS, sends `oidc_auth` message
5. Proxy forwards JWT to Director; Director validates + resolves profile
6. Director sends `auth_ok`; proxy relays to browser
7. `auth.loginWithOidc(…)` called; router pushes to `/dashboard`

**Token lifecycle**: `oidc-client-ts` handles silent renewal via a hidden
iframe. On page reload, existing token is reused if valid; expired tokens
trigger silent renewal or a full redirect to the IdP.

---

## Design Decisions

| Question | Decision |
|---|---|
| Both login methods on same page? | Yes — password form always visible; SSO button shown only when `oidc-config.json` is present |
| OIDC group change mid-session? | Existing WS session unaffected; new profile applies on next login |
| No matching group and no `DefaultProfile`? | Director denies auth; user sees login error |
| Token revocation? | Revoke at IdP; `exp` validated on every new WS connection |
| Multiple IdPs? | Multiple `OidcProfile` + `Console` resources in Director; `oidc_auth` WS message can include a `console` field to select the gateway |

---

## Implementation Plan

### Phase 1 — Director: config resources

1. **`director-oidc-resource`** — New `OidcProfile` resource type in
   `dird_conf.h/.cc` with all fields and config parser support.
2. **`director-oidc-console-config`** — `UseOidcAuthentication` (bool) and
   `OidcProfile` (resource pointer) on `ConsoleResource`.

### Phase 2 — Director: OIDC validation logic

3. **`director-jwks-client`** — `oidc_jwks.h/.cc`: JWKS HTTP client + key
   cache.
4. **`director-jwt-validate`** — `oidc_jwt.h/.cc`: JWT signature + claims
   validation.
5. **`director-group-mapping`** — `oidc_group_mapping.h/.cc`: group list →
   `ProfileResource*`.
6. **`director-oidc-tests`** — GoogleTest unit tests with mock JWKS (no
   network): JWT decode/validate, group mapping priority and fallback.

### Phase 3 — Director: auth protocol

7. **`director-oidc-auth-protocol`** — `OidcConsoleAuthenticator` in
   `authenticate_console.cc`.
8. **`director-oidc-session-identity`** — Store OIDC `UserClaim` value in
   `UaContext` so Director audit logs show `"alice"` not `"webui-oidc"`.

### Phase 4 — Proxy: token forwarding

9. **`proxy-oidc-deps`** — Confirm no new proxy CMake dependencies needed
   (JWKS/JWT/OpenSSL all moved to Director).
10. **`proxy-oidc-validator`** — Token forwarding logic in `proxy_session.cc`:
    extract JWT, run OIDC console auth protocol with Director.
11. **`proxy-oidc-session`** — Wire `"oidc_auth"` into first-message dispatch.
12. **`proxy-config`** — OIDC env vars in `main.cc`; write `oidc-config.json`
    on startup.
13. **`proxy-oidc-tests`** — Tests for proxy OIDC forwarding paths.

### Phase 5 — Frontend

14. **`frontend-oidc-dep`** — Add `oidc-client-ts` to `package.json`.
15. **`frontend-oidc-store`** — `src/stores/oidc.js` Pinia store.
16. **`frontend-callback-page`** — `OidcCallbackPage.vue` + `/callback` route.
17. **`frontend-login-sso`** — SSO button in `LoginPage.vue`.
18. **`frontend-director-oidc`** — `connectWithOidc()` in `director.js`.
19. **`frontend-auth-oidc`** — Auth method tracking in `auth.js`.
20. **`frontend-app-reconnect`** — OIDC reconnect logic in `App.vue`.
21. **`frontend-logout-oidc`** — IdP session end in `MainLayout.vue`.
22. **`frontend-build`** — Rebuild `dist/` with `npm run build`.

### Phase 6 — Docs & packaging

23. **`docs-oidc`** — Setup guide: Director `OidcProfile` config reference,
    Keycloak realm/client walkthrough (public client, PKCE, redirect URIs),
    Okta/Azure AD notes, multiple IdP setup, token revocation, troubleshooting.
24. **`packaging-oidc`** — `webui-oidc-console.conf.example` in
    `/etc/bareos/`; OIDC env var comments in the proxy systemd unit template;
    note that no runtime dep is added to the proxy package.
