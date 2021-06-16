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
use Zend\Session\Storage\ArrayStorage;

/**
 * @group      Zend_Session
 * @covers Zend\Session\Storage\ArrayStorage
 */
class StorageTest extends TestCase
{
    /**
     * @var ArrayStorage
     */
    protected $storage;

    public function setUp()
    {
        $this->storage = new ArrayStorage;
    }

    public function testStorageAllowsArrayAccess()
    {
        $this->storage['foo'] = 'bar';
        $this->assertTrue(isset($this->storage['foo']));
        $this->assertEquals('bar', $this->storage['foo']);
        unset($this->storage['foo']);
        $this->assertFalse(isset($this->storage['foo']));
    }

    public function testStorageAllowsPropertyAccess()
    {
        $this->storage->foo = 'bar';
        $this->assertTrue(isset($this->storage->foo));
        $this->assertEquals('bar', $this->storage->foo);
        unset($this->storage->foo);
        $this->assertFalse(isset($this->storage->foo));
    }

    public function testStorageAllowsSettingMetadata()
    {
        $this->storage->setMetadata('TEST', 'foo');
        $this->assertEquals('foo', $this->storage->getMetadata('TEST'));
    }

    public function testSettingArrayMetadataMergesOnSubsequentRequests()
    {
        $this->storage->setMetadata('TEST', ['foo' => 'bar', 'bar' => 'baz']);
        $this->storage->setMetadata('TEST', ['foo' => 'baz', 'baz' => 'bat', 'lonesome']);
        $expected = ['foo' => 'baz', 'bar' => 'baz', 'baz' => 'bat', 'lonesome'];
        $this->assertEquals($expected, $this->storage->getMetadata('TEST'));
    }

    public function testSettingArrayMetadataWithOverwriteFlagOverwritesExistingData()
    {
        $this->storage->setMetadata('TEST', ['foo' => 'bar', 'bar' => 'baz']);
        $this->storage->setMetadata('TEST', ['foo' => 'baz', 'baz' => 'bat', 'lonesome'], true);
        $expected = ['foo' => 'baz', 'baz' => 'bat', 'lonesome'];
        $this->assertEquals($expected, $this->storage->getMetadata('TEST'));
    }

    public function testLockWithNoKeyMakesStorageReadOnly()
    {
        $this->storage->foo = 'bar';
        $this->storage->lock();
        $this->expectException('Zend\Session\Exception\RuntimeException');
        $this->expectExceptionMessage('Cannot set key "foo" due to locking');
        $this->storage->foo = 'baz';
    }

    public function testLockWithNoKeyMarksEntireStorageLocked()
    {
        $this->storage->foo = 'bar';
        $this->storage->bar = 'baz';
        $this->storage->lock();
        $this->assertTrue($this->storage->isLocked());
        $this->assertTrue($this->storage->isLocked('foo'));
        $this->assertTrue($this->storage->isLocked('bar'));
    }

    public function testLockWithKeyMakesOnlyThatKeyReadOnly()
    {
        $this->storage->foo = 'bar';
        $this->storage->lock('foo');

        $this->storage->bar = 'baz';
        $this->assertEquals('baz', $this->storage->bar);

        $this->expectException('Zend\Session\Exception\RuntimeException');
        $this->expectExceptionMessage('Cannot set key "foo" due to locking');
        $this->storage->foo = 'baz';
    }

    public function testLockWithKeyMarksOnlyThatKeyLocked()
    {
        $this->storage->foo = 'bar';
        $this->storage->bar = 'baz';
        $this->storage->lock('foo');
        $this->assertTrue($this->storage->isLocked('foo'));
        $this->assertFalse($this->storage->isLocked('bar'));
    }

    public function testLockWithNoKeyShouldWriteToMetadata()
    {
        $this->storage->foo = 'bar';
        $this->storage->lock();
        $locked = $this->storage->getMetadata('_READONLY');
        $this->assertTrue($locked);
    }

    public function testLockWithKeyShouldWriteToMetadata()
    {
        $this->storage->foo = 'bar';
        $this->storage->lock('foo');
        $locks = $this->storage->getMetadata('_LOCKS');
        $this->assertInternalType('array', $locks);
        $this->assertArrayHasKey('foo', $locks);
    }

    public function testUnlockShouldUnlockEntireObject()
    {
        $this->storage->foo = 'bar';
        $this->storage->bar = 'baz';
        $this->storage->lock();
        $this->storage->unlock();
        $this->assertFalse($this->storage->isLocked('foo'));
        $this->assertFalse($this->storage->isLocked('bar'));
    }

    public function testUnlockShouldUnlockSelectivelyLockedKeys()
    {
        $this->storage->foo = 'bar';
        $this->storage->bar = 'baz';
        $this->storage->lock('foo');
        $this->storage->unlock();
        $this->assertFalse($this->storage->isLocked('foo'));
        $this->assertFalse($this->storage->isLocked('bar'));
    }

    public function testUnlockWithKeyShouldUnlockOnlyThatKey()
    {
        $this->storage->foo = 'bar';
        $this->storage->bar = 'baz';
        $this->storage->lock();
        $this->storage->unlock('foo');
        $this->assertFalse($this->storage->isLocked('foo'));
        $this->assertTrue($this->storage->isLocked('bar'));
    }

    public function testUnlockWithKeyOfSelectiveLockShouldUnlockThatKey()
    {
        $this->storage->foo = 'bar';
        $this->storage->lock('foo');
        $this->storage->unlock('foo');
        $this->assertFalse($this->storage->isLocked('foo'));
    }

    public function testClearWithNoArgumentsRemovesExistingData()
    {
        $this->storage->foo = 'bar';
        $this->storage->bar = 'baz';

        $this->storage->clear();
        $data = $this->storage->toArray();
        $this->assertSame([], $data);
    }

    public function testClearWithNoArgumentsRemovesExistingMetadata()
    {
        $this->storage->foo = 'bar';
        $this->storage->lock('foo');
        $this->storage->setMetadata('FOO', 'bar');
        $this->storage->clear();

        $this->assertFalse($this->storage->isLocked('foo'));
        $this->assertFalse($this->storage->getMetadata('FOO'));
    }

    public function testClearWithArgumentRemovesExistingDataForThatKeyOnly()
    {
        $this->storage->foo = 'bar';
        $this->storage->bar = 'baz';

        $this->storage->clear('foo');
        $data = $this->storage->toArray();
        $this->assertSame(['bar' => 'baz'], $data);
    }

    public function testClearWithArgumentRemovesExistingMetadataForThatKeyOnly()
    {
        $this->storage->foo = 'bar';
        $this->storage->bar = 'baz';
        $this->storage->lock('foo');
        $this->storage->lock('bar');
        $this->storage->setMetadata('foo', 'bar');
        $this->storage->setMetadata('bar', 'baz');
        $this->storage->clear('foo');

        $this->assertFalse($this->storage->isLocked('foo'));
        $this->assertTrue($this->storage->isLocked('bar'));
        $this->assertFalse($this->storage->getMetadata('foo'));
        $this->assertEquals('baz', $this->storage->getMetadata('bar'));
    }

    public function testClearWhenStorageMarkedImmutableRaisesException()
    {
        $this->storage->foo = 'bar';
        $this->storage->bar = 'baz';
        $this->storage->markImmutable();
        $this->expectException('Zend\Session\Exception\RuntimeException');
        $this->expectExceptionMessage('Cannot clear storage as it is marked immutable');
        $this->storage->clear();
    }

    public function testRequestAccessTimeIsPreservedEvenInFactoryMethod()
    {
        $this->assertNotEmpty($this->storage->getRequestAccessTime());
        $this->storage->fromArray([]);
        $this->assertNotEmpty($this->storage->getRequestAccessTime());
    }

    public function testToArrayWithMetaData()
    {
        $this->storage->foo = 'bar';
        $this->storage->bar = 'baz';
        $this->storage->setMetadata('foo', 'bar');
        $expected = [
            '__ZF' => [
                '_REQUEST_ACCESS_TIME' => $this->storage->getRequestAccessTime(),
                'foo' => 'bar',
            ],
            'foo' => 'bar',
            'bar' => 'baz',
        ];
        $this->assertSame($expected, $this->storage->toArray(true));
    }

    public function testUnsetMultidimensional()
    {
        $this->storage['foo'] = ['bar' => ['baz' => 'boo']];
        unset($this->storage['foo']['bar']['baz']);
        unset($this->storage['foo']['bar']);

        $this->assertFalse(isset($this->storage['foo']['bar']));
    }
}
