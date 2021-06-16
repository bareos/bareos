<?php
/**
 * @see       https://github.com/zendframework/zend-filter for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-filter/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Filter;

use PHPUnit\Framework\TestCase;
use stdClass;
use Zend\Filter\Exception\InvalidArgumentException;
use Zend\Filter\StringPrefix as StringPrefixFilter;

class StringPrefixTest extends TestCase
{
    /**
     * @var StringPrefixFilter
     */
    protected $filter;

    /**
     * @return void
     */
    public function setUp()
    {
        $this->filter = new StringPrefixFilter();
    }

    /**
     * Ensures that the filter follows expected behavior
     *
     * @return void
     */
    public function testBasic()
    {
        $filter = $this->filter;

        $prefix = 'ABC123';
        $filter->setPrefix($prefix);

        $this->assertStringStartsWith($prefix, $filter('sample'));
    }

    public function testWithoutPrefix()
    {
        $filter = $this->filter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('expects a "prefix" option; none given');
        $filter('sample');
    }

    /**
     * @return array
     */
    public function invalidPrefixesDataProvider()
    {
        return [
            'int'                 => [1],
            'float'               => [1.00],
            'true'                => [true],
            'null'                => [null],
            'empty array'         => [[]],
            'resource'            => [fopen('php://memory', 'rb+')],
            'array with callable' => [
                function () {
                },
            ],
            'object'              => [new stdClass()],
        ];
    }

    /**
     * @dataProvider invalidPrefixesDataProvider
     *
     * @param mixed $prefix
     */
    public function testInvalidPrefixes($prefix)
    {
        $filter = $this->filter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('expects "prefix" to be string');

        $filter->setPrefix($prefix);
        $filter('sample');
    }

    public function testNonScalarInput()
    {
        $filter = $this->filter;

        $prefix = 'ABC123';
        $filter->setPrefix($prefix);

        $this->assertInstanceOf(stdClass::class, $filter(new stdClass()));
    }
}
