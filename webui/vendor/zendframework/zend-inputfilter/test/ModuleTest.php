<?php
/**
 * @see       https://github.com/zendframework/zend-inputfilter for the canonical source repository
 * @copyright Copyright (c) 2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-inputfilter/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\InputFilter;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\InputFilter\InputFilterAbstractServiceFactory;
use Zend\InputFilter\InputFilterPluginManager;
use Zend\InputFilter\Module;

class ModuleTest extends TestCase
{
    /** @var Module */
    private $module;

    protected function setUp()
    {
        $this->module = new Module();
    }

    public function testGetConfigMethodShouldReturnExpectedKeys()
    {
        $config = $this->module->getConfig();

        $this->assertInternalType('array', $config);

        // Service manager
        $this->assertArrayHasKey('service_manager', $config);

        // Input filters
        $this->assertArrayHasKey('input_filters', $config);
    }

    public function testServiceManagerConfigShouldContainInputFilterManager()
    {
        $config = $this->module->getConfig();

        $this->assertArrayHasKey(
            InputFilterPluginManager::class,
            $config['service_manager']['factories']
        );
    }

    public function testServiceManagerConfigShouldContainAliasForInputFilterManager()
    {
        $config = $this->module->getConfig();

        $this->assertArrayHasKey(
            'InputFilterManager',
            $config['service_manager']['aliases']
        );
    }

    public function testInputFilterConfigShouldContainAbstractServiceFactory()
    {
        $config = $this->module->getConfig();

        $this->assertContains(
            InputFilterAbstractServiceFactory::class,
            $config['input_filters']['abstract_factories']
        );
    }

    public function testInitMethodShouldRegisterPluginManagerSpecificationWithServiceListener()
    {
        // Service listener
        $serviceListener = $this->prophesize(TestAsset\ServiceListenerInterface::class);
        $serviceListener->addServiceManager(
            'InputFilterManager',
            'input_filters',
            'Zend\ModuleManager\Feature\InputFilterProviderInterface',
            'getInputFilterConfig'
        )->shouldBeCalled();

        // Container
        $container = $this->prophesize(ContainerInterface::class);
        $container->get('ServiceListener')->will([$serviceListener, 'reveal']);

        // Event
        $event = $this->prophesize(TestAsset\ModuleEventInterface::class);
        $event->getParam('ServiceManager')->will([$container, 'reveal']);

        // Module manager
        $moduleManager = $this->prophesize(TestAsset\ModuleManagerInterface::class);
        $moduleManager->getEvent()->will([$event, 'reveal']);

        $this->assertNull($this->module->init($moduleManager->reveal()));
    }
}
