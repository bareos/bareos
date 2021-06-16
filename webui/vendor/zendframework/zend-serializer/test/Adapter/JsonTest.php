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

/**
 * @group      Zend_Serializer
 * @covers \Zend\Serializer\Adapter\Json
 */
class JsonTest extends TestCase
{
    /**
     * @var Serializer\Adapter\Json
     */
    private $adapter;

    protected function setUp()
    {
        $this->adapter = new Serializer\Adapter\Json();
    }

    protected function tearDown()
    {
        $this->adapter = null;
    }

    public function testAdapterAcceptsOptions()
    {
        $adapter = new Serializer\Adapter\Json();
        $options = new Serializer\Adapter\JsonOptions([
            'cycle_check'             => true,
            'enable_json_expr_finder' => true,
            'object_decode_type'      => 1,
        ]);
        $adapter->setOptions($options);

        $this->assertEquals(true, $adapter->getOptions()->getCycleCheck());
        $this->assertEquals(true, $adapter->getOptions()->getEnableJsonExprFinder());
        $this->assertEquals(1, $adapter->getOptions()->getObjectDecodeType());
    }

    public function testSerializeString()
    {
        $value    = 'test';
        $expected = '"test"';

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeFalse()
    {
        $value    = false;
        $expected = 'false';

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeNull()
    {
        $value    = null;
        $expected = 'null';

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeNumeric()
    {
        $value    = 100;
        $expected = '100';

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testSerializeObject()
    {
        $value       = new \stdClass();
        $value->test = "test";
        $expected    = '{"test":"test"}';

        $data = $this->adapter->serialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeString()
    {
        $value    = '"test"';
        $expected = 'test';

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeFalse()
    {
        $value    = 'false';
        $expected = false;

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeNull()
    {
        $value    = 'null';
        $expected = null;

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeNumeric()
    {
        $value    = '100';
        $expected = 100;

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeAsArray()
    {
        $value    = '{"test":"test"}';
        $expected = ['test' => 'test'];

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserializeAsObject()
    {
        $value      = '{"test":"test"}';
        $expected   = new \stdClass();
        $expected->test = 'test';

        $this->adapter->getOptions()->setObjectDecodeType(\Zend\Json\Json::TYPE_OBJECT);

        $data = $this->adapter->unserialize($value);
        $this->assertEquals($expected, $data);
    }

    public function testUnserialzeInvalid()
    {
        $value = 'not a serialized string';
        $this->expectException('Zend\Serializer\Exception\RuntimeException');
        $this->expectExceptionMessage('Unserialization failed: Decoding failed: Syntax error');
        $this->adapter->unserialize($value);
    }
}
