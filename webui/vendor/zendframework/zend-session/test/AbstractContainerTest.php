<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Session;

use PHPUnit\Framework\TestCase;
use Zend\Session\Container;
use Zend\Session\Config\StandardConfig;
use Zend\Session\ManagerInterface as Manager;
use ZendTest\Session\TestAsset\TestContainer;

/**
 * @group      Zend_Session
 * @covers Zend\Session\AbstractContainer
 */
class AbstractContainerTest extends TestCase
{
    /**
     * Hack to allow running tests in separate processes
     *
     * @see    http://matthewturland.com/2010/08/19/process-isolation-in-phpunit/
     */
    protected $preserveGlobalState = false;

    /**
     * @var Manager
     */
    protected $manager;

    /**
     * @var Container
     */
    protected $container;

    public function setUp()
    {
        $_SESSION = [];
        Container::setDefaultManager(null);

        $config = new StandardConfig([
            'storage' => 'Zend\\Session\\Storage\\ArrayStorage',
        ]);

        $this->manager = $manager = new TestAsset\TestManager($config);
        $this->container = new TestContainer('Default', $manager);
    }

    public function tearDown()
    {
        $_SESSION = [];
        Container::setDefaultManager(null);
    }

    /**
     * This test case fails on zend-session 2.8.0 with the php error below and works fine on 2.7.*.
     * "Only variable references should be returned by reference"
     */
    public function testOffsetGetMissingKey()
    {
        self::assertNull($this->container->offsetGet('this key does not exist in the container'));
    }
}
