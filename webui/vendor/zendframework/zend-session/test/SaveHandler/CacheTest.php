<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Session\SaveHandler;

use PHPUnit\Framework\TestCase;
use Prophecy\Argument;
use Zend\Session\SaveHandler\Cache;

/**
 * Unit testing for DbTable include all tests for
 * regular session handling
 *
 * @group      Zend_Session
 * @group      Zend_Cache
 * @covers Zend\Session\SaveHandler\Cache
 */
class CacheTest extends TestCase
{
    /**
     * @var CacheAdapter
     */
    protected $cache;

    /**
     * @var array
     */
    protected $testArray;

    /**
     * Array to collect used Cache objects, so they are not
     * destroyed before all tests are done and session is not closed
     *
     * @var array
     */
    protected $usedSaveHandlers = [];

    public function setUp()
    {
        $this->testArray = ['foo' => 'bar', 'bar' => ['foo' => 'bar']];
    }

    public function testReadWrite()
    {
        $cacheStorage = $this->prophesize('Zend\Cache\Storage\StorageInterface');
        $cacheStorage->setItem('242', Argument::type('string'))
            ->will(function ($args) {
                $this->getItem('242')->willReturn($args[1]);
                return true;
            });
        $this->usedSaveHandlers[] = $saveHandler = new Cache($cacheStorage->reveal());

        $id = '242';

        $this->assertTrue($saveHandler->write($id, serialize($this->testArray)));

        $data = unserialize($saveHandler->read($id));
        $this->assertEquals(
            $this->testArray,
            $data,
            'Expected ' . var_export($this->testArray, 1) . "\nbut got: " . var_export($data, 1)
        );
    }

    public function testReadWriteComplex()
    {
        $cacheStorage = $this->prophesize('Zend\Cache\Storage\StorageInterface');
        $cacheStorage->setItem('242', Argument::type('string'))
            ->will(function ($args) {
                $this->getItem('242')->willReturn($args[1]);
                return true;
            });
        $this->usedSaveHandlers[] = $saveHandler = new Cache($cacheStorage->reveal());
        $saveHandler->open('savepath', 'sessionname');

        $id = '242';

        $this->assertTrue($saveHandler->write($id, serialize($this->testArray)));

        $this->assertEquals($this->testArray, unserialize($saveHandler->read($id)));
    }

    public function testReadWriteTwice()
    {
        $cacheStorage = $this->prophesize('Zend\Cache\Storage\StorageInterface');
        $cacheStorage->setItem('242', Argument::type('string'))
            ->will(function ($args) {
                $this->getItem('242')->willReturn($args[1])->shouldBeCalledTimes(2);
                return true;
            })
            ->shouldBeCalledTimes(2);
        $this->usedSaveHandlers[] = $saveHandler = new Cache($cacheStorage->reveal());

        $id = '242';

        $this->assertTrue($saveHandler->write($id, serialize($this->testArray)));

        $this->assertEquals($this->testArray, unserialize($saveHandler->read($id)));

        $this->assertTrue($saveHandler->write($id, serialize($this->testArray)));

        $this->assertEquals($this->testArray, unserialize($saveHandler->read($id)));
    }

    public function testReadShouldAlwaysReturnString()
    {
        $cacheStorage = $this->prophesize('Zend\Cache\Storage\StorageInterface');
        $cacheStorage->getItem('242')->willReturn(null);
        $this->usedSaveHandlers[] = $saveHandler = new Cache($cacheStorage->reveal());

        $id = '242';

        $data = $saveHandler->read($id);

        $this->assertTrue(is_string($data));
    }

    public function testDestroyReturnsTrueEvenWhenSessionDoesNotExist()
    {
        $cacheStorage = $this->prophesize('Zend\Cache\Storage\StorageInterface');
        $this->usedSaveHandlers[] = $saveHandler = new Cache($cacheStorage->reveal());

        $id = '242';

        $result = $saveHandler->destroy($id);

        $this->assertTrue($result);
    }

    public function testDestroyReturnsTrueWhenSessionIsDeleted()
    {
        $cacheStorage = $this->prophesize('Zend\Cache\Storage\StorageInterface');
        $cacheStorage->setItem('242', Argument::type('string'))
            ->will(function ($args) {
                $this->getItem('242', Argument::any())
                    ->will(function ($args) {
                        $return =& $args[1];
                        return $return;
                    });
                return true;
            });
        $this->usedSaveHandlers[] = $saveHandler = new Cache($cacheStorage->reveal());

        $id = '242';

        $this->assertTrue($saveHandler->write($id, serialize($this->testArray)));

        $result = $saveHandler->destroy($id);

        $this->assertTrue($result);
    }
}
