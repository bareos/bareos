<?php
/**
 * @see       https://github.com/zendframework/zend-view for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-view/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\View\Helper\Service;

use PHPUnit\Framework\TestCase;
use Zend\Authentication\AuthenticationService;
use Zend\Authentication\AuthenticationServiceInterface;
use Zend\ServiceManager\ServiceManager;
use Zend\View\Helper\Identity;
use Zend\View\HelperPluginManager;
use Zend\View\Helper\Service\IdentityFactory;

class IdentityFactoryTest extends TestCase
{
    public function setUp()
    {
        $this->services = $this->prophesize(ServiceManager::class);
        $this->helpers  = new HelperPluginManager($this->services->reveal());
    }

    public function getContainerForFactory()
    {
        if (method_exists($this->helpers, 'configure')) {
            return $this->services->reveal();
        }
        return $this->helpers;
    }

    public function testFactoryReturnsEmptyIdentityIfNoAuthenticationServicePresent()
    {
        $this->services->has(AuthenticationService::class)->willReturn(false);
        $this->services->get(AuthenticationService::class)->shouldNotBeCalled();
        $this->services->has(AuthenticationServiceInterface::class)->willReturn(false);
        $this->services->get(AuthenticationServiceInterface::class)->shouldNotBeCalled();

        $factory = new IdentityFactory();

        $plugin = $factory($this->getContainerForFactory(), Identity::class);

        $this->assertInstanceOf(Identity::class, $plugin);
        $this->assertNull($plugin->getAuthenticationService());
    }

    public function testFactoryReturnsIdentityWithConfiguredAuthenticationServiceWhenPresent()
    {
        $authentication = $this->prophesize(AuthenticationService::class);

        $this->services->has(AuthenticationService::class)->willReturn(true);
        $this->services->get(AuthenticationService::class)->will([$authentication, 'reveal']);
        $this->services->has(AuthenticationServiceInterface::class)->willReturn(false);
        $this->services->get(AuthenticationServiceInterface::class)->shouldNotBeCalled();

        $factory = new IdentityFactory();

        $plugin = $factory($this->getContainerForFactory(), Identity::class);

        $this->assertInstanceOf(Identity::class, $plugin);
        $this->assertSame($authentication->reveal(), $plugin->getAuthenticationService());
    }

    public function testFactoryReturnsIdentityWithConfiguredAuthenticationServiceInterfaceWhenPresent()
    {
        $authentication = $this->prophesize(AuthenticationServiceInterface::class);

        $this->services->has(AuthenticationService::class)->willReturn(false);
        $this->services->get(AuthenticationService::class)->shouldNotBeCalled();
        $this->services->has(AuthenticationServiceInterface::class)->willReturn(true);
        $this->services->get(AuthenticationServiceInterface::class)->will([$authentication, 'reveal']);

        $factory = new IdentityFactory();

        $plugin = $factory($this->getContainerForFactory(), Identity::class);

        $this->assertInstanceOf(Identity::class, $plugin);
        $this->assertSame($authentication->reveal(), $plugin->getAuthenticationService());
    }
}
