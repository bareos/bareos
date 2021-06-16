<?php
/**
 * @link      http://github.com/zendframework/zend-navigation for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Navigation;

use PHPUnit\Framework\TestCase;
use Zend\Navigation\ConfigProvider;
use Zend\Navigation\Navigation;
use Zend\Navigation\Service;
use Zend\Navigation\View;

class ConfigProviderTest extends TestCase
{
    private $config = [
        'abstract_factories' => [
            Service\NavigationAbstractServiceFactory::class,
        ],
        'aliases' => [
            'navigation' => Navigation::class,
        ],
        'delegators' => [
            'ViewHelperManager' => [
                View\ViewHelperManagerDelegatorFactory::class,
            ],
        ],
        'factories' => [
            Navigation::class => Service\DefaultNavigationFactory::class,
        ],
    ];

    public function testProvidesExpectedConfiguration()
    {
        $provider = new ConfigProvider();
        $this->assertEquals($this->config, $provider->getDependencyConfig());
        return $provider;
    }

    /**
     * @depends testProvidesExpectedConfiguration
     */
    public function testInvocationProvidesDependencyConfiguration(ConfigProvider $provider)
    {
        $this->assertEquals(['dependencies' => $provider->getDependencyConfig()], $provider());
    }
}
