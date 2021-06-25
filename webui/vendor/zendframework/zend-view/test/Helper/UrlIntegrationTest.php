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
use Zend\Console\Console;
use Zend\Console\Request;
use Zend\Http\Request as HttpRequest;
use Zend\ServiceManager\ServiceManager;
use Zend\Mvc\Console\ConfigProvider as MvcConsoleConfigProvider;
use Zend\Mvc\Router\Http as V2HttpRoute;
use Zend\Mvc\Service\ServiceManagerConfig;
use Zend\Mvc\Service\ServiceListenerFactory;
use Zend\Router\ConfigProvider as RouterConfigProvider;
use Zend\Router\Http as V3HttpRoute;
use Zend\ServiceManager\Config;

/**
 * url() helper test -- tests integration with MVC
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class UrlIntegrationTest extends TestCase
{
    protected function setUp()
    {
        $this->literalRouteType = class_exists(V2HttpRoute\Literal::class)
            ? V2HttpRoute\Literal::class
            : V3HttpRoute\Literal::class;
        $config = [
            'router' => [
                'routes' => [
                    'test' => [
                        'type' => $this->literalRouteType,
                        'options' => [
                            'route' => '/test',
                            'defaults' => [
                                'controller' => 'Test\Controller\Test',
                            ],
                        ],
                    ],
                ],
            ],
            'console' => [
                'router' => [
                    'routes' => [
                        'test' => [
                            'type' => 'Simple',
                            'options' => [
                                'route' => 'test this',
                                'defaults' => [
                                    'controller' => 'Test\Controller\TestConsole',
                                ],
                            ],
                        ],
                    ],
                ],
            ],
        ];

        $serviceConfig = $this->readAttribute(new ServiceListenerFactory, 'defaultServiceConfig');

        $this->serviceManager = new ServiceManager();
        (new ServiceManagerConfig($serviceConfig))->configureServiceManager($this->serviceManager);

        if (! class_exists(V2HttpRoute\Literal::class) && class_exists(RouterConfigProvider::class)) {
            $routerConfig = new Config((new RouterConfigProvider())->getDependencyConfig());
            $routerConfig->configureServiceManager($this->serviceManager);
        }

        if (class_exists(MvcConsoleConfigProvider::class)) {
            $mvcConsoleConfig = new Config((new MvcConsoleConfigProvider())->getDependencyConfig());
            $mvcConsoleConfig->configureServiceManager($this->serviceManager);
        }

        $this->serviceManager->setAllowOverride(true);
        $this->serviceManager->setService('config', $config);
        $this->serviceManager->setAlias('Configure', 'config');
        $this->serviceManager->setAllowOverride(false);
    }

    public function testUrlHelperWorksUnderNormalHttpParadigms()
    {
        Console::overrideIsConsole(false);
        $this->serviceManager->get('Application')->bootstrap();
        $request = $this->serviceManager->get('Request');
        $this->assertInstanceOf(HttpRequest::class, $request);
        $viewHelpers = $this->serviceManager->get('ViewHelperManager');
        $urlHelper   = $viewHelpers->get('url');
        $test        = $urlHelper('test');
        $this->assertEquals('/test', $test);
    }

    public function testUrlHelperWorksWithForceCanonicalFlag()
    {
        Console::overrideIsConsole(false);
        $this->serviceManager->get('Application')->bootstrap();
        $request = $this->serviceManager->get('Request');
        $this->assertInstanceOf(HttpRequest::class, $request);
        $router = $this->serviceManager->get('Router');
        $router->setRequestUri($request->getUri());
        $request->setUri('http://example.com/test');
        $viewHelpers = $this->serviceManager->get('ViewHelperManager');
        $urlHelper   = $viewHelpers->get('url');
        $test        = $urlHelper('test', [], ['force_canonical' => true]);
        $this->assertContains('/test', $test);
    }

    public function testUrlHelperUnderConsoleParadigmShouldReturnHttpRoutes()
    {
        Console::overrideIsConsole(true);
        $this->serviceManager->get('Application')->bootstrap();
        $request = $this->serviceManager->get('Request');
        $this->assertInstanceOf(Request::class, $request);
        $viewHelpers = $this->serviceManager->get('ViewHelperManager');
        $urlHelper   = $viewHelpers->get('url');
        $test        = $urlHelper('test');
        $this->assertEquals('/test', $test);
    }
}
