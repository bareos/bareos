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
use Zend\Filter\StringSuffix as StringSuffixFilter;

class StringSuffixTest extends TestCase
{
    /**
     * @var StringSuffixFilter
     */
    protected $filter;

    /**
     * @return void
     */
    public function setUp()
    {
        $this->filter = new StringSuffixFilter();
    }

    /**
     * Ensures that the filter follows expected behavior
     *
     * @return void
     */
    public function testBasic()
    {
        $filter = $this->filter;

        $suffix = 'ABC123';
        $filter->setSuffix($suffix);

        $this->assertStringEndsWith($suffix, $filter('sample'));
    }

    public function testWithoutSuffix()
    {
        $filter = $this->filter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('expects a "suffix" option; none given');
        $filter('sample');
    }

    /**
     * @return array
     */
    public function invalidSuffixesDataProvider()
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
     * @dataProvider invalidSuffixesDataProvider
     *
     * @param mixed $suffix
     */
    public function testInvalidSuffixes($suffix)
    {
        $filter = $this->filter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('expects "suffix" to be string');

        $filter->setSuffix($suffix);
        $filter('sample');
    }

    public function testNonScalarInput()
    {
        $filter = $this->filter;

        $suffix = 'ABC123';
        $filter->setSuffix($suffix);

        $this->assertInstanceOf(stdClass::class, $filter(new stdClass()));
    }
}
