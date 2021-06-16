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

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\Apc<extended>
 */
class ApcTest extends CommonAdapterTest
{
    /**
     * Restore 'apc.use_request_time'
     *
     * @var mixed
     */
    protected $iniUseRequestTime;

    public function setUp()
    {
        if (getenv('TESTS_ZEND_CACHE_APC_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_APC_ENABLED to run this test');
        }

        try {
            new Cache\Storage\Adapter\Apc();
        } catch (Cache\Exception\ExtensionNotLoadedException $e) {
            $this->markTestSkipped($e->getMessage());
        }

        // needed on test expirations
        $this->iniUseRequestTime = ini_get('apc.use_request_time');
        ini_set('apc.use_request_time', 0);

        $this->_options = new Cache\Storage\Adapter\ApcOptions();
        $this->_storage = new Cache\Storage\Adapter\Apc();
        $this->_storage->setOptions($this->_options);

        parent::setUp();
    }

    public function tearDown()
    {
        if (function_exists('apc_clear_cache')) {
            apc_clear_cache('user');
        }

        // reset ini configurations
        ini_set('apc.use_request_time', $this->iniUseRequestTime);

        parent::tearDown();
    }

    public function getCommonAdapterNamesProvider()
    {
        return [
            ['apc'],
            ['Apc'],
        ];
    }
}
