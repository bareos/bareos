<?php
/**
 * @link      https://github.com/zendframework/zend-inputfilter for the canonical source repository
 * @copyright Copyright (c) 2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-inputfilter/blob/master/LICENSE.md
 */

namespace ZendTest\InputFilter\TestAsset;

/**
 * Stub interfact to mock when testing Module::init.
 *
 * Mimics method that will be called on ServiceListener.
 */
interface ServiceListenerInterface
{
    /**
     * @param string $pluginManagerService
     * @param string $configKey
     * @param string $interface
     * @param string $method
     */
    public function addServiceManager(
        $pluginManagerService,
        $configKey,
        $interface,
        $method
    );
}
