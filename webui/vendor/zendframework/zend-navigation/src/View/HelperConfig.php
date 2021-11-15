<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Navigation\View;

use Zend\ServiceManager\ConfigInterface;
use Zend\ServiceManager\ServiceManager;
use Zend\View\HelperPluginManager;

/**
 * Service manager configuration for navigation view helpers
 */
class HelperConfig implements ConfigInterface
{
    /**
     * Configure the provided service manager instance with the configuration
     * in this class.
     *
     * Simply adds a factory for the navigation helper, and has it inject the helper
     * with the service locator instance.
     *
     * @param  ServiceManager $serviceManager
     * @return void
     */
    public function configureServiceManager(ServiceManager $serviceManager)
    {
        $serviceManager->setFactory('navigation', function (HelperPluginManager $pm) {
            $helper = new \Zend\View\Helper\Navigation;
            $helper->setServiceLocator($pm->getServiceLocator());

            $config = $pm->getServiceLocator()->get('config');
            if (isset($config['navigation_helpers'])) {
                $config = new \Zend\ServiceManager\Config($config['navigation_helpers']);
                $config->configureServiceManager($helper->getPluginManager());
            }
            return $helper;
        });
    }
}
