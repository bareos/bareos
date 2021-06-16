<?php
/**
 * @link      http://github.com/zendframework/zend-navigation for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Navigation\Service;

use PHPUnit\Framework\TestCase;
use ReflectionMethod;
use Zend\Mvc\Application;
use Zend\Mvc\MvcEvent;
use Zend\Mvc\Router as MvcRouter;
use Zend\Navigation\Exception;
use Zend\Navigation\Navigation;
use Zend\Navigation\Service\AbstractNavigationFactory;
use Zend\Router;
use Zend\ServiceManager\Config;
use Zend\ServiceManager\ServiceManager;

/**
 * @todo Write tests covering full functionality. Tests were introduced to
 *     resolve zendframework/zend-navigation#37, and cover one specific
 *     method to ensure argument validation works correctly.
 */
class AbstractNavigationFactoryTest extends TestCase
{
    public function setUp()
    {
        $this->factory = new TestAsset\TestNavigationFactory();
    }

    public function testCanInjectComponentsUsingZendRouterClasses()
    {
        $routeMatch = $this->prophesize(Router\RouteMatch::class)->reveal();
        $router = $this->prophesize(Router\RouteStackInterface::class)->reveal();
        $args = [[], $routeMatch, $router];

        $r = new ReflectionMethod($this->factory, 'injectComponents');
        $r->setAccessible(true);
        try {
            $pages = $r->invokeArgs($this->factory, $args);
        } catch (Exception\InvalidArgumentException $e) {
            $message = sprintf(
                'injectComponents should not raise exception for zend-router classes; received %s',
                $e->getMessage()
            );
            $this->fail($message);
        }

        $this->assertSame([], $pages);
    }

    public function testCanInjectComponentsUsingZendMvcRouterClasses()
    {
        if (! class_exists(MvcRouter\RouteMatch::class)) {
            $this->markTestSkipped('Test does not run for zend-mvc v3 releases');
        }

        $routeMatch = $this->prophesize(MvcRouter\RouteMatch::class)->reveal();
        $router = $this->prophesize(MvcRouter\RouteStackInterface::class)->reveal();
        $args = [[], $routeMatch, $router];

        $r = new ReflectionMethod($this->factory, 'injectComponents');
        $r->setAccessible(true);
        try {
            $pages = $r->invokeArgs($this->factory, $args);
        } catch (Exception\InvalidArgumentException $e) {
            $message = sprintf(
                'injectComponents should not raise exception for zend-mvc router classes; received %s',
                $e->getMessage()
            );
            $this->fail($message);
        }

        $this->assertSame([], $pages);
    }

    public function testCanCreateNavigationInstanceV2()
    {
        $routerMatchClass = $this->getRouteMatchClass();
        $routerClass = $this->getRouterClass();
        $routeMatch = new $routerMatchClass([]);
        $router = new $routerClass();

        $mvcEventStub = new MvcEvent();
        $mvcEventStub->setRouteMatch($routeMatch);
        $mvcEventStub->setRouter($router);

        $applicationMock = $this->getMockBuilder(Application::class)
            ->disableOriginalConstructor()
            ->getMock();

        $applicationMock->expects($this->any())
            ->method('getMvcEvent')
            ->willReturn($mvcEventStub);

        $serviceManagerMock = $this->getMockBuilder(ServiceManager::class)
            ->disableOriginalConstructor()
            ->getMock();

        $serviceManagerMock->expects($this->any())
            ->method('get')
            ->willReturnMap([
                ['config', ['navigation' => ['testStubNavigation' => []]]],
                ['Application', $applicationMock]
            ]);

        $navigationFactory
            = $this->getMockForAbstractClass(AbstractNavigationFactory::class);
        $navigationFactory->expects($this->any())
            ->method('getName')
            ->willReturn('testStubNavigation');
        $navigation = $navigationFactory->createService($serviceManagerMock);

        $this->assertInstanceOf(Navigation::class, $navigation);
    }

    public function getRouterClass()
    {
        return class_exists(MvcRouter\Http\TreeRouteStack::class)
            ? MvcRouter\Http\TreeRouteStack::class
            : Router\Http\TreeRouteStack::class;
    }

    public function getRouteMatchClass()
    {
        return class_exists(MvcRouter\RouteMatch::class)
            ? MvcRouter\RouteMatch::class
            : Router\RouteMatch::class;
    }
}
