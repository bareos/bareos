<?php
/**
 * @link      http://github.com/zendframework/zend-navigation for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Navigation\View;

use PHPUnit\Framework\TestCase;
use Zend\Navigation\View\ViewHelperManagerDelegatorFactory;
use Zend\ServiceManager\ServiceManager;
use Zend\View\HelperPluginManager;
use Zend\View\Helper\Navigation as NavigationHelper;

class ViewHelperManagerDelegatorFactoryTest extends TestCase
{
    public function testFactoryConfiguresViewHelperManagerWithNavigationHelpers()
    {
        $services = new ServiceManager();
        $helpers = new HelperPluginManager($services);
        $callback = function () use ($helpers) {
            return $helpers;
        };

        $factory = new ViewHelperManagerDelegatorFactory();
        $this->assertSame($helpers, $factory($services, 'ViewHelperManager', $callback));

        $this->assertTrue($helpers->has('navigation'));
        $this->assertTrue($helpers->has(NavigationHelper::class));
    }
}
