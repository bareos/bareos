<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Service;

class HydratorManagerFactory extends AbstractPluginManagerFactory
{
    /**
     * @todo Switch to Laminas\Hydrator\HydratorPluginManager for 3.0 (if kept)
     */
    const PLUGIN_MANAGER_CLASS = 'Laminas\Stdlib\Hydrator\HydratorPluginManager';
}
