<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Mvc\Service;

class HydratorManagerFactory extends AbstractPluginManagerFactory
{
    /**
     * @todo Switch to Zend\Hydrator\HydratorPluginManager for 3.0 (if kept)
     */
    const PLUGIN_MANAGER_CLASS = 'Zend\Stdlib\Hydrator\HydratorPluginManager';
}
