<?php
/**
 * @see       https://github.com/zendframework/zend-inputfilter for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-inputfilter/blob/master/LICENSE.md New BSD License
 */

namespace Zend\InputFilter;

class Module
{
    /**
     * Return default zend-inputfilter configuration for zend-mvc applications.
     */
    public function getConfig()
    {
        $provider = new ConfigProvider();

        return [
            'service_manager' => $provider->getDependencyConfig(),
            'input_filters'   => $provider->getInputFilterConfig(),
        ];
    }

    /**
     * Register a specification for the InputFilterManager with the ServiceListener.
     *
     * @param \Zend\ModuleManager\ModuleManager $moduleManager
     * @return void
     */
    public function init($moduleManager)
    {
        $event = $moduleManager->getEvent();
        $container = $event->getParam('ServiceManager');
        $serviceListener = $container->get('ServiceListener');

        $serviceListener->addServiceManager(
            'InputFilterManager',
            'input_filters',
            'Zend\ModuleManager\Feature\InputFilterProviderInterface',
            'getInputFilterConfig'
        );
    }
}
