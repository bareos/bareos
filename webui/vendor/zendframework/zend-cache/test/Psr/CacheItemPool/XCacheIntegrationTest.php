<?php
/**
 * @see       https://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-cache/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Cache\Psr\CacheItemPool;

use PHPUnit\Framework\TestCase;
use Zend\Cache\Psr\CacheItemPool\CacheItemPoolDecorator;
use Zend\Cache\Storage\Plugin\Serializer;
use Zend\Cache\StorageFactory;
use Zend\Cache\Exception;
use Zend\ServiceManager\Exception\ServiceNotCreatedException;

/**
 * @requires extension xcache
 */
class XCacheIntegrationTest extends TestCase
{

    /**
     * XCache is using request time based TTL handling which violates PSR-6
     *
     * @expectedException \Zend\Cache\Psr\CacheItemPool\CacheException
     */
    public function testAdapterNotSupported()
    {
        try {
            $storage = StorageFactory::adapterFactory('xcache', [
                'admin_auth' => getenv('TESTS_ZEND_CACHE_XCACHE_ADMIN_AUTH') ?: false,
                'admin_user' => getenv('TESTS_ZEND_CACHE_XCACHE_ADMIN_USER') ?: '',
                'admin_pass' => getenv('TESTS_ZEND_CACHE_XCACHE_ADMIN_PASS') ?: '',
            ]);
            $storage->addPlugin(new Serializer());

            $deferredSkippedMessage = sprintf(
                '%s storage doesn\'t support driver deferred',
                \get_class($storage)
            );
            $this->skippedTests['testHasItemReturnsFalseWhenDeferredItemIsExpired'] = $deferredSkippedMessage;

            return new CacheItemPoolDecorator($storage);
        } catch (Exception\ExtensionNotLoadedException $e) {
            $this->markTestSkipped($e->getMessage());
        } catch (ServiceNotCreatedException $e) {
            if ($e->getPrevious() instanceof Exception\ExtensionNotLoadedException) {
                $this->markTestSkipped($e->getMessage());
            }
            throw $e;
        }
    }
}
