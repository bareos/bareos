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
use Zend\Cache\Exception;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\ZendServerShm<extended>
 */
class ZendServerShmTest extends CommonAdapterTest
{
    public function setUp()
    {
        if (getenv('TESTS_ZEND_CACHE_ZEND_SERVER_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_ZEND_SERVER_ENABLED to run this test');
        }

        if (strtolower(PHP_SAPI) == 'cli') {
            $this->markTestSkipped('Zend Server SHM does not work in CLI environment');
            return;
        }

        if (! function_exists('zend_shm_cache_store')) {
            try {
                new Cache\Storage\Adapter\ZendServerShm();
                $this->fail("Missing expected ExtensionNotLoadedException");
            } catch (Exception\ExtensionNotLoadedException $e) {
                $this->markTestSkipped($e->getMessage());
            }
        }

        $this->_options = new Cache\Storage\Adapter\AdapterOptions();
        $this->_storage = new Cache\Storage\Adapter\ZendServerShm($this->_options);
        parent::setUp();
    }

    public function tearDown()
    {
        if (function_exists('zend_shm_cache_clear')) {
            zend_shm_cache_clear();
        }

        parent::tearDown();
    }

    public function getCommonAdapterNamesProvider()
    {
        return [
            ['zend_server_shm'],
            ['zendservershm'],
            ['ZendServerShm'],
            ['ZendServerSHM'],
            ['zendServerShm'],
            ['zendServerSHM'],
        ];
    }
}
