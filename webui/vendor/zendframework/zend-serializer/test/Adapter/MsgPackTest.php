<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Serializer\Adapter;

use PHPUnit\Framework\TestCase;
use Zend\Serializer;
use Zend\Serializer\Exception\ExtensionNotLoadedException;

/**
 * @group      Zend_Serializer
 * @covers \Zend\Serializer\Adapter\MsgPack
 */
class MsgPackTest extends TestCase
{
    /**
     * @var Serializer\Adapter\MsgPack
     */
    private $adapter;

    protected function setUp()
    {
        if (! extension_loaded('msgpack')) {
            try {
                new Serializer\Adapter\MsgPack();
                $this->fail("Zend\\Serializer\\Adapter\\MsgPack needs missing ext/msgpack but did't throw exception");
            } catch (ExtensionNotLoadedException $e) {
            }
            $this->markTestSkipped('Zend\\Serializer\\Adapter\\MsgPack needs ext/msgpack');
        }
        $this->adapter = new Serializer\Adapter\MsgPack();
    }

    protected function tearDown()
    {
        $this->adapter = null;
    }

    public function testSerializeString()
    {
        $value    = 'test';
        $expected = msgpack_serialize($value);

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeFalse()
    {
        $value    = false;
        $expected = msgpack_serialize($value);

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeNull()
    {
        $value    = null;
        $expected = msgpack_serialize($value);

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeNumeric()
    {
        $value    = 100;
        $expected = msgpack_serialize($value);

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeObject()
    {
        $value    = new \stdClass();
        $expected = msgpack_serialize($value);

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeString()
    {
        $expected = 'test';
        $value    = msgpack_serialize($expected);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeFalse()
    {
        $expected = false;
        $value    = msgpack_serialize($expected);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeNull()
    {
        $expected = null;
        $value    = msgpack_serialize($expected);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeNumeric()
    {
        $expected = 100;
        $value    = msgpack_serialize($expected);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeObject()
    {
        $expected = new \stdClass();
        $value    = msgpack_serialize($expected);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserialize0()
    {
        $expected = 0;
        $value    = msgpack_serialize($expected);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeInvalid()
    {
        $value = "\0\1\r\n";
        $this->expectException('Zend\Serializer\Exception\RuntimeException');
        $this->expectExceptionMessage('Unserialization failed');
        $this->adapter->unserialize($value);
    }
}
