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
 * @covers Zend\Cache\Storage\Adapter\WinCache<extended>
 */
class WinCacheTest extends CommonAdapterTest
{
    public function setUp()
    {
        if (getenv('TESTS_ZEND_CACHE_WINCACHE_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_WINCACHE_ENABLED to run this test');
        }

        if (! extension_loaded('wincache')) {
            $this->markTestSkipped("WinCache extension is not loaded");
        }

        $enabled = ini_get('wincache.ucenabled');
        if (PHP_SAPI == 'cli') {
            $enabled = $enabled && (bool) ini_get('wincache.enablecli');
        }

        if (! $enabled) {
            throw new Exception\ExtensionNotLoadedException(
                "WinCache is disabled - see 'wincache.ucenabled' and 'wincache.enablecli'"
            );
        }

        $this->_options = new Cache\Storage\Adapter\WinCacheOptions();
        $this->_storage = new Cache\Storage\Adapter\WinCache();
        $this->_storage->setOptions($this->_options);

        parent::setUp();
    }

    public function tearDown()
    {
        if (function_exists('wincache_ucache_clear')) {
            wincache_ucache_clear();
        }

        parent::tearDown();
    }

    public function getCommonAdapterNamesProvider()
    {
        return [
            ['win_cache'],
            ['wincache'],
            ['WinCache'],
            ['winCache'],
        ];
    }
}
