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
 * @covers \Zend\Serializer\Adapter\IgBinary
 */
class IgBinaryTest extends TestCase
{
    /**
     * @var Serializer\Adapter\IgBinary
     */
    private $adapter;

    protected function setUp()
    {
        if (! extension_loaded('igbinary')) {
            try {
                new Serializer\Adapter\IgBinary();
                $this->fail("Zend\\Serializer\\Adapter\\IgBinary needs missing ext/igbinary but did't throw exception");
            } catch (ExtensionNotLoadedException $e) {
            }
            $this->markTestSkipped('Zend\\Serializer\\Adapter\\IgBinary needs ext/igbinary');
        }
        $this->adapter = new Serializer\Adapter\IgBinary();
    }

    protected function tearDown()
    {
        $this->adapter = null;
    }

    public function testSerializeString()
    {
        $value    = 'test';
        $expected = igbinary_serialize($value);

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeFalse()
    {
        $value    = false;
        $expected = igbinary_serialize($value);

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeNull()
    {
        $value    = null;
        $expected = igbinary_serialize($value);

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeNumeric()
    {
        $value    = 100;
        $expected = igbinary_serialize($value);

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeObject()
    {
        $value    = new \stdClass();
        $expected = igbinary_serialize($value);

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeString()
    {
        $expected = 'test';
        $value    = igbinary_serialize($expected);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeFalse()
    {
        $expected = false;
        $value    = igbinary_serialize($expected);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeNull()
    {
        $expected = null;
        $value    = igbinary_serialize($expected);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeNumeric()
    {
        $expected = 100;
        $value    = igbinary_serialize($expected);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeObject()
    {
        $expected = new \stdClass();
        $value    = igbinary_serialize($expected);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserialzeInvalid()
    {
        $value = "\0\1\r\n";
        $this->expectException('Zend\Serializer\Exception\RuntimeException');
        $this->expectExceptionMessage('Unserialization failed');
        $this->adapter->unserialize($value);
    }
}
