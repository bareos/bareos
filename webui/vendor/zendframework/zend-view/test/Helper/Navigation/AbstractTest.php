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
use Zend\Navigation\Navigation;
use Zend\Config\Factory as ConfigFactory;
use Zend\Mvc\Router\RouteMatch as V2RouteMatch;
use Zend\Mvc\Service\ServiceManagerConfig;
use Zend\Navigation\Service\DefaultNavigationFactory;
use Zend\Permissions\Acl\Acl;
use Zend\Permissions\Acl\Role\GenericRole;
use Zend\Permissions\Acl\Resource\GenericResource;
use Zend\Router\ConfigProvider as RouterConfigProvider;
use Zend\Router\RouteMatch as V3RouteMatch;
use Zend\ServiceManager\Config;
use Zend\ServiceManager\ServiceManager;
use Zend\I18n\Translator\Translator;
use Zend\View\Renderer\PhpRenderer;
use ZendTest\View\Helper\TestAsset;

/**
 * Base class for navigation view helper tests
 */
abstract class AbstractTest extends TestCase
{
    /**
     * @var ServiceManager
     */
    protected $serviceManager;

    // @codingStandardsIgnoreStart
    /**
     * Path to files needed for test
     *
     * @var string
     */
    protected $_files;

    /**
     * Class name for view helper to test
     *
     * @var string
     */
    protected $_helperName;

    /**
     * View helper
     *
     * @var \Zend\View\Helper\Navigation\AbstractHelper
     */
    protected $_helper;

    /**
     * The first container in the config file (files/navigation.xml)
     *
     * @var Navigation
     */
    protected $_nav1;

    /**
     * The second container in the config file (files/navigation.xml)
     *
     * @var Navigation\Navigation
     */
    protected $_nav2;

    /**
     * The third container in the config file (files/navigation.xml)
     *
     * @var Navigation\Navigation
     */
    protected $_nav3;

    private $_oldControllerDir;
    // @codingStandardsIgnoreEnd

    /**
     * Prepares the environment before running a test
     *
     */
    protected function setUp()
    {
        $cwd = __DIR__;

        $this->routeMatchType = class_exists(V2RouteMatch::class)
            ? V2RouteMatch::class
            : V3RouteMatch::class;

        // read navigation config
        $this->_files = $cwd . '/_files';
        $config = ConfigFactory::fromFile($this->_files . '/navigation.xml', true);

        // setup containers from config
        $this->_nav1 = new Navigation($config->get('nav_test1'));
        $this->_nav2 = new Navigation($config->get('nav_test2'));
        $this->_nav3 = new Navigation($config->get('nav_test3'));

        // setup view
        $view = new PhpRenderer();
        $view->resolver()->addPath($cwd . '/_files/mvc/views');

        // create helper
        $this->_helper = new $this->_helperName;
        $this->_helper->setView($view);

        // set nav1 in helper as default
        $this->_helper->setContainer($this->_nav1);

        // setup service manager
        $smConfig = [
            'modules'                 => [],
            'module_listener_options' => [
                'config_cache_enabled' => false,
                'cache_dir'            => 'data/cache',
                'module_paths'         => [],
                'extra_config'         => [
                    'service_manager' => [
                        'factories' => [
                            'config' => function () use ($config) {
                                return [
                                    'navigation' => [
                                        'default' => $config->get('nav_test1'),
                                    ],
                                ];
                            }
                        ],
                    ],
                ],
            ],
        ];

        $sm = $this->serviceManager = new ServiceManager();
        $sm->setAllowOverride(true);

        (new ServiceManagerConfig())->configureServiceManager($sm);

        if (! class_exists(V2RouteMatch::class) && class_exists(RouterConfigProvider::class)) {
            $routerConfig = new Config((new RouterConfigProvider())->getDependencyConfig());
            $routerConfig->configureServiceManager($sm);
        }

        $sm->setService('ApplicationConfig', $smConfig);
        $sm->get('ModuleManager')->loadModules();
        $sm->get('Application')->bootstrap();
        $sm->setFactory('Navigation', DefaultNavigationFactory::class);

        $sm->setService('nav1', $this->_nav1);
        $sm->setService('nav2', $this->_nav2);

        $sm->setAllowOverride(false);

        $app = $this->serviceManager->get('Application');
        $app->getMvcEvent()->setRouteMatch(new $this->routeMatchType([
            'controller' => 'post',
            'action'     => 'view',
            'id'         => '1337',
        ]));
    }

    /**
     * Returns the contens of the expected $file
     * @param  string $file
     * @return string
     */
    // @codingStandardsIgnoreStart
    protected function _getExpected($file)
    {
        // @codingStandardsIgnoreEnd
        return file_get_contents($this->_files . '/expected/' . $file);
    }

    /**
     * Sets up ACL
     *
     * @return Acl
     */
    // @codingStandardsIgnoreStart
    protected function _getAcl()
    {
        // @codingStandardsIgnoreEnd
        $acl = new Acl();

        $acl->addRole(new GenericRole('guest'));
        $acl->addRole(new GenericRole('member'), 'guest');
        $acl->addRole(new GenericRole('admin'), 'member');
        $acl->addRole(new GenericRole('special'), 'member');

        $acl->addResource(new GenericResource('guest_foo'));
        $acl->addResource(new GenericResource('member_foo'), 'guest_foo');
        $acl->addResource(new GenericResource('admin_foo', 'member_foo'));
        $acl->addResource(new GenericResource('special_foo'), 'member_foo');

        $acl->allow('guest', 'guest_foo');
        $acl->allow('member', 'member_foo');
        $acl->allow('admin', 'admin_foo');
        $acl->allow('special', 'special_foo');
        $acl->allow('special', 'admin_foo', 'read');

        return ['acl' => $acl, 'role' => 'special'];
    }

    /**
     * Returns translator
     *
     * @return Translator
     */
    // @codingStandardsIgnoreStart
    protected function _getTranslator()
    {
        // @codingStandardsIgnoreEnd
        $loader = new TestAsset\ArrayTranslator();
        $loader->translations = [
            'Page 1'       => 'Side 1',
            'Page 1.1'     => 'Side 1.1',
            'Page 2'       => 'Side 2',
            'Page 2.3'     => 'Side 2.3',
            'Page 2.3.3.1' => 'Side 2.3.3.1',
            'Home'         => 'Hjem',
            'Go home'      => 'Gå hjem'
        ];
        $translator = new Translator();
        $translator->getPluginManager()->setService('default', $loader);
        $translator->addTranslationFile('default', null);
        return $translator;
    }

    /**
     * Returns translator with text domain
     *
     * @return Translator
     */
    // @codingStandardsIgnoreStart
    protected function _getTranslatorWithTextDomain()
    {
        // @codingStandardsIgnoreEnd
        $loader1 = new TestAsset\ArrayTranslator();
        $loader1->translations = [
            'Page 1'       => 'TextDomain1 1',
            'Page 1.1'     => 'TextDomain1 1.1',
            'Page 2'       => 'TextDomain1 2',
            'Page 2.3'     => 'TextDomain1 2.3',
            'Page 2.3.3'   => 'TextDomain1 2.3.3',
            'Page 2.3.3.1' => 'TextDomain1 2.3.3.1',
        ];

        $loader2 = new TestAsset\ArrayTranslator();
        $loader2->translations = [
            'Page 1'       => 'TextDomain2 1',
            'Page 1.1'     => 'TextDomain2 1.1',
            'Page 2'       => 'TextDomain2 2',
            'Page 2.3'     => 'TextDomain2 2.3',
            'Page 2.3.3'   => 'TextDomain2 2.3.3',
            'Page 2.3.3.1' => 'TextDomain2 2.3.3.1',
        ];

        $translator = new Translator();
        $translator->getPluginManager()->setService('default1', $loader1);
        $translator->getPluginManager()->setService('default2', $loader2);
        $translator->addTranslationFile('default1', null, 'ZendTest_1');
        $translator->addTranslationFile('default2', null, 'ZendTest_2');
        return $translator;
    }
}
