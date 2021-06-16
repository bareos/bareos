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
use Zend\Session\Storage\SessionArrayStorage;
use Zend\Session\SessionManager;
use Zend\Session\Container;

/**
 * @group      Zend_Session
 * @covers Zend\Session\Storage\SessionArrayStorage
 */
class SessionArrayStorageTest extends TestCase
{
    public function setUp()
    {
        $_SESSION = [];
        $this->storage = new SessionArrayStorage;
    }

    public function tearDown()
    {
        $_SESSION = [];
    }

    public function testStorageWritesToSessionSuperglobal()
    {
        $this->storage['foo'] = 'bar';
        $this->assertSame($_SESSION['foo'], $this->storage->foo);
        unset($this->storage['foo']);
        $this->assertArrayNotHasKey('foo', $_SESSION);
    }

    public function testPassingArrayToConstructorOverwritesSessionSuperglobal()
    {
        $_SESSION['foo'] = 'bar';
        $array   = ['foo' => 'FOO'];
        $storage = new SessionArrayStorage($array);
        $expected = [
            'foo' => 'FOO',
            '__ZF' => [
                '_REQUEST_ACCESS_TIME' => $storage->getRequestAccessTime(),
            ],
        ];
        $this->assertSame($expected, $_SESSION);
    }

    public function testModifyingSessionSuperglobalDirectlyUpdatesStorage()
    {
        $_SESSION['foo'] = 'bar';
        $this->assertTrue(isset($this->storage['foo']));
    }

    public function testDestructorSetsSessionToArray()
    {
        $this->storage->foo = 'bar';
        $expected = [
            '__ZF' => [
                '_REQUEST_ACCESS_TIME' => $this->storage->getRequestAccessTime(),
            ],
            'foo' => 'bar',
        ];
        $this->storage->__destruct();
        $this->assertSame($expected, $_SESSION);
    }

    public function testModifyingOneSessionObjectModifiesTheOther()
    {
        $this->storage->foo = 'bar';
        $storage = new SessionArrayStorage();
        $storage->bar = 'foo';
        $this->assertEquals('foo', $this->storage->bar);
    }

    public function testMarkingOneSessionObjectImmutableShouldMarkOtherInstancesImmutable()
    {
        $this->storage->foo = 'bar';
        $storage = new SessionArrayStorage();
        $this->assertEquals('bar', $storage['foo']);
        $this->storage->markImmutable();
        $this->assertTrue($storage->isImmutable(), var_export($_SESSION, 1));
    }

    public function testAssignment()
    {
        $_SESSION['foo'] = 'bar';
        $this->assertEquals('bar', $this->storage['foo']);
    }

    public function testMultiDimensionalAssignment()
    {
        $_SESSION['foo']['bar'] = 'baz';
        $this->assertEquals('baz', $this->storage['foo']['bar']);
    }

    public function testUnset()
    {
        $_SESSION['foo'] = 'bar';
        unset($_SESSION['foo']);
        $this->assertFalse(isset($this->storage['foo']));
    }

    public function testMultiDimensionalUnset()
    {
        $this->storage['foo'] = ['bar' => ['baz' => 'boo']];
        unset($this->storage['foo']['bar']['baz']);
        $this->assertFalse(isset($this->storage['foo']['bar']['baz']));
        unset($this->storage['foo']['bar']);
        $this->assertFalse(isset($this->storage['foo']['bar']));
    }

    public function testSessionWorksWithContainer()
    {
        // Run without any validators; session ID is often invalid in CLI
        $container = new Container(
            'test',
            new SessionManager(null, null, null, [], ['attach_default_validators' => false])
        );
        $container->foo = 'bar';

        $this->assertSame($container->foo, $_SESSION['test']['foo']);
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

    public function testUndefinedSessionManipulation()
    {
        $this->storage['foo'] = 'bar';
        $this->storage['bar'][] = 'bar';
        $this->storage['baz']['foo'] = 'bar';

        $expected = [
            '__ZF' => [
                '_REQUEST_ACCESS_TIME' => $this->storage->getRequestAccessTime(),
            ],
            'foo' => 'bar',
            'bar' => ['bar'],
            'baz' => ['foo' => 'bar'],
        ];
        $this->assertSame($expected, $this->storage->toArray(true));
    }

    /**
     * @runInSeparateProcess
     */
    public function testExpirationHops()
    {
        // since we cannot explicitly test reinitializing the session
        // we will act in how session manager would in this case.
        $storage = new SessionArrayStorage();
        $manager = new SessionManager(null, $storage);
        $manager->start();

        $container = new Container('test');
        $container->foo = 'bar';
        $container->setExpirationHops(1);

        $copy = $_SESSION;
        $_SESSION = null;
        $storage->init($copy);
        $this->assertEquals('bar', $container->foo);

        $copy = $_SESSION;
        $_SESSION = null;
        $storage->init($copy);
        $this->assertNull($container->foo);
    }

    /**
     * @runInSeparateProcess
     */
    public function testPreserveRequestAccessTimeAfterStart()
    {
        $manager = new SessionManager(null, $this->storage);
        $this->assertGreaterThan(0, $this->storage->getRequestAccessTime());
        $manager->start();
        $this->assertGreaterThan(0, $this->storage->getRequestAccessTime());
    }

    public function testGetArrayCopyFromContainer()
    {
        $container = new Container('test');
        $container->foo = 'bar';
        $container->baz = 'qux';
        $this->assertSame(['foo' => 'bar', 'baz' => 'qux'], $container->getArrayCopy());
    }
}
