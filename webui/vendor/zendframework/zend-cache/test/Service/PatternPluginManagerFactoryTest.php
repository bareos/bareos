<?php
/**
 * @link      http://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Service;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\Cache\Pattern\PatternInterface;
use Zend\Cache\PatternPluginManager;
use Zend\Cache\Service\PatternPluginManagerFactory;
use Zend\ServiceManager\ServiceLocatorInterface;

class PatternPluginManagerFactoryTest extends TestCase
{
    public function testFactoryReturnsPluginManager()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $factory = new PatternPluginManagerFactory();

        $patterns = $factory($container, PatternPluginManager::class);
        $this->assertInstanceOf(PatternPluginManager::class, $patterns);

        if (method_exists($patterns, 'configure')) {
            // zend-servicemanager v3
            $this->assertAttributeSame($container, 'creationContext', $patterns);
        } else {
            // zend-servicemanager v2
            $this->assertSame($container, $patterns->getServiceLocator());
        }
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderContainerInterop()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $pattern = $this->prophesize(PatternInterface::class)->reveal();

        $factory = new PatternPluginManagerFactory();
        $patterns = $factory($container, PatternPluginManager::class, [
            'services' => [
                'test' => $pattern,
            ],
        ]);
        $this->assertSame($pattern, $patterns->get('test'));
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderServiceManagerV2()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $pattern = $this->prophesize(PatternInterface::class)->reveal();

        $factory = new PatternPluginManagerFactory();
        $factory->setCreationOptions([
            'services' => [
                'test' => $pattern,
            ],
        ]);

        $patterns = $factory->createService($container->reveal());
        $this->assertSame($pattern, $patterns->get('test'));
    }
}
