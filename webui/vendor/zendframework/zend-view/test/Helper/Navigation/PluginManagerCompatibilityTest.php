<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Helper\Navigation;

use PHPUnit\Framework\TestCase;
use Zend\ServiceManager\Config;
use Zend\ServiceManager\ServiceManager;
use Zend\ServiceManager\Test\CommonPluginManagerTrait;
use Zend\View\Exception\InvalidHelperException;
use Zend\View\Helper\Navigation\AbstractHelper;
use Zend\View\Helper\Navigation\Breadcrumbs;
use Zend\View\Helper\Navigation\PluginManager;

/**
 * @group      Zend_View
 */
class PluginManagerCompatibilityTest extends TestCase
{
    use CommonPluginManagerTrait;

    protected function getPluginManager()
    {
        return new PluginManager(new ServiceManager());
    }

    protected function getV2InvalidPluginException()
    {
        return InvalidHelperException::class;
    }

    protected function getInstanceOf()
    {
        return AbstractHelper::class;
    }

    /**
     * @group 43
     */
    public function testConstructorArgumentsAreOptionalUnderV2()
    {
        $helpers = $this->getPluginManager();
        if (method_exists($helpers, 'configure')) {
            $this->markTestSkipped('zend-servicemanager v3 plugin managers require a container argument');
        }

        $helpers = new PluginManager();
        $this->assertInstanceOf(PluginManager::class, $helpers);
    }

    /**
     * @group 43
     */
    public function testConstructorAllowsConfigInstanceAsFirstArgumentUnderV2()
    {
        $helpers = $this->getPluginManager();
        if (method_exists($helpers, 'configure')) {
            $this->markTestSkipped('zend-servicemanager v3 plugin managers require a container argument');
        }

        $helpers = new PluginManager(new Config([]));
        $this->assertInstanceOf(PluginManager::class, $helpers);
    }

    public function testInjectsParentContainerIntoHelpers()
    {
        $config = new Config([
            'navigation' => [
                'default' => [],
            ],
        ]);

        $services = new ServiceManager();
        $config->configureServiceManager($services);
        $helpers = new PluginManager($services);

        $helper = $helpers->get('breadcrumbs');
        $this->assertInstanceOf(Breadcrumbs::class, $helper);
        $this->assertSame($services, $helper->getServiceLocator());
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
