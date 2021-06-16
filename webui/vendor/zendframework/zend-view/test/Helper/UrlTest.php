<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Helper;

use PHPUnit\Framework\TestCase;
use Zend\Mvc\MvcEvent;
use Zend\Mvc\ModuleRouteListener;
use Zend\Mvc\Router\Http\Literal as LiteralRoute;
use Zend\Mvc\Router\Http\Segment as SegmentRoute;
use Zend\Mvc\Router\Http\TreeRouteStack;
use Zend\Mvc\Router\Http\Wildcard as WildcardRoute;
use Zend\Mvc\Router\RouteMatch;
use Zend\Mvc\Router\SimpleRouteStack as Router;
use Zend\Router\Http\Literal as NextGenLiteralRoute;
use Zend\Router\Http\Segment as NextGenSegmentRoute;
use Zend\Router\Http\TreeRouteStack as NextGenTreeRouteStack;
use Zend\Router\Http\Wildcard as NextGenWildcardRoute;
use Zend\Router\RouteMatch as NextGenRouteMatch;
use Zend\Router\SimpleRouteStack as NextGenRouter;
use Zend\View\Exception;
use Zend\View\Helper\Url as UrlHelper;

/**
 * Zend\View\Helper\Url Test
 *
 * Tests formText helper, including some common functionality of all form helpers
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class UrlTest extends TestCase
{
    /**
     * @var Router
     */
    private $router;

    /**
     * @var UrlHelper
     */
    private $url;

    /**
     * Sets up the fixture, for example, open a network connection.
     * This method is called before a test is executed.
     */
    protected function setUp()
    {
        $this->routeMatchType = class_exists(RouteMatch::class)
            ? RouteMatch::class
            : NextGenRouteMatch::class;

        $this->literalRouteType = class_exists(LiteralRoute::class)
            ? LiteralRoute::class
            : NextGenLiteralRoute::class;

        $this->segmentRouteType = class_exists(SegmentRoute::class)
            ? SegmentRoute::class
            : NextGenSegmentRoute::class;

        $this->treeRouteStackType = class_exists(TreeRouteStack::class)
            ? TreeRouteStack::class
            : NextGenTreeRouteStack::class;

        $this->wildcardRouteType = class_exists(WildcardRoute::class)
            ? WildcardRoute::class
            : NextGenWildcardRoute::class;

        $this->routerClass = class_exists(Router::class)
            ? Router::class
            : NextGenRouter::class;

        $router = new $this->routerClass();
        $router->addRoute('home', [
            'type' => $this->literalRouteType,
            'options' => [
                'route' => '/',
            ]
        ]);
        $router->addRoute('default', [
                'type' => $this->segmentRouteType,
                'options' => [
                    'route' => '/:controller[/:action]',
                ]
        ]);
        $this->router = $router;

        $this->url = new UrlHelper;
        $this->url->setRouter($router);
    }

    public function testHelperHasHardDependencyWithRouter()
    {
        $this->expectException(Exception\RuntimeException::class);
        $this->expectExceptionMessage('No RouteStackInterface instance provided');
        $url = new UrlHelper;
        $url('home');
    }

    public function testHomeRoute()
    {
        $url = $this->url->__invoke('home');
        $this->assertEquals('/', $url);
    }

    public function testModuleRoute()
    {
        $url = $this->url->__invoke('default', ['controller' => 'ctrl', 'action' => 'act']);
        $this->assertEquals('/ctrl/act', $url);
    }

    public function testModel()
    {
        $it = new \ArrayIterator(['controller' => 'ctrl', 'action' => 'act']);

        $url = $this->url->__invoke('default', $it);
        $this->assertEquals('/ctrl/act', $url);
    }

    /**
     * @expectedException \Zend\View\Exception\InvalidArgumentException
     */
    public function testThrowsExceptionOnInvalidParams()
    {
        $this->url->__invoke('default', 'invalid params');
    }

    public function testPluginWithoutRouteMatchesInEventRaisesExceptionWhenNoRouteProvided()
    {
        $this->expectException(Exception\RuntimeException::class);
        $this->expectExceptionMessage('RouteMatch');
        $this->url->__invoke();
    }

    public function testPluginWithRouteMatchesReturningNoMatchedRouteNameRaisesExceptionWhenNoRouteProvided()
    {
        $this->url->setRouteMatch(new $this->routeMatchType([]));
        $this->expectException(Exception\RuntimeException::class);
        $this->expectExceptionMessage('matched');
        $this->url->__invoke();
    }

    public function testPassingNoArgumentsWithValidRouteMatchGeneratesUrl()
    {
        $routeMatch = new $this->routeMatchType([]);
        $routeMatch->setMatchedRouteName('home');
        $this->url->setRouteMatch($routeMatch);
        $url = $this->url->__invoke();
        $this->assertEquals('/', $url);
    }

    public function testCanReuseMatchedParameters()
    {
        $this->router->addRoute('replace', [
            'type'    => $this->segmentRouteType,
            'options' => [
                'route'    => '/:controller/:action',
                'defaults' => [
                    'controller' => 'ZendTest\Mvc\Controller\TestAsset\SampleController',
                ],
            ],
        ]);
        $routeMatch = new $this->routeMatchType([
            'controller' => 'foo',
        ]);
        $routeMatch->setMatchedRouteName('replace');
        $this->url->setRouteMatch($routeMatch);
        $url = $this->url->__invoke('replace', ['action' => 'bar'], [], true);
        $this->assertEquals('/foo/bar', $url);
    }

    public function testCanPassBooleanValueForThirdArgumentToAllowReusingRouteMatches()
    {
        $this->router->addRoute('replace', [
            'type' => $this->segmentRouteType,
            'options' => [
                'route'    => '/:controller/:action',
                'defaults' => [
                    'controller' => 'ZendTest\Mvc\Controller\TestAsset\SampleController',
                ],
            ],
        ]);
        $routeMatch = new $this->routeMatchType([
            'controller' => 'foo',
        ]);
        $routeMatch->setMatchedRouteName('replace');
        $this->url->setRouteMatch($routeMatch);
        $url = $this->url->__invoke('replace', ['action' => 'bar'], true);
        $this->assertEquals('/foo/bar', $url);
    }

    public function testRemovesModuleRouteListenerParamsWhenReusingMatchedParameters()
    {
        $router = new $this->treeRouteStackType;
        $router->addRoute('default', [
            'type' => $this->segmentRouteType,
            'options' => [
                'route'    => '/:controller/:action',
                'defaults' => [
                    ModuleRouteListener::MODULE_NAMESPACE => 'ZendTest\Mvc\Controller\TestAsset',
                    'controller' => 'SampleController',
                    'action'     => 'Dash'
                ]
            ],
            'child_routes' => [
                'wildcard' => [
                    'type'    => $this->wildcardRouteType,
                    'options' => [
                        'param_delimiter'     => '=',
                        'key_value_delimiter' => '%'
                    ]
                ]
            ]
        ]);

        $routeMatch = new $this->routeMatchType([
            ModuleRouteListener::MODULE_NAMESPACE => 'ZendTest\Mvc\Controller\TestAsset',
            'controller' => 'Rainbow'
        ]);
        $routeMatch->setMatchedRouteName('default/wildcard');

        $event = new MvcEvent();
        $event->setRouter($router)
              ->setRouteMatch($routeMatch);

        $moduleRouteListener = new ModuleRouteListener();
        $moduleRouteListener->onRoute($event);

        $helper = new UrlHelper();
        $helper->setRouter($router);
        $helper->setRouteMatch($routeMatch);

        $url = $helper->__invoke('default/wildcard', ['Twenty' => 'Cooler'], true);
        $this->assertEquals('/Rainbow/Dash=Twenty%Cooler', $url);
    }

    public function testAcceptsNextGenRouterToSetRouter()
    {
        $router = new $this->routerClass();
        $url = new UrlHelper();
        $url->setRouter($router);
        $this->assertAttributeSame($router, 'router', $url);
    }

    public function testAcceptsNextGenRouteMatche()
    {
        $routeMatch = new $this->routeMatchType([]);
        $url = new UrlHelper();
        $url->setRouteMatch($routeMatch);
        $this->assertAttributeSame($routeMatch, 'routeMatch', $url);
    }
}
