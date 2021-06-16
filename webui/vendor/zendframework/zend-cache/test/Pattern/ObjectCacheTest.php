<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Pattern;

use Zend\Cache;
use ZendTest\Cache\Pattern\TestAsset\TestObjectCache;

/**
 * @group      Zend_Cache
 */
class ObjectCacheTest extends CommonPatternTest
{
    // @codingStandardsIgnoreStart
    /**
     * @var \Zend\Cache\Storage\StorageInterface
     */
    protected $_storage;
    // @codingStandardsIgnoreEnd

    public function setUp()
    {
        $this->_storage = new Cache\Storage\Adapter\Memory([
            'memory_limit' => 0
        ]);
        $this->_options = new Cache\Pattern\PatternOptions([
            'object'  => new TestObjectCache(),
            'storage' => $this->_storage,
        ]);
        $this->_pattern = new Cache\Pattern\ObjectCache();
        $this->_pattern->setOptions($this->_options);

        parent::setUp();
    }

    public function tearDown()
    {
        parent::tearDown();
    }

    public function getCommonPatternNamesProvider()
    {
        return [
            ['object'],
            ['Object'],
        ];
    }

    public function testCallEnabledCacheOutputByDefault()
    {
        $this->_testCall(
            'bar',
            ['testCallEnabledCacheOutputByDefault', 'arg2']
        );
    }

    public function testCallDisabledCacheOutput()
    {
        $this->_options->setCacheOutput(false);
        $this->_testCall(
            'bar',
            ['testCallDisabledCacheOutput', 'arg2']
        );
    }

    public function testCallInvoke()
    {
        $this->_options->setCacheOutput(false);
        $this->_testCall('__invoke', ['arg1', 'arg2']);
    }

    public function testGenerateKey()
    {
        $args = ['arg1', 2, 3.33, null];

        $generatedKey = $this->_pattern->generateKey('emptyMethod', $args);
        $usedKey      = null;
        $this->_options->getStorage()->getEventManager()->attach('setItem.pre', function ($event) use (&$usedKey) {
            $params = $event->getParams();
            $usedKey = $params['key'];
        });

        $this->_pattern->call('emptyMethod', $args);
        $this->assertEquals($generatedKey, $usedKey);
    }

    public function testSetProperty()
    {
        $this->_pattern->property = 'testSetProperty';
        $this->assertEquals('testSetProperty', $this->_options->getObject()->property);
    }

    public function testGetProperty()
    {
        $this->assertEquals($this->_options->getObject()->property, $this->_pattern->property);
    }

    public function testIssetProperty()
    {
        $this->assertTrue(isset($this->_pattern->property));
        $this->assertFalse(isset($this->_pattern->unknownProperty));
    }

    public function testUnsetProperty()
    {
        unset($this->_pattern->property);
        $this->assertFalse(isset($this->_pattern->property));
    }

    /**
     * @group 7039
     */
    public function testEmptyObjectKeys()
    {
        $this->_options->setObjectKey('0');
        $this->assertSame('0', $this->_options->getObjectKey(), "Can't set string '0' as object key");

        $this->_options->setObjectKey('');
        $this->assertSame('', $this->_options->getObjectKey(), "Can't set an empty string as object key");

        $this->_options->setObjectKey(null);
        $this->assertSame(get_class($this->_options->getObject()), $this->_options->getObjectKey());
    }

    // @codingStandardsIgnoreStart
    protected function _testCall($method, array $args)
    {
        // @codingStandardsIgnoreEnd
        $returnSpec = 'foobar_return(' . implode(', ', $args) . ') : ';
        $outputSpec = 'foobar_output(' . implode(', ', $args) . ') : ';
        $callback   = [$this->_pattern, $method];

        // first call - not cached
        $firstCounter = TestObjectCache::$fooCounter + 1;

        ob_start();
        ob_implicit_flush(0);
        $return = call_user_func_array($callback, $args);
        $data = ob_get_contents();
        ob_end_clean();

        $this->assertEquals($returnSpec . $firstCounter, $return);
        $this->assertEquals($outputSpec . $firstCounter, $data);

        // second call - cached
        ob_start();
        ob_implicit_flush(0);
        $return = call_user_func_array($callback, $args);
        $data = ob_get_contents();
        ob_end_clean();

        $this->assertEquals($returnSpec . $firstCounter, $return);
        if ($this->_options->getCacheOutput()) {
            $this->assertEquals($outputSpec . $firstCounter, $data);
        } else {
            $this->assertEquals('', $data);
        }
    }
}
