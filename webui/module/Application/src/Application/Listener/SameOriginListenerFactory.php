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

namespace Application\Listener;

use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;

/**
 * Builds SameOriginListener from the trusted_proxies configuration.
 */
class SameOriginListenerFactory implements FactoryInterface
{
    /**
     * @param ServiceLocatorInterface $serviceLocator
     * @param string $requestedName
     * @return SameOriginListener
     */
    public function createService(ServiceLocatorInterface $serviceLocator, $requestedName = null)
    {
        $config = $serviceLocator->get('config');
        $trustedProxies = array();

        // The shipped module default, which is empty.
        if (isset($config['trusted_proxies'])) {
            $trustedProxies = $config['trusted_proxies'];
        }

        // configuration.ini overrides it, so a proxied deployment needs no edit
        // to the shipped module config.
        if (isset($config['configuration']['trusted_proxies'])) {
            $trustedProxies = $config['configuration']['trusted_proxies'];
        }

        // Passed on as configured; the listener normalizes it.
        return new SameOriginListener($trustedProxies);
    }
}
