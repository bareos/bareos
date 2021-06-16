<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n\Translator;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\I18n\Translator\LoaderPluginManager;
use Zend\I18n\Translator\TranslatorServiceFactory;
use Zend\I18n\Translator\Translator;

class TranslatorServiceFactoryTest extends TestCase
{
    public function testCreateServiceWithNoTranslatorKeyDefined()
    {
        $pluginManagerMock = $this->prophesize(LoaderPluginManager::class)->reveal();

        $serviceLocator = $this->prophesize(ContainerInterface::class);
        $serviceLocator->has('TranslatorPluginManager')->willReturn(true)->shouldBeCalledTimes(1);
        $serviceLocator->get('TranslatorPluginManager')->willReturn($pluginManagerMock)->shouldBeCalledTimes(1);
        $serviceLocator->get('config')->willReturn([])->shouldBeCalledTimes(1);

        $factory = new TranslatorServiceFactory();
        $translator = $factory($serviceLocator->reveal(), Translator::class);
        $this->assertInstanceOf(Translator::class, $translator);
        $this->assertSame($pluginManagerMock, $translator->getPluginManager());
    }

    public function testCreateServiceWithNoTranslatorPluginManagerDefined()
    {
        $serviceLocator = $this->prophesize(ContainerInterface::class);
        $serviceLocator->has('TranslatorPluginManager')->willReturn(false)->shouldBeCalledTimes(1);
        $serviceLocator->get('config')->willReturn([])->shouldBeCalledTimes(1);

        $factory = new TranslatorServiceFactory();
        $translator = $factory($serviceLocator->reveal(), Translator::class);
        $this->assertInstanceOf(Translator::class, $translator);
    }
}
