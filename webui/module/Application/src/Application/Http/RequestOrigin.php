<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2026-2026 Bareos GmbH & Co. KG (http://www.bareos.org/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

namespace Application\Http;

/**
 * The scheme, host and origin a request arrived with, honoring reverse proxies.
 *
 * Shared by Application\Module (the cookie's Secure flag) and
 * SameOriginListener (the Origin check), so the two cannot disagree.
 *
 * One instance describes one request; the proxy list is read from the config
 * and normalized in the constructor. X-Forwarded-* is believed only from a
 * configured proxy (REMOTE_ADDR), since a forwarded header can be set by anyone.
 *
 * Deliberately framework-free, so it can be required on its own.
 */
class RequestOrigin
{
    /** [a-z0-9.-]+ or a bracketed [0-9a-f:.] run, then an optional :digits. */
    const HOST_PATTERN = '/^(?:[a-z0-9.-]+|\[[0-9a-f:.]+\])(?::[0-9]+)?$/';

    /**
     * @var array
     */
    private $server;

    /**
     * @var array
     */
    private $trustedProxies;

    /**
     * @param array $config
     *   Merged configuration; trusted_proxies is read from it. This list is the
     *   whole trust boundary.
     * @param array|null $server
     *   Defaults to $_SERVER; null because PHP forbids a superglobal default.
     */
    public function __construct(array $config = array(), ?array $server = null)
    {
        $this->trustedProxies = self::normalize(self::trustedProxiesFromConfig($config));
        $this->server = $server === null ? $_SERVER : $server;
    }

    /**
     * The origin as a browser serializes it.
     *
     * @return string|null null when the host is missing or malformed
     */
    public function origin()
    {
        $host = $this->host();
        if ($host === null) {
            return null;
        }

        return ($this->isHttps() ? 'https' : 'http') . '://' . $host;
    }

    /**
     * The addressed host; X-Forwarded-Host wins for a trusted peer.
     *
     * @return string|null null when no usable host is present
     */
    public function host()
    {
        if ($this->isTrustedProxy() && !empty($this->server['HTTP_X_FORWARDED_HOST'])) {
            $host = self::firstForwardedValue($this->server['HTTP_X_FORWARDED_HOST']);
        } else {
            $host = $this->server['HTTP_HOST'] ?? null;
        }

        $host = strtolower(trim($host ?? ''));

        if ($host === '' || !preg_match(self::HOST_PATTERN, $host)) {
            return null;
        }

        return $host;
    }

    /**
     * Whether the browser reached this request over https.
     *
     * @return bool
     */
    public function isHttps()
    {
        if ($this->isTrustedProxy() && !empty($this->server['HTTP_X_FORWARDED_PROTO'])) {
            $proto = self::firstForwardedValue($this->server['HTTP_X_FORWARDED_PROTO']);
            if ($proto === 'http' || $proto === 'https') {
                return $proto === 'https';
            }
        }

        if (isset($this->server['HTTPS']) && strtolower($this->server['HTTPS']) !== 'off') {
            return true;
        }

        // HTTPS is optional in CGI (RFC 3875 4.1.18); SERVER_PORT is not
        // (4.1.15), so the port is the only guaranteed scheme signal.
        if (isset($this->server['SERVER_PORT']) && (int)$this->server['SERVER_PORT'] === 443) {
            return true;
        }

        return false;
    }

    /**
     * Whether the immediate peer is a configured proxy. REMOTE_ADDR, not a
     * forwarded header, which any client can set.
     *
     * @return bool
     */
    public function isTrustedProxy()
    {
        if (empty($this->trustedProxies)) {
            return false;
        }

        $packed = @inet_pton(trim($this->server['REMOTE_ADDR'] ?? ''));
        if ($packed === false) {
            return false;
        }

        foreach ($this->trustedProxies as $trusted) {
            if (@inet_pton($trusted) === $packed) {
                return true;
            }
        }

        return false;
    }

    /**
     * Flatten a configured proxy list into usable entries.
     *
     * Takes the value as the configuration held it (array or comma separated
     * string), so no caller needs an (array) cast. Unusable entries are
     * dropped; an empty pattern must not become a wildcard.
     *
     * @param array|string|null $addresses
     * @return array
     */
    public static function normalize($addresses)
    {
        $normalized = array();

        foreach ((array)$addresses as $address) {
            if (!is_string($address) && !is_numeric($address)) {
                continue;
            }

            foreach (explode(',', (string)$address) as $part) {
                $part = trim($part);
                if ($part === '') {
                    continue;
                }

                $normalized[] = $part;
            }
        }

        return $normalized;
    }

    /**
     * trusted_proxies from the merged config: module.config.php, overridden by
     * configuration.ini.
     *
     * @param array $config
     * @return array|string|null
     */
    private static function trustedProxiesFromConfig(array $config)
    {
        if (isset($config['configuration']['trusted_proxies'])) {
            return $config['configuration']['trusted_proxies'];
        }

        return isset($config['trusted_proxies']) ? $config['trusted_proxies'] : array();
    }

    /**
     * The rightmost entry of an X-Forwarded-* header: a chain appends, so that
     * is the one written by the proxy in front of the webui.
     *
     * @param string $value
     * @return string the entry, lower-cased, or '' when there is none
     */
    public static function firstForwardedValue(string $value)
    {
        $parts = explode(',', $value);
        return strtolower(trim(end($parts)));
    }
}
