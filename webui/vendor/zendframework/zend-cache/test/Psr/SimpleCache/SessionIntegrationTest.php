<?php
/**
 * @see       https://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-cache/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Cache\Psr\SimpleCache;

use Cache\IntegrationTests\SimpleCacheTest;
use Zend\Cache\Psr\SimpleCache\SimpleCacheDecorator;
use Zend\Cache\StorageFactory;
use Zend\Session\Container as SessionContainer;

class SessionIntegrationTest extends SimpleCacheTest
{
    public function setUp()
    {
        if (! class_exists(SessionContainer::class)) {
            $this->markTestSkipped('Install zend-session to enable this test');
        }

        $_SESSION = [];
        SessionContainer::setDefaultManager(null);

        $this->skippedTests['testSetTtl'] = 'Session adapter does not honor TTL';
        $this->skippedTests['testSetMultipleTtl'] = 'Session adapter does not honor TTL';
        $this->skippedTests['testObjectDoesNotChangeInCache'] =
            'Session adapter stores objects in memory; so change in references is possible';

        parent::setUp();
    }

    public function tearDown()
    {
        if (! class_exists(SessionContainer::class)) {
            return;
        }

        $_SESSION = [];
        SessionContainer::setDefaultManager(null);

        parent::tearDown();
    }

    public function createSimpleCache()
    {
        $sessionContainer = new SessionContainer('Default');
        $storage = StorageFactory::adapterfactory('session', [
            'session_container' => $sessionContainer,
        ]);
        return new SimpleCacheDecorator($storage);
    }
}
