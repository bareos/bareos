<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zend-view for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View;

use PHPUnit\Framework\TestCase;
use ReflectionProperty;
use Zend\Mvc\Controller\Plugin\FlashMessenger as V2FlashMessenger;
use Zend\Mvc\Controller\PluginManager as ControllerPluginManager;
use Zend\Mvc\Plugin\FlashMessenger\FlashMessenger;
use Zend\ServiceManager\Config;
use Zend\ServiceManager\ServiceManager;
use Zend\ServiceManager\Test\CommonPluginManagerTrait;
use Zend\View\Exception\InvalidHelperException;
use Zend\View\HelperPluginManager;

class HelperPluginManagerCompatibilityTest extends TestCase
{
    use CommonPluginManagerTrait;

    protected function getPluginManager()
    {
        $factories = [];

        if (class_exists(ControllerPluginManager::class)) {
            $factories['ControllerPluginManager'] = function ($services, $name, $options) {
                return new ControllerPluginManager($services, [
                    'invokables' => [
                        'flashmessenger' => class_exists(FlashMessenger::class)
                            ? FlashMessenger::class
                            : V2FlashMessenger::class,
                    ],
                ]);
            };
        }

        $config = new Config([
            'services' => [
                'config' => [],
            ],
            'factories' => $factories,
        ]);
        $manager = new ServiceManager();
        $config->configureServiceManager($manager);
        $helperManager = new HelperPluginManager($manager);

        return $helperManager;
    }

    protected function getV2InvalidPluginException()
    {
        return InvalidHelperException::class;
    }

    public function aliasProvider()
    {
        $pluginManager = $this->getPluginManager();
        $r = new ReflectionProperty($pluginManager, 'aliases');
        $r->setAccessible(true);
        $aliases = $r->getValue($pluginManager);

        foreach ($aliases as $alias => $target) {
            // Skipping conditionally since it depends on zend-mvc
            if (! class_exists(ControllerPluginManager::class) && strpos($target, '\\FlashMessenger')) {
                continue;
            }

            // Skipping conditionally since it depends on zend-mvc
            if (! class_exists(ControllerPluginManager::class) && strpos($target, '\\Url')) {
                continue;
            }

            yield $alias => [$alias, $target];
        }
    }

    public function getInstanceOf()
    {
        // no-op; instanceof is not used in this implementation
    }

    public function testInstanceOfMatches()
    {
        $this->markTestSkipped('instanceOf is not used with this implementation');
    }

    /**
     * @todo remove this test once we set the minimum zend-servicemanager version to 3
     */
    public function testRegisteringInvalidElementRaisesException()
    {
        $this->expectException($this->getServiceNotFoundException());
        $this->getPluginManager()->setService('test', $this);
    }

    /**
     * @todo remove this test once we set the minimum zend-servicemanager version to 3
     */
    public function testLoadingInvalidElementRaisesException()
    {
        $manager = $this->getPluginManager();
        $manager->setInvokableClass('test', get_class($this));
        $this->expectException($this->getServiceNotFoundException());
        $manager->get('test');
    }
}
