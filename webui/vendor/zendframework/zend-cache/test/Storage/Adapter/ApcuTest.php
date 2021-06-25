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
 * @covers Zend\Cache\Storage\Adapter\Apcu<extended>
 */
class ApcuTest extends CommonAdapterTest
{
    /**
     * Restore 'apc.use_request_time'
     *
     * @var mixed
     */
    protected $iniUseRequestTime;

    public function setUp()
    {
        if (! getenv('TESTS_ZEND_CACHE_APCU_ENABLED')) {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_APCU_ENABLED to run this test');
        }

        $enabled = extension_loaded('apcu') && version_compare(phpversion('apcu'), '5.1.0', '>=');
        $enabled = $enabled && ini_get('apc.enabled') && (PHP_SAPI !== 'cli' || ini_get('apc.enable_cli'));

        try {
            $apcu = new Cache\Storage\Adapter\Apcu();
            if (! $enabled) {
                $this->fail('Missing expected ExtensionNotLoadedException');
            }

            // needed on test expirations
            $this->iniUseRequestTime = ini_get('apc.use_request_time');
            ini_set('apc.use_request_time', 0);

            $this->_options = new Cache\Storage\Adapter\ApcuOptions();
            $this->_storage = $apcu;
            $this->_storage->setOptions($this->_options);
        } catch (Cache\Exception\ExtensionNotLoadedException $e) {
            if ($enabled) {
                $this->fail('ext/apcu enabled but an ExtensionNotLoadedException was thrown: ' . $e->getMessage());
            } else {
                $this->markTestSkipped($e->getMessage());
            }
        }

        parent::setUp();
    }

    public function tearDown()
    {
        if (function_exists('apcu_clear_cache')) {
            apcu_clear_cache();
        }

        // reset ini configurations
        ini_set('apc.use_request_time', $this->iniUseRequestTime);

        parent::tearDown();
    }

    public function getCommonAdapterNamesProvider()
    {
        return [
            ['apcu'],
            ['Apcu'],
            ['ApcU'],
            ['APCu'],
        ];
    }
}
