<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use Zend\Cache;
use Zend\Session\Container as SessionContainer;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\Session<extended>
 */
class SessionTest extends CommonAdapterTest
{
    public function setUp()
    {
        $_SESSION = [];
        SessionContainer::setDefaultManager(null);
        $sessionContainer = new SessionContainer('Default');

        $this->_options = new Cache\Storage\Adapter\SessionOptions([
            'session_container' => $sessionContainer
        ]);
        $this->_storage = new Cache\Storage\Adapter\Session();
        $this->_storage->setOptions($this->_options);

        parent::setUp();
    }

    public function tearDown()
    {
        if (! class_exists(SessionContainer::class)) {
            return;
        }

        $_SESSION = [];
        SessionContainer::setDefaultManager(null);
    }

    public function getCommonAdapterNamesProvider()
    {
        return [
            ['session'],
            ['Session'],
        ];
    }
}
