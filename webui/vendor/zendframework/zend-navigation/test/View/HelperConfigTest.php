<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Navigation\View;

use PHPUnit\Framework\TestCase;
use Zend\Navigation\Service\DefaultNavigationFactory;
use Zend\Navigation\View\HelperConfig;
use Zend\ServiceManager\Config;
use Zend\ServiceManager\ServiceManager;
use Zend\View\HelperPluginManager;
use Zend\View\Helper\Navigation as NavigationHelper;

/**
 * Tests the class Zend_Navigation_Page_Mvc
 *
 * @group      Zend_Navigation
 */
class HelperConfigTest extends TestCase
{
    public function navigationServiceNameProvider()
    {
        return [
            ['navigation'],
            ['Navigation'],
            [NavigationHelper::class],
            ['zendviewhelpernavigation'],
        ];
    }

    /**
     * @dataProvider navigationServiceNameProvider
     */
    public function testConfigureServiceManagerWithConfig($navigationHelperServiceName)
    {
        $replacedMenuClass = NavigationHelper\Links::class;

        $serviceManager = new ServiceManager();
        (new Config([
            'services' => [
                'config' => [
                    'navigation_helpers' => [
                        'invokables' => [
                            'menu' => $replacedMenuClass,
                        ],
                    ],
                    'navigation' => [
                        'file'    => __DIR__ . '/../_files/navigation.xml',
                        'default' => [
                            [
                                'label' => 'Page 1',
                                'uri'   => 'page1.html',
                            ],
                            [
                                'label' => 'MVC Page',
                                'route' => 'foo',
                                'pages' => [
                                    [
                                        'label' => 'Sub MVC Page',
                                        'route' => 'foo',
                                    ],
                                ],
                            ],
                            [
                                'label' => 'Page 3',
                                'uri'   => 'page3.html',
                            ],
                        ],
                    ],
                ],
            ],
            'factories' => [
                'Navigation' => DefaultNavigationFactory::class,
                'ViewHelperManager' => function ($services) {
                    return new HelperPluginManager($services);
                },
            ],
        ]))->configureServiceManager($serviceManager);

        $helpers = $serviceManager->get('ViewHelperManager');
        (new HelperConfig())->configureServiceManager($helpers);

        $menu = $helpers->get($navigationHelperServiceName)->findHelper('menu');
        $this->assertInstanceOf($replacedMenuClass, $menu);
    }
}
