<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\I18n\Module;

class ModuleTest extends TestCase
{
    protected function setUp()
    {
        $this->module = new Module();
    }

    public function testConfigReturnsExpectedKeys()
    {
        $config = $this->module->getConfig();
        $this->assertInternalType('array', $config);
        $this->assertArrayHasKey('filters', $config);
        $this->assertArrayHasKey('service_manager', $config);
        $this->assertArrayHasKey('validators', $config);
        $this->assertArrayHasKey('view_helpers', $config);
    }

    public function testInitRegistersPluginManagerSpecificationWithServiceListener()
    {
        $serviceListener = $this->prophesize(TestAsset\ServiceListenerInterface::class);
        $serviceListener->addServiceManager(
            'TranslatorPluginManager',
            'translator_plugins',
            'Zend\ModuleManager\Feature\TranslatorPluginProviderInterface',
            'getTranslatorPluginConfig'
        )->shouldBeCalled();

        $container = $this->prophesize(ContainerInterface::class);
        $container->get('ServiceListener')->willReturn($serviceListener->reveal());

        $event = $this->prophesize(TestAsset\ModuleEventInterface::class);
        $event->getParam('ServiceManager')->willReturn($container->reveal());

        $moduleManager = $this->prophesize(TestAsset\ModuleManagerInterface::class);
        $moduleManager->getEvent()->willReturn($event->reveal());

        $this->assertNull($this->module->init($moduleManager->reveal()));
    }
}
