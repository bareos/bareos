<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Helper\Navigation;

use Interop\Container\ContainerInterface;
use Zend\Navigation\Navigation as Container;
use Zend\Navigation\Page;
use Zend\Permissions\Acl;
use Zend\Permissions\Acl\Role;
use Zend\ServiceManager\ServiceManager;
use Zend\View;
use Zend\View\Helper\Navigation;
use Zend\View\Renderer\PhpRenderer;
use Zend\I18n\Translator\Translator;

/**
 * Tests Zend\View\Helper\Navigation
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class NavigationTest extends AbstractTest
{
    // @codingStandardsIgnoreStart
    /**
     * Class name for view helper to test
     *
     * @var string
     */
    protected $_helperName = Navigation::class;

    /**
     * View helper
     *
     * @var Navigation
     */
    protected $_helper;
    // @codingStandardsIgnoreEnd

    public function testHelperEntryPointWithoutAnyParams()
    {
        $returned = $this->_helper->__invoke();
        $this->assertEquals($this->_helper, $returned);
        $this->assertEquals($this->_nav1, $returned->getContainer());
    }

    public function testHelperEntryPointWithContainerParam()
    {
        $returned = $this->_helper->__invoke($this->_nav2);
        $this->assertEquals($this->_helper, $returned);
        $this->assertEquals($this->_nav2, $returned->getContainer());
    }

    public function testAcceptAclShouldReturnGracefullyWithUnknownResource()
    {
        // setup
        $acl = $this->_getAcl();
        $this->_helper->setAcl($acl['acl']);
        $this->_helper->setRole($acl['role']);

        $accepted = $this->_helper->accept(
            new Page\Uri([
                'resource'  => 'unknownresource',
                'privilege' => 'someprivilege'
            ], false)
        );

        $this->assertEquals($accepted, false);
    }

    public function testShouldProxyToMenuHelperByDefault()
    {
        $this->_helper->setContainer($this->_nav1);
        $this->_helper->setServiceLocator(new ServiceManager());

        // result
        $expected = $this->_getExpected('menu/default1.html');
        $actual = $this->_helper->render();

        $this->assertEquals($expected, $actual);
    }

    public function testHasContainer()
    {
        $oldContainer = $this->_helper->getContainer();
        $this->_helper->setContainer(null);
        $this->assertFalse($this->_helper->hasContainer());
        $this->_helper->setContainer($oldContainer);
    }

    public function testInjectingContainer()
    {
        // setup
        $this->_helper->setContainer($this->_nav2);
        $this->_helper->setServiceLocator(new ServiceManager());
        $expected = [
            'menu' => $this->_getExpected('menu/default2.html'),
            'breadcrumbs' => $this->_getExpected('bc/default.html')
        ];
        $actual = [];

        // result
        $actual['menu'] = $this->_helper->render();
        $this->_helper->setContainer($this->_nav1);
        $actual['breadcrumbs'] = $this->_helper->breadcrumbs()->render();

        $this->assertEquals($expected, $actual);
    }

    public function testDisablingContainerInjection()
    {
        // setup
        $this->_helper->setServiceLocator(new ServiceManager());
        $this->_helper->setInjectContainer(false);
        $this->_helper->menu()->setContainer(null);
        $this->_helper->breadcrumbs()->setContainer(null);
        $this->_helper->setContainer($this->_nav2);

        // result
        $expected = [
            'menu'        => '',
            'breadcrumbs' => ''
        ];
        $actual = [
            'menu'        => $this->_helper->render(),
            'breadcrumbs' => $this->_helper->breadcrumbs()->render()
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testMultipleNavigationsAndOneMenuDisplayedTwoTimes()
    {
        $this->_helper->setServiceLocator(new ServiceManager());
        $expected = $this->_helper->setContainer($this->_nav1)->menu()->getContainer();
        $this->_helper->setContainer($this->_nav2)->menu()->getContainer();
        $actual = $this->_helper->setContainer($this->_nav1)->menu()->getContainer();

        $this->assertEquals($expected, $actual);
    }

    public function testServiceManagerIsUsedToRetrieveContainer()
    {
        $container      = new Container;
        $serviceManager = new ServiceManager;
        $serviceManager->setService('navigation', $container);

        $pluginManager  = new View\HelperPluginManager($serviceManager);

        $this->_helper->setServiceLocator($serviceManager);
        $this->_helper->setContainer('navigation');

        $expected = $this->_helper->getContainer();
        $actual   = $container;
        $this->assertEquals($expected, $actual);
    }

    public function testInjectingAcl()
    {
        // setup
        $acl = $this->_getAcl();
        $this->_helper->setAcl($acl['acl']);
        $this->_helper->setRole($acl['role']);
        $this->_helper->setServiceLocator(new ServiceManager());

        $expected = $this->_getExpected('menu/acl.html');
        $actual = $this->_helper->render();

        $this->assertEquals($expected, $actual);
    }

    public function testDisablingAclInjection()
    {
        // setup
        $acl = $this->_getAcl();
        $this->_helper->setAcl($acl['acl']);
        $this->_helper->setRole($acl['role']);
        $this->_helper->setInjectAcl(false);
        $this->_helper->setServiceLocator(new ServiceManager());

        $expected = $this->_getExpected('menu/default1.html');
        $actual = $this->_helper->render();

        $this->assertEquals($expected, $actual);
    }

    public function testInjectingTranslator()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $this->_helper->setTranslator($this->_getTranslator());
        $this->_helper->setServiceLocator(new ServiceManager());

        $expected = $this->_getExpected('menu/translated.html');
        $actual = $this->_helper->render();

        $this->assertEquals($expected, $actual);
    }

    public function testDisablingTranslatorInjection()
    {
        $this->_helper->setTranslator($this->_getTranslator());
        $this->_helper->setInjectTranslator(false);
        $this->_helper->setServiceLocator(new ServiceManager());

        $expected = $this->_getExpected('menu/default1.html');
        $actual = $this->_helper->render();

        $this->assertEquals($expected, $actual);
    }

    public function testTranslatorMethods()
    {
        $translatorMock = $this->prophesize(Translator::class)->reveal();
        $this->_helper->setTranslator($translatorMock, 'foo');

        $this->assertEquals($translatorMock, $this->_helper->getTranslator());
        $this->assertEquals('foo', $this->_helper->getTranslatorTextDomain());
        $this->assertTrue($this->_helper->hasTranslator());
        $this->assertTrue($this->_helper->isTranslatorEnabled());

        $this->_helper->setTranslatorEnabled(false);
        $this->assertFalse($this->_helper->isTranslatorEnabled());
    }

    public function testSpecifyingDefaultProxy()
    {
        $expected = [
            'breadcrumbs' => $this->_getExpected('bc/default.html'),
            'menu' => $this->_getExpected('menu/default1.html')
        ];
        $actual = [];

        // result
        $this->_helper->setServiceLocator(new ServiceManager());
        $this->_helper->setDefaultProxy('breadcrumbs');
        $actual['breadcrumbs'] = $this->_helper->render($this->_nav1);
        $this->_helper->setDefaultProxy('menu');
        $actual['menu'] = $this->_helper->render($this->_nav1);

        $this->assertEquals($expected, $actual);
    }

    public function testGetAclReturnsNullIfNoAclInstance()
    {
        $this->assertNull($this->_helper->getAcl());
    }

    public function testGetAclReturnsAclInstanceSetWithSetAcl()
    {
        $acl = new Acl\Acl();
        $this->_helper->setAcl($acl);
        $this->assertEquals($acl, $this->_helper->getAcl());
    }

    public function testGetAclReturnsAclInstanceSetWithSetDefaultAcl()
    {
        $acl = new Acl\Acl();
        Navigation\AbstractHelper::setDefaultAcl($acl);
        $actual = $this->_helper->getAcl();
        Navigation\AbstractHelper::setDefaultAcl(null);
        $this->assertEquals($acl, $actual);
    }

    public function testSetDefaultAclAcceptsNull()
    {
        $acl = new Acl\Acl();
        Navigation\AbstractHelper::setDefaultAcl($acl);
        Navigation\AbstractHelper::setDefaultAcl(null);
        $this->assertNull($this->_helper->getAcl());
    }

    public function testSetDefaultAclAcceptsNoParam()
    {
        $acl = new Acl\Acl();
        Navigation\AbstractHelper::setDefaultAcl($acl);
        Navigation\AbstractHelper::setDefaultAcl();
        $this->assertNull($this->_helper->getAcl());
    }

    public function testSetRoleAcceptsString()
    {
        $this->_helper->setRole('member');
        $this->assertEquals('member', $this->_helper->getRole());
    }

    public function testSetRoleAcceptsRoleInterface()
    {
        $role = new Role\GenericRole('member');
        $this->_helper->setRole($role);
        $this->assertEquals($role, $this->_helper->getRole());
    }

    public function testSetRoleAcceptsNull()
    {
        $this->_helper->setRole('member')->setRole(null);
        $this->assertNull($this->_helper->getRole());
    }

    public function testSetRoleAcceptsNoParam()
    {
        $this->_helper->setRole('member')->setRole();
        $this->assertNull($this->_helper->getRole());
    }

    public function testSetRoleThrowsExceptionWhenGivenAnInt()
    {
        try {
            $this->_helper->setRole(1337);
            $this->fail('An invalid argument was given, but a ' .
                        'Zend\View\Exception\InvalidArgumentException was not thrown');
        } catch (View\Exception\ExceptionInterface $e) {
            $this->assertContains('$role must be a string', $e->getMessage());
        }
    }

    public function testSetRoleThrowsExceptionWhenGivenAnArbitraryObject()
    {
        try {
            $this->_helper->setRole(new \stdClass());
            $this->fail('An invalid argument was given, but a ' .
                        'Zend\View\Exception\InvalidArgumentException was not thrown');
        } catch (View\Exception\ExceptionInterface $e) {
            $this->assertContains('$role must be a string', $e->getMessage());
        }
    }

    public function testSetDefaultRoleAcceptsString()
    {
        $expected = 'member';
        Navigation\AbstractHelper::setDefaultRole($expected);
        $actual = $this->_helper->getRole();
        Navigation\AbstractHelper::setDefaultRole(null);
        $this->assertEquals($expected, $actual);
    }

    public function testSetDefaultRoleAcceptsRoleInterface()
    {
        $expected = new Role\GenericRole('member');
        Navigation\AbstractHelper::setDefaultRole($expected);
        $actual = $this->_helper->getRole();
        Navigation\AbstractHelper::setDefaultRole(null);
        $this->assertEquals($expected, $actual);
    }

    public function testSetDefaultRoleAcceptsNull()
    {
        Navigation\AbstractHelper::setDefaultRole(null);
        $this->assertNull($this->_helper->getRole());
    }

    public function testSetDefaultRoleAcceptsNoParam()
    {
        Navigation\AbstractHelper::setDefaultRole();
        $this->assertNull($this->_helper->getRole());
    }

    public function testSetDefaultRoleThrowsExceptionWhenGivenAnInt()
    {
        try {
            Navigation\AbstractHelper::setDefaultRole(1337);
            $this->fail('An invalid argument was given, but a ' .
                        'Zend\View\Exception\InvalidArgumentException was not thrown');
        } catch (View\Exception\ExceptionInterface $e) {
            $this->assertContains('$role must be', $e->getMessage());
        }
    }

    public function testSetDefaultRoleThrowsExceptionWhenGivenAnArbitraryObject()
    {
        try {
            Navigation\AbstractHelper::setDefaultRole(new \stdClass());
            $this->fail('An invalid argument was given, but a ' .
                        'Zend\View\Exception\InvalidArgumentException was not thrown');
        } catch (View\Exception\ExceptionInterface $e) {
            $this->assertContains('$role must be', $e->getMessage());
        }
    }
    // @codingStandardsIgnoreStart
    private $_errorMessage;
    // @codingStandardsIgnoreEnd
    public function toStringErrorHandler($code, $msg, $file, $line, array $c)
    {
        $this->_errorMessage = $msg;
    }

    public function testMagicToStringShouldNotThrowException()
    {
        set_error_handler([$this, 'toStringErrorHandler']);
        $this->_helper->menu()->setPartial([1337]);
        $this->_helper->__toString();
        restore_error_handler();

        $this->assertContains('array must contain', $this->_errorMessage);
    }

    public function testPageIdShouldBeNormalized()
    {
        $nl = PHP_EOL;

        $container = new \Zend\Navigation\Navigation([
            [
                'label' => 'Page 1',
                'id'    => 'p1',
                'uri'   => 'p1'
            ],
            [
                'label' => 'Page 2',
                'id'    => 'p2',
                'uri'   => 'p2'
            ]
        ]);

        $expected = '<ul class="navigation">' . $nl
                  . '    <li>' . $nl
                  . '        <a id="menu-p1" href="p1">Page 1</a>' . $nl
                  . '    </li>' . $nl
                  . '    <li>' . $nl
                  . '        <a id="menu-p2" href="p2">Page 2</a>' . $nl
                  . '    </li>' . $nl
                  . '</ul>';

        $this->_helper->setServiceLocator(new ServiceManager());
        $actual = $this->_helper->render($container);

        $this->assertEquals($expected, $actual);
    }

    /**
     * @group ZF-6854
     */
    public function testRenderInvisibleItem()
    {
        $container = new \Zend\Navigation\Navigation([
            [
                'label' => 'Page 1',
                'id'    => 'p1',
                'uri'   => 'p1'
            ],
            [
                'label'   => 'Page 2',
                'id'      => 'p2',
                'uri'     => 'p2',
                'visible' => false
            ]
        ]);

        $this->_helper->setServiceLocator(new ServiceManager());
        $render = $this->_helper->menu()->render($container);

        $this->assertNotContains('p2', $render);

        $this->_helper->menu()->setRenderInvisible();

        $render = $this->_helper->menu()->render($container);

        $this->assertContains('p2', $render);
    }

    public function testMultipleNavigations()
    {
        $sm   = new ServiceManager();
        $nav1 = new Container();
        $nav2 = new Container();
        $sm->setService('nav1', $nav1);
        $sm->setService('nav2', $nav2);

        $helper = new Navigation();
        $helper->setServiceLocator($sm);

        $menu     = $helper('nav1')->menu();
        $actual   = spl_object_hash($nav1);
        $expected = spl_object_hash($menu->getContainer());
        $this->assertEquals($expected, $actual);

        $menu     = $helper('nav2')->menu();
        $actual   = spl_object_hash($nav2);
        $expected = spl_object_hash($menu->getContainer());
        $this->assertEquals($expected, $actual);
    }

    /**
     * @group #3859
     */
    public function testMultipleNavigationsWithDifferentHelpersAndDifferentContainers()
    {
        $sm   = new ServiceManager();
        $nav1 = new Container();
        $nav2 = new Container();
        $sm->setService('nav1', $nav1);
        $sm->setService('nav2', $nav2);

        $helper = new Navigation();
        $helper->setServiceLocator($sm);

        $menu     = $helper('nav1')->menu();
        $actual   = spl_object_hash($nav1);
        $expected = spl_object_hash($menu->getContainer());
        $this->assertEquals($expected, $actual);

        $breadcrumbs = $helper('nav2')->breadcrumbs();
        $actual      = spl_object_hash($nav2);
        $expected    = spl_object_hash($breadcrumbs->getContainer());
        $this->assertEquals($expected, $actual);

        $links    = $helper()->links();
        $expected = spl_object_hash($links->getContainer());
        $this->assertEquals($expected, $actual);
    }

    /**
     * @group #3859
     */
    public function testMultipleNavigationsWithDifferentHelpersAndSameContainer()
    {
        $sm   = new ServiceManager();
        $nav1 = new Container();
        $sm->setService('nav1', $nav1);

        $helper = new Navigation();
        $helper->setServiceLocator($sm);

        // Tests
        $menu     = $helper('nav1')->menu();
        $actual   = spl_object_hash($nav1);
        $expected = spl_object_hash($menu->getContainer());
        $this->assertEquals($expected, $actual);

        $breadcrumbs = $helper('nav1')->breadcrumbs();
        $expected    = spl_object_hash($breadcrumbs->getContainer());
        $this->assertEquals($expected, $actual);

        $links    = $helper()->links();
        $expected = spl_object_hash($links->getContainer());
        $this->assertEquals($expected, $actual);
    }

    /**
     * @group #3859
     */
    public function testMultipleNavigationsWithSameHelperAndSameContainer()
    {
        $sm   = new ServiceManager();
        $nav1 = new Container();
        $sm->setService('nav1', $nav1);

        $helper = new Navigation();
        $helper->setServiceLocator($sm);

        // Test
        $menu     = $helper('nav1')->menu();
        $actual   = spl_object_hash($nav1);
        $expected = spl_object_hash($menu->getContainer());
        $this->assertEquals($expected, $actual);

        $menu     = $helper('nav1')->menu();
        $expected = spl_object_hash($menu->getContainer());
        $this->assertEquals($expected, $actual);

        $menu    = $helper()->menu();
        $expected = spl_object_hash($menu->getContainer());
        $this->assertEquals($expected, $actual);
    }

    public function testSetPluginManagerAndView()
    {
        $pluginManager = new Navigation\PluginManager(new ServiceManager());
        $view = new PhpRenderer();

        $helper = new $this->_helperName;
        $helper->setPluginManager($pluginManager);
        $helper->setView($view);

        $this->assertEquals($view, $pluginManager->getRenderer());
    }

    /**
     * @group 49
     */
    public function testInjectsLazyInstantiatedPluginManagerWithCurrentServiceLocator()
    {
        $services = $this->prophesize(ContainerInterface::class)->reveal();
        $helper = new $this->_helperName;
        $helper->setServiceLocator($services);

        $plugins = $helper->getPluginManager();
        $this->assertInstanceOf(Navigation\PluginManager::class, $plugins);

        if (method_exists($plugins, 'configure')) {
            // v3
            $this->assertAttributeSame($services, 'creationContext', $plugins);
        } else {
            // v2
            $this->assertSame($services, $plugins->getServiceLocator());
        }
    }

    /**
     * Returns the contens of the expected $file, normalizes newlines
     * @param  string $file
     * @return string
     */
    // @codingStandardsIgnoreStart
    protected function _getExpected($file)
    {
        // @codingStandardsIgnoreEnd
        return str_replace("\n", PHP_EOL, parent::_getExpected($file));
    }
}
