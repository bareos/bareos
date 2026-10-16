<?php
#
# Run the same-origin CSRF decision table of the webui
# against a set of hand-written requests.
#
# This is a standalone script on purpose: it needs no Bareos
# installation, no director and no database, just PHP.
#

//   BAREOS® - Backup Archiving REcovery Open Sourced
//
//   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
//
//   This program is Free Software; you can redistribute it
//   and/or modify it under the terms of version three of the GNU Affero
//   General Public License as published by the Free Software Foundation
//   and included in the file LICENSE.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU Affero General Public License for more details.
//
//   You should have received a copy of the GNU Affero General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
//   02110-1301, USA.

require_once __DIR__ . '/../../vendor/autoload.php';
require_once __DIR__ . '/../../module/Application/src/Application/Http/RequestOrigin.php';
require_once __DIR__ . '/../../module/Application/src/Application/Listener/SameOriginListener.php';

use Application\Http\RequestOrigin;
use Application\Listener\SameOriginListener;

const ORIGIN = 'https://webui.example.com';

// A request served over TLS. Every case whose Origin is https needs this,
// because the listener reconstructs the expected origin from the connection.
function tls(array $server)
{
    return array_merge(array('HTTPS' => 'on', 'HTTP_HOST' => 'webui.example.com'), $server);
}

// Each case is [server params, expected status].
//
// classify() does not see the method: which methods the check applies to is
// scope, decided by isSafeMethod(), and is covered by its own table below.
$cases = array(
    // Fetch Metadata: only same-origin passes.
    array(array('HTTP_SEC_FETCH_SITE' => 'same-origin'), SameOriginListener::OK),
    array(array('HTTP_SEC_FETCH_SITE' => 'Same-Origin'), SameOriginListener::OK),
    array(array('HTTP_SEC_FETCH_SITE' => 'cross-site'), SameOriginListener::CROSS_SITE),
    array(array('HTTP_SEC_FETCH_SITE' => 'same-site'), SameOriginListener::SAME_SITE),
    array(array('HTTP_SEC_FETCH_SITE' => 'none'), SameOriginListener::SAME_SITE),

    // Fetch Metadata wins over Origin, even when Origin would pass. A
    // sibling subdomain is same-site, and SameSite=Strict also lets the
    // session cookie ride along for it, so this is the case that needs
    // the server-side check.
    array(
        tls(array('HTTP_SEC_FETCH_SITE' => 'same-site', 'HTTP_ORIGIN' => ORIGIN)),
        SameOriginListener::SAME_SITE,
    ),

    // Origin fallback, for clients without Fetch Metadata (Safari < 16.4).
    array(tls(array('HTTP_ORIGIN' => ORIGIN)), SameOriginListener::OK),
    array(
        tls(array('HTTP_ORIGIN' => 'https://webui.example.com:8443')),
        SameOriginListener::MISMATCH,
    ),
    array(
        tls(array('HTTP_ORIGIN' => 'http://webui.example.com')),
        SameOriginListener::MISMATCH,
    ),
    array(
        tls(array('HTTP_ORIGIN' => 'https://other.example.com')),
        SameOriginListener::MISMATCH,
    ),
    // A sibling hostname is same-site, which SameSite=Strict also allows.
    array(
        tls(array('HTTP_ORIGIN' => 'https://evil.example.com')),
        SameOriginListener::MISMATCH,
    ),
    // An opaque origin arrives as the literal string 'null'.
    array(tls(array('HTTP_ORIGIN' => 'null')), SameOriginListener::MISMATCH),

    // Plain HTTP deployment, non-default port.
    array(
        array('HTTP_HOST' => '127.0.0.1:8080', 'HTTP_ORIGIN' => 'http://127.0.0.1:8080'),
        SameOriginListener::OK,
    ),

    // No proof at all is a rejection, not an allow. This is what makes the
    // webui's state-changing endpoints browser-only.
    array(array(), SameOriginListener::NO_PROOF),
    array(array('HTTP_HOST' => 'webui.example.com'), SameOriginListener::NO_PROOF),

    // A malformed Host header must not be usable to forge a match. The Origin
    // is deliberately the same malformed value, and HTTPS is on, so the host
    // pattern is the only thing that can reject these.
    array(
        tls(array('HTTP_HOST' => 'webui.example.com/evil', 'HTTP_ORIGIN' => 'https://webui.example.com/evil')),
        SameOriginListener::MISMATCH,
    ),
    array(
        tls(array(
            'HTTP_HOST' => 'webui.example.com@evil.example.com',
            'HTTP_ORIGIN' => 'https://webui.example.com@evil.example.com',
        )),
        SameOriginListener::MISMATCH,
    ),
);

/**
 * Drive a request through the real request object and the listener, not only
 * the decision table.
 */
function requestThroughListener($method, array $server, array $trustedProxies = array(), $listener = null)
{
    $request = new Laminas\Http\PhpEnvironment\Request();
    $request->setMethod($method);
    $request->setUri('https://webui.example.com/bareos-webui/auth/logout');
    foreach ($server as $name => $value) {
        $request->getServer()->set($name, $value);
    }

    $events = new Laminas\EventManager\EventManager();
    if (!$listener instanceof SameOriginListener) {
        $listener = new SameOriginListener($trustedProxies);
    }
    $listener->attach($events, 1);

    $event = new Laminas\Mvc\MvcEvent();
    $event->setRequest($request);
    $event->setResponse(new Laminas\Http\Response());
    $events->trigger(Laminas\Mvc\MvcEvent::EVENT_ROUTE, $event);

    return $event->getResponse()->getStatusCode();
}

// [method, server params, expected status code]
$failures = 0;
$listenerCases = array(
    array('POST', tls(array('HTTP_SEC_FETCH_SITE' => 'same-origin')), 200),
    array('POST', tls(array('HTTP_SEC_FETCH_SITE' => 'cross-site')), 403),
    array('POST', tls(array('HTTP_SEC_FETCH_SITE' => 'same-site')), 403),
    array('POST', tls(array()), 403),
    array('POST', tls(array('HTTP_ORIGIN' => ORIGIN)), 200),
    array('POST', tls(array('HTTP_ORIGIN' => 'https://evil.example.com')), 403),
    array('PUT', tls(array('HTTP_SEC_FETCH_SITE' => 'same-origin')), 200),
    array('DELETE', tls(array()), 403),
    // A cross-site GET must still be served, so deep links from monitoring
    // and chat clients keep working. Only state changes are gated.
    array('GET', tls(array('HTTP_SEC_FETCH_SITE' => 'cross-site')), 200),
    array('GET', tls(array()), 200),
);

foreach ($listenerCases as $index => $case) {
    list($method, $server, $expected) = $case;
    $actual = requestThroughListener($method, $server);

    if ($actual === $expected) {
        continue;
    }

    $failures++;
    printf(
        "FAIL listener case %d: %s\n  expected status: %d\n  actual status:   %d\n",
        $index,
        $method,
        $expected,
        $actual
    );
}

foreach ($cases as $index => $case) {
    list($server, $expected) = $case;
    $actual = SameOriginListener::classify($server);

    if ($actual === $expected) {
        continue;
    }

    $failures++;
    printf(
        "FAIL case %d: %s\n  expected: %s\n  actual:   %s\n",
        $index,
        json_encode($server),
        var_export($expected, true),
        var_export($actual, true)
    );
}

// Scope: which methods the check applies to. [method, expected]

$safeMethodCases = array(
    array('GET', true),
    array('HEAD', true),
    array('OPTIONS', true),
    // Matched case-insensitively.
    array('get', true),
    array('HeAd', true),
    array('POST', false),
    array('PUT', false),
    array('PATCH', false),
    array('DELETE', false),
    array('TRACE', false),
    array('', false),
    array(null, false),
);

foreach ($safeMethodCases as $index => $case) {
    list($method, $expected) = $case;
    $actual = SameOriginListener::isSafeMethod($method);

    if ($actual === $expected) {
        continue;
    }

    $failures++;
    printf(
        "FAIL safe method case %d: %s\n  expected: %s\n  actual:   %s\n",
        $index,
        var_export($method, true),
        var_export($expected, true),
        var_export($actual, true)
    );
}

// X-Forwarded-Proto is honored only when the socket peer is a listed proxy.
$forwarded = array(
    'HTTP_HOST' => 'webui.example.com',
    'HTTP_ORIGIN' => ORIGIN,
    'HTTP_X_FORWARDED_PROTO' => 'https',
    'REMOTE_ADDR' => '192.0.2.10',
);

// [server, trusted proxies, expected reason]
$proxyCases = array(
    // No trusted proxies at all: the header is ignored.
    array($forwarded, array(), SameOriginListener::MISMATCH),
    // Trusted proxy configured, and the peer is it.
    array($forwarded, array('192.0.2.10'), SameOriginListener::OK),
    // Header present but the peer is somebody else: must not be honored.
    array(array_merge($forwarded, array('REMOTE_ADDR' => '198.51.100.7')), array('192.0.2.10'), SameOriginListener::MISMATCH),
    // An exact address, from a list of several.
    array(array_merge($forwarded, array('REMOTE_ADDR' => '10.4.5.6')), array('192.0.2.10', '10.4.5.6'), SameOriginListener::OK),
    array(array_merge($forwarded, array('REMOTE_ADDR' => '10.4.5.6')), array('192.0.2.10', '10.4.5.7'), SameOriginListener::MISMATCH),
    // A CIDR range is not an address, so it matches nothing.
    array(array_merge($forwarded, array('REMOTE_ADDR' => '10.4.5.6')), array('10.0.0.0/8'), SameOriginListener::MISMATCH),
    // REMOTE_ADDR missing entirely: nothing can be trusted.
    array(
        array_diff_key($forwarded, array('REMOTE_ADDR' => null)),
        array('192.0.2.10'),
        SameOriginListener::MISMATCH,
    ),
    // A chain appends: the rightmost entry is the proxy's own, not the client's.
    array(array_merge($forwarded, array('HTTP_X_FORWARDED_PROTO' => 'https, http')), array('192.0.2.10'), SameOriginListener::MISMATCH),
    // A client-supplied value to the left must not count.
    array(array_merge($forwarded, array('HTTP_X_FORWARDED_PROTO' => 'gopher, https')), array('192.0.2.10'), SameOriginListener::OK),
    // A value that is neither http nor https must not be passed through.
    array(array_merge($forwarded, array('HTTP_X_FORWARDED_PROTO' => 'gopher')), array('192.0.2.10'), SameOriginListener::MISMATCH),
    // Whitespace around the entry is tolerated.
    array(array_merge($forwarded, array('HTTP_X_FORWARDED_PROTO' => '  https  ')), array('192.0.2.10'), SameOriginListener::OK),
    // An empty chain falls back to the observed connection scheme.
    array(array_merge($forwarded, array('HTTP_X_FORWARDED_PROTO' => ',')), array('192.0.2.10'), SameOriginListener::MISMATCH),
    // Loopback over IPv6, written the short way.
    array(array_merge($forwarded, array('REMOTE_ADDR' => '::1')), array('::1'), SameOriginListener::OK),
    // A v4 peer must not match a v6 entry, or the other way round.
    array(array_merge($forwarded, array('REMOTE_ADDR' => '192.0.2.10')), array('2001:db8::5'), SameOriginListener::MISMATCH),
    array(array_merge($forwarded, array('REMOTE_ADDR' => '2001:db8::5')), array('192.0.2.10'), SameOriginListener::MISMATCH),
    // Multiple entries, none of which match.
    array(array_merge($forwarded, array('REMOTE_ADDR' => '198.51.100.7')), array('127.0.0.1', '10.4.5.6', '::1'), SameOriginListener::MISMATCH),
    // An unusable entry must not be treated as a wildcard.
    array(array_merge($forwarded, array('REMOTE_ADDR' => '198.51.100.7')), array(''), SameOriginListener::MISMATCH),
    array(array_merge($forwarded, array('REMOTE_ADDR' => '198.51.100.7')), array('not-an-ip'), SameOriginListener::MISMATCH),
);

// X-Forwarded-Host is only trusted from a listed proxy as well, and it must
// still pass the host pattern so it cannot be used to smuggle anything.
$forwardedHost = array(
    'HTTP_HOST' => 'backend.internal:8080',
    'HTTP_ORIGIN' => ORIGIN,
    'HTTP_X_FORWARDED_HOST' => 'webui.example.com',
    'HTTP_X_FORWARDED_PROTO' => 'https',
    'REMOTE_ADDR' => '192.0.2.10',
);

$proxyCases[] = array($forwardedHost, array('192.0.2.10'), SameOriginListener::OK);
$proxyCases[] = array(
    array_merge($forwardedHost, array('REMOTE_ADDR' => '198.51.100.7')),
    array('192.0.2.10'),
    SameOriginListener::MISMATCH,
);

// Same, but with the connection scheme already https, so that only the host
// is in question. An untrusted peer must not get to rewrite the host.
$hostOnly = array(
    'HTTPS' => 'on',
    'HTTP_HOST' => 'backend.internal:8080',
    'HTTP_ORIGIN' => ORIGIN,
    'HTTP_X_FORWARDED_HOST' => 'webui.example.com',
);
$proxyCases[] = array(array_merge($hostOnly, array('REMOTE_ADDR' => '198.51.100.7')), array('192.0.2.10'), SameOriginListener::MISMATCH);
$proxyCases[] = array(array_merge($hostOnly, array('REMOTE_ADDR' => '198.51.100.7')), array(), SameOriginListener::MISMATCH);
// A trusted peer may rewrite the host, and the port travels with it.
$proxyCases[] = array(
    array_merge($hostOnly, array('REMOTE_ADDR' => '192.0.2.10', 'HTTP_X_FORWARDED_HOST' => 'webui.example.com:8443')),
    array('192.0.2.10'),
    SameOriginListener::MISMATCH,
);
$proxyCases[] = array(
    array_merge($hostOnly, array('REMOTE_ADDR' => '192.0.2.10', 'HTTP_ORIGIN' => 'https://webui.example.com:8443', 'HTTP_X_FORWARDED_HOST' => 'webui.example.com:8443')),
    array('192.0.2.10'),
    SameOriginListener::OK,
);
// A chained X-Forwarded-Host: the rightmost entry wins.
$proxyCases[] = array(
    array_merge($hostOnly, array('REMOTE_ADDR' => '192.0.2.10', 'HTTP_X_FORWARDED_HOST' => 'evil.example.com, webui.example.com')),
    array('192.0.2.10'),
    SameOriginListener::OK,
);
// A malformed forwarded host falls back to Host, which does not match here.
$proxyCases[] = array(
    array_merge($forwardedHost, array('HTTP_X_FORWARDED_HOST' => 'evil.example.com/x')),
    array('192.0.2.10'),
    SameOriginListener::MISMATCH,
);
// X-Forwarded-For must never influence the trust decision.
$proxyCases[] = array(
    array_merge($forwarded, array('REMOTE_ADDR' => '198.51.100.7', 'HTTP_X_FORWARDED_FOR' => '192.0.2.10')),
    array('192.0.2.10'),
    SameOriginListener::MISMATCH,
);

// The port is the remaining scheme signal when HTTPS is absent.
$proxyCases[] = array(
    array('HTTP_HOST' => 'webui.example.com', 'HTTP_ORIGIN' => ORIGIN, 'SERVER_PORT' => '443'),
    array(),
    SameOriginListener::OK,
);
$proxyCases[] = array(
    array('HTTP_HOST' => 'webui.example.com', 'HTTP_ORIGIN' => 'http://webui.example.com', 'SERVER_PORT' => '443'),
    array(),
    SameOriginListener::MISMATCH,
);
$proxyCases[] = array(
    array('HTTP_HOST' => 'webui.example.com', 'HTTP_ORIGIN' => ORIGIN, 'SERVER_PORT' => '80'),
    array(),
    SameOriginListener::MISMATCH,
);

foreach ($proxyCases as $index => $case) {
    list($server, $trusted, $expected) = $case;
    $actual = SameOriginListener::classify($server, $trusted);

    if ($actual === $expected) {
        continue;
    }

    $failures++;
    printf(
        "FAIL proxy case %d: %s\n  expected: %s\n  actual:   %s\n",
        $index,
        json_encode($server),
        var_export($expected, true),
        var_export($actual, true)
    );
}

// The listener has to be wired into the MVC pipeline, otherwise it is dead
// code and every state-changing request is unguarded.
$moduleConfig = require __DIR__ . '/../../module/Application/config/module.config.php';

$wiringChecks = array(
    isset($moduleConfig['listeners'])
        && in_array('Application\Listener\SameOrigin', $moduleConfig['listeners'], true),
    isset($moduleConfig['service_manager']['factories']['Application\Listener\SameOrigin']),
    isset($moduleConfig['trusted_proxies']) && is_array($moduleConfig['trusted_proxies']),
    // The nested spelling is gone; a stray one would silently do nothing.
    !isset($moduleConfig['same_origin_check']),
);

foreach ($wiringChecks as $index => $ok) {
    if ($ok) {
        continue;
    }

    $failures++;
    printf("FAIL wiring check %d\n", $index);
}

// An empty trusted_proxies list must mean "trust nothing", not "trust all".
$defaultTrusted = $moduleConfig['trusted_proxies'];
if (!is_array($defaultTrusted) || !empty($defaultTrusted)) {
    $failures++;
    printf(
        "FAIL default trusted_proxies should be an empty array, got %s\n",
        json_encode($defaultTrusted)
    );
}

// Address comparison, through isTrustedProxy().
// [configured address, peer, expected]
$addressCases = array(
    array('127.0.0.1', '127.0.0.1', true),
    array('127.0.0.1', '127.0.0.2', false),

    // The same IPv6 address written two ways compares equal, because the
    // packed form is what is compared.
    array('::1', '::1', true),
    array('::1', '0:0:0:0:0:0:0:1', true),
    array('2001:db8::1', '2001:0db8:0000:0000:0000:0000:0000:0001', true),
    array('2001:db8::1', '2001:db8::2', false),

    // A v4 address must not match a v6 one, or the other way round.
    array('127.0.0.1', '::1', false),
    array('::1', '127.0.0.1', false),

    // A CIDR range is not an address, so it matches nothing, itself included.
    array('10.0.0.0/8', '10.0.0.1', false),
    array('10.0.0.0/8', '10.0.0.0/8', false),

    // Unusable input is never a match. In particular two unusable values must
    // not compare equal just because inet_pton returns false for both.
    array('', '127.0.0.1', false),
    array('127.0.0.1', '', false),
    array('not-an-ip', '127.0.0.1', false),
    array('127.0.0.1', 'not-an-ip', false),
    array('not-an-ip', 'not-an-ip', false),
);


foreach ($addressCases as $index => $case) {
    list($configured, $peer, $expected) = $case;
    $actual = (new RequestOrigin(array($configured), array('REMOTE_ADDR' => $peer)))->isTrustedProxy();

    if ($actual === $expected) {
        continue;
    }

    $failures++;
    printf(
        "FAIL address case %d: %s vs %s\n  expected: %s\n  actual:   %s\n",
        $index,
        $configured,
        $peer,
        var_export($expected, true),
        var_export($actual, true)
    );
}

/*
 * The X-Forwarded-* header parser. [header value, expected parsed value]
 */
$forwardedCases = array(
    array('https', 'https'),
    array('HTTPS', 'https'),
    array('  https  ', 'https'),
    array('https, http', 'http'),
    array('http, https', 'https'),
    array('gopher, https', 'https'),
    // A trailing empty entry: fall back to the connection scheme.
    array('https,', ''),
    array(', https', 'https'),
    array(',', ''),
    array('   ', ''),
    array('', ''),
);

foreach ($forwardedCases as $index => $case) {
    list($value, $expected) = $case;
    $actual = RequestOrigin::firstForwardedValue($value);

    if ($actual === $expected) {
        continue;
    }

    $failures++;
    printf(
        "FAIL forwarded case %d: %s\n  expected: %s\n  actual:   %s\n",
        $index,
        var_export($value, true),
        var_export($expected, true),
        var_export($actual, true)
    );
}

/*
 * trusted_proxies must also land at the top level of the parsed
 * configuration.ini values, or such a deployment keeps trusting nothing.
 *
 * The template is checked, not the generated global.php, which exits when no
 * directors.ini is installed.
 */
$configTemplate = file_get_contents(__DIR__ . '/../../config/autoload/global.php.in');

$globalChecks = 2;
if (strpos($configTemplate, "'trusted_proxies' => array()") === false) {
    $failures++;
    print "FAIL global.php.in is missing the top level trusted_proxies default\n";
}
if (strpos($configTemplate, "\$configuration['trusted_proxies']['addresses']") === false) {
    $failures++;
    print "FAIL global.php.in does not read trusted_proxies from configuration.ini\n";
}

/*
 * Drive the real factory: the tables above bypass it, so a stale config key
 * would leave a deployment trusting nothing unnoticed.
 */
require_once __DIR__ . '/../../module/Application/src/Application/Listener/SameOriginListenerFactory.php';

/**
 * The factory only ever calls get('config'); everything else is unused.
 */
class SameOriginTestServiceLocator implements Laminas\ServiceManager\ServiceLocatorInterface
{
    private $config;

    public function __construct($config)
    {
        $this->config = $config;
    }

    public function get($id)
    {
        return $this->config[$id];
    }

    public function has($id)
    {
        return isset($this->config[$id]);
    }

    public function build($name, array $options = null)
    {
        return null;
    }

    public function buildFactory($name, $requestedName = null)
    {
        return null;
    }

    public function addFactory($name, $factory, $flags = null)
    {
    }

    public function addInvokable($name, $options = null)
    {
    }

    public function addService($name, $service)
    {
    }

    public function addShared($name, $service)
    {
    }

    public function setService($name, $service)
    {
    }

    public function share($name, $service)
    {
    }

    public function setInvokable($name, $service)
    {
    }

    public function setShared($name, $service)
    {
    }

    public function setAlias($alias, $target)
    {
    }

    public function setFactory($name, $factory)
    {
    }

    public function setConfig($config)
    {
    }

    public function setAllowOverride($allowOverride)
    {
    }

    public function allowOverride($override = false)
    {
    }

    public function getRegisteredServices()
    {
        return array();
    }

    public function getRegisteredInvokables()
    {
        return array();
    }

    public function getRegisteredAliases()
    {
        return array();
    }

    public function getRegisteredFactories()
    {
        return array();
    }
}

$factory = new Application\Listener\SameOriginListenerFactory();

/*
 * Drive the configured value through factory -> listener -> RequestOrigin,
 * behaviourally rather than by inspecting private state.
 * [config, peer address, expected to be trusted]
 */
$factoryCases = array(
    // module.config.php style: a PHP list.
    array(array('trusted_proxies' => array('127.0.0.1', '10.4.5.6')), '127.0.0.1', true),
    array(array('trusted_proxies' => array('127.0.0.1', '10.4.5.6')), '203.0.113.9', false),
    // module.config.php may also hold the comma form; the listener takes it
    // as configured and normalizes it, so no shape has to be special cased.
    array(array('trusted_proxies' => '127.0.0.1, 10.4.5.6'), '10.4.5.6', true),
    array(array('trusted_proxies' => '127.0.0.1, 10.4.5.6'), '203.0.113.9', false),
    // configuration.ini style: one comma separated string.
    array(array('configuration' => array('trusted_proxies' => '127.0.0.1, 10.1.2.3')), '10.1.2.3', true),
    array(array('configuration' => array('trusted_proxies' => '127.0.0.1, 10.1.2.3')), '10.2.2.3', false),
    // A scalar address, as an ini scalar arrives.
    array(array('configuration' => array('trusted_proxies' => '127.0.0.1')), '127.0.0.1', true),
    // configuration.ini replaces the shipped module default rather than
    // merging with it, so the module default address must stop being trusted.
    array(
        array(
            'trusted_proxies' => array('127.0.0.1'),
            'configuration' => array('trusted_proxies' => '192.0.2.10'),
        ),
        '192.0.2.10',
        true,
    ),
    array(
        array(
            'trusted_proxies' => array('127.0.0.1'),
            'configuration' => array('trusted_proxies' => '192.0.2.10'),
        ),
        '127.0.0.1',
        false,
    ),
    // Blank entries must be dropped, never widened to a match-all.
    array(array('trusted_proxies' => array('', '   ', '127.0.0.1')), '127.0.0.1', true),
    array(array('trusted_proxies' => array('', '   ', '127.0.0.1')), '198.51.100.7', false),
    // Nothing configured at all: trust nobody.
    array(array(), '127.0.0.1', false),
    array(array(), '198.51.100.7', false),
    // The retired nested spelling must be ignored, not half honored.
    array(array('same_origin_check' => array('trusted_proxies' => array('198.51.100.7'))), '198.51.100.7', false),
    array(
        array('configuration' => array('same_origin_check' => array('trusted_proxies' => '198.51.100.7'))),
        '198.51.100.7',
        false,
    ),
);

foreach ($factoryCases as $index => $case) {
    list($config, $peer, $expectedTrusted) = $case;

    $listener = $factory->createService(
        new SameOriginTestServiceLocator(array('config' => $config)),
        'Application\Listener\SameOrigin'
    );

    // With X-Forwarded-Proto honored the expected origin is https and matches
    // the Origin header; ignored, it is http and the request is rejected.
    $status = requestThroughListener(
        'POST',
        array(
            'HTTP_HOST' => 'webui.example.com',
            'HTTP_ORIGIN' => ORIGIN,
            'HTTP_X_FORWARDED_PROTO' => 'https',
            'REMOTE_ADDR' => $peer,
        ),
        array(),
        $listener
    );

    $actualTrusted = ($status === 200);

    if ($actualTrusted === $expectedTrusted) {
        continue;
    }

    $failures++;
    printf(
        "FAIL factory case %d: %s peer %s\n  expected trusted: %s\n  actual status:    %d\n",
        $index,
        json_encode($config),
        $peer,
        var_export($expectedTrusted, true),
        $status
    );
}

$globalChecks += count($factoryCases);

/*
 * Resolve the listener the way Application::bootstrap() does. The cases above
 * call the factory directly, skipping the manager's own factory validation.
 */
$serviceManagerChecks = 0;
$serviceManagerCases = 0;

$applicationConfig = require __DIR__ . '/../../module/Application/config/module.config.php';

$serviceManager = new Laminas\ServiceManager\ServiceManager(
    new Laminas\Mvc\Service\ServiceManagerConfig($applicationConfig['service_manager'])
);
$serviceManager->setService(
    'config',
    array_merge($applicationConfig, array('configuration' => array('trusted_proxies' => array())))
);

// This is the call that fails outright if the factory is not usable.
try {
    $resolved = $serviceManager->get($applicationConfig['listeners'][0]);
} catch (Throwable $e) {
    $failures++;
    printf(
        "FAIL resolving %s through the service manager: %s: %s\n",
        $applicationConfig['listeners'][0],
        get_class($e),
        $e->getMessage()
    );
    $resolved = null;
}

$serviceManagerChecks++;
if ($resolved instanceof SameOriginListener) {
    printf("ok   listener resolves through the service manager\n");
} else {
    $failures++;
    print "FAIL listener did not resolve to a SameOriginListener\n";
}

// And it must still work once resolved that way.
if ($resolved instanceof SameOriginListener) {
    $listenerCasesThroughManager = array(
        array('POST', tls(array('HTTP_SEC_FETCH_SITE' => 'same-origin')), 200),
        array('POST', tls(array('HTTP_SEC_FETCH_SITE' => 'cross-site')), 403),
    );

    foreach ($listenerCasesThroughManager as $case) {
        list($method, $server, $expected) = $case;
        $status = requestThroughListener($method, $server, array(), $resolved);

        if ($status === $expected) {
            continue;
        }

        $failures++;
        printf(
            "FAIL resolved listener: %s expected %d, got %d\n",
            $method,
            $expected,
            $status
        );
    }

    $serviceManagerCases += count($listenerCasesThroughManager);
}

// global.php requires RequestOrigin before autoloaders exist, so it must stay
// framework-free.

$requestOriginSource = file_get_contents(
    __DIR__ . '/../../module/Application/src/Application/Http/RequestOrigin.php'
);

$dependencyChecks = 1;
if (preg_match('/^\s*use\s+/m', $requestOriginSource)) {
    $failures++;
    print "FAIL RequestOrigin must not import anything\n";
}
if (preg_match('/\b(Laminas|Zend)\\\\/', $requestOriginSource)) {
    $failures++;
    print "FAIL RequestOrigin must not reference framework classes\n";
}
if (preg_match('/^\s*(abstract\s+|final\s+)?class\s+\w+\s+(extends|implements)\s/m', $requestOriginSource)) {
    $failures++;
    print "FAIL RequestOrigin must not extend or implement anything\n";
}

// The instance API. [trusted proxies as configured, server, expected isHttps]

$instanceCases = array(
    // Server passed explicitly; $_SERVER must not leak in.
    array(array(), array('HTTPS' => 'on'), true),
    array(array(), array('SERVER_PORT' => '443'), true),
    array(array(), array('SERVER_PORT' => '80'), false),
    array(array(), array(), false),
    // A proxy list that arrives as one comma separated string.
    array(
        array('127.0.0.1, 10.4.5.6'),
        array('HTTP_X_FORWARDED_PROTO' => 'https', 'REMOTE_ADDR' => '10.4.5.6'),
        true,
    ),
    array(
        array('127.0.0.1, 10.4.5.6'),
        array('HTTP_X_FORWARDED_PROTO' => 'https', 'REMOTE_ADDR' => '203.0.113.9'),
        false,
    ),
);

$instanceChecks = count($instanceCases);

foreach ($instanceCases as $index => $case) {
    list($proxies, $server, $expected) = $case;
    $actual = (new RequestOrigin($proxies, $server))->isHttps();

    if ($actual === $expected) {
        continue;
    }

    $failures++;
    printf(
        "FAIL instance case %d: %s\n  expected: %s\n  actual:   %s\n",
        $index,
        json_encode($case),
        var_export($expected, true),
        var_export($actual, true)
    );
}

// No server argument falls back to $_SERVER, which PHP cannot express as a
// default value.
$_SERVER['HTTPS'] = 'on';
$_SERVER['HTTP_HOST'] = 'from-superglobal.example.com';
$fromSuperglobal = new RequestOrigin(array());
if (!$fromSuperglobal->isHttps() || $fromSuperglobal->host() !== 'from-superglobal.example.com') {
    $failures++;
    printf(
        "FAIL default server: isHttps=%s host=%s\n",
        var_export($fromSuperglobal->isHttps(), true),
        var_export($fromSuperglobal->host(), true)
    );
}
// An explicit server must win over $_SERVER.
$explicit = new RequestOrigin(array(), array('HTTPS' => 'off', 'HTTP_HOST' => 'explicit.example.com'));
if ($explicit->isHttps() || $explicit->host() !== 'explicit.example.com') {
    $failures++;
    print "FAIL explicit server did not override \$_SERVER\n";
}
$instanceChecks += 1;

// host() and origin() on the same instance must agree with each other.
$one = new RequestOrigin(array(), array('HTTPS' => 'on', 'HTTP_HOST' => 'webui.example.com:8443'));
if ($one->origin() !== 'https://webui.example.com:8443') {
    $failures++;
    printf("FAIL origin composition: %s\n", var_export($one->origin(), true));
}
$instanceChecks += 1;

// The proxy list is taken as configured, which is why global.php needs no cast.
// [configured value, peer, expected]

$looseCases = array(
    // configuration.ini's comma separated string.
    array('127.0.0.1, 10.4.5.6', '127.0.0.1', true),
    array('127.0.0.1, 10.4.5.6', '10.4.5.6', true),
    array('127.0.0.1, 10.4.5.6', '203.0.113.9', false),
    // A single scalar, as an ini scalar arrives.
    array('127.0.0.1', '127.0.0.1', true),
    // module.config.php's array form.
    array(array('127.0.0.1'), '127.0.0.1', true),
    array(array('127.0.0.1'), '203.0.113.9', false),
    // Nothing configured, in each of the shapes it can take.
    array(array(), '127.0.0.1', false),
    array(null, '127.0.0.1', false),
    array('', '127.0.0.1', false),
    array('   ', '127.0.0.1', false),
    // Blank entries within a list are dropped, not treated as match-all.
    array('', '127.0.0.1', false),
    array(array('', '   '), '127.0.0.1', false),
);

$looseChecks = count($looseCases);

foreach ($looseCases as $index => $case) {
    list($configured, $peer, $expected) = $case;
    $actual = (new RequestOrigin($configured, array('REMOTE_ADDR' => $peer)))->isTrustedProxy();

    if ($actual === $expected) {
        continue;
    }

    $failures++;
    printf(
        "FAIL loose case %d: %s peer %s\n  expected: %s\n  actual:   %s\n",
        $index,
        json_encode($configured),
        $peer,
        var_export($expected, true),
        var_export($actual, true)
    );
}

// It must also load on its own, with no autoloader in play.
$requestOriginPath = __DIR__ . '/../../module/Application/src/Application/Http/RequestOrigin.php';
$standalone = shell_exec(sprintf(
    'php -r %s 2>&1',
    escapeshellarg(
        'require ' . var_export($requestOriginPath, true) . ';'
        . 'echo (new Application\Http\RequestOrigin(array(), array("HTTPS" => "on")))->isHttps();'
    )
));
if (trim((string)$standalone) !== '1') {
    $failures++;
    printf("FAIL RequestOrigin did not load standalone: %s\n", trim((string)$standalone));
}

$total = count($cases) + count($proxyCases) + count($listenerCases) + count($wiringChecks)
    + count($addressCases) + count($forwardedCases) + $globalChecks
    + $serviceManagerChecks + $serviceManagerCases + $dependencyChecks
    + $instanceChecks + $looseChecks + count($safeMethodCases);

// Last, so a failure anywhere above fails the run.
if ($failures > 0) {
    printf("%d test(s) failed\n", $failures);
    exit(1);
}

printf("%d cases passed\n", $total);
exit(0);
