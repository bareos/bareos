<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Filter;

use PHPUnit\Framework\TestCase;
use Zend\Filter\Exception;
use Zend\Filter\ToNull as ToNullFilter;

class ToNullTest extends TestCase
{
    public function testConstructorOptions()
    {
        $filter = new ToNullFilter([
            'type' => ToNullFilter::TYPE_INTEGER,
        ]);

        $this->assertEquals(ToNullFilter::TYPE_INTEGER, $filter->getType());
    }

    public function testConstructorParams()
    {
        $filter = new ToNullFilter(ToNullFilter::TYPE_INTEGER);

        $this->assertEquals(ToNullFilter::TYPE_INTEGER, $filter->getType());
    }

    /**
     * @param mixed $value
     * @param bool  $expected
     * @dataProvider defaultTestProvider
     */
    public function testDefault($value, $expected)
    {
        $filter = new ToNullFilter();
        $this->assertSame($expected, $filter->filter($value));
    }

    /**
     * @param int $type
     * @param array $testData
     * @dataProvider typeTestProvider
     */
    public function testTypes($type, $testData)
    {
        $filter = new ToNullFilter($type);
        foreach ($testData as $data) {
            list($value, $expected) = $data;
            $message = sprintf(
                '%s (%s) is not filtered as %s; type = %s',
                var_export($value, true),
                gettype($value),
                var_export($expected, true),
                $type
            );
            $this->assertSame($expected, $filter->filter($value), $message);
        }
    }

    /**
     * @param array $typeData
     * @param array $testData
     * @dataProvider combinedTypeTestProvider
     */
    public function testCombinedTypes($typeData, $testData)
    {
        foreach ($typeData as $type) {
            $filter = new ToNullFilter(['type' => $type]);
            foreach ($testData as $data) {
                list($value, $expected) = $data;
                $message = sprintf(
                    '%s (%s) is not filtered as %s; type = %s',
                    var_export($value, true),
                    gettype($value),
                    var_export($expected, true),
                    var_export($type, true)
                );
                $this->assertSame($expected, $filter->filter($value), $message);
            }
        }
    }

    public function testSettingFalseType()
    {
        $filter = new ToNullFilter();
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('Unknown type value');
        $filter->setType(true);
    }

    public function testGettingDefaultType()
    {
        $filter = new ToNullFilter();
        $this->assertEquals(63, $filter->getType());
    }

    /**
     * Ensures that providing a duplicate initializing type results in the expected type
     *
     * @param mixed $type Type to duplicate initialization
     * @param mixed $expected Expected resulting type
     *
     * @dataProvider duplicateTypeProvider
     */
    public function testDuplicateInitializationResultsInCorrectType($type, $expected)
    {
        $filter = new ToNullFilter([$type, $type]);
        $this->assertEquals($expected, $filter->getType());
    }

    public static function duplicateTypeProvider()
    {
        return [
            [ToNullFilter::TYPE_BOOLEAN, ToNullFilter::TYPE_BOOLEAN,],
            [ToNullFilter::TYPE_INTEGER, ToNullFilter::TYPE_INTEGER,],
            [ToNullFilter::TYPE_EMPTY_ARRAY, ToNullFilter::TYPE_EMPTY_ARRAY,],
            [ToNullFilter::TYPE_STRING, ToNullFilter::TYPE_STRING,],
            [ToNullFilter::TYPE_ZERO_STRING, ToNullFilter::TYPE_ZERO_STRING,],
            [ToNullFilter::TYPE_FLOAT, ToNullFilter::TYPE_FLOAT,],
            [ToNullFilter::TYPE_ALL, ToNullFilter::TYPE_ALL,],
            ['boolean', ToNullFilter::TYPE_BOOLEAN,],
            ['integer', ToNullFilter::TYPE_INTEGER,],
            ['array', ToNullFilter::TYPE_EMPTY_ARRAY,],
            ['string', ToNullFilter::TYPE_STRING,],
            ['zero', ToNullFilter::TYPE_ZERO_STRING,],
            ['float', ToNullFilter::TYPE_FLOAT,],
            ['all', ToNullFilter::TYPE_ALL,],
        ];
    }

    public static function defaultTestProvider()
    {
        return [
            [null, null],
            [false, null],
            [true, true],
            [0, null],
            [1, 1],
            [0.0, null],
            [1.0, 1.0],
            ['', null],
            ['abc', 'abc'],
            ['0', null],
            ['1', '1'],
            [[], null],
            [[0], [0]],
        ];
    }

    public static function typeTestProvider()
    {
        return [
            [
                ToNullFilter::TYPE_BOOLEAN,
                [
                    [null, null],
                    [false, null],
                    [true, true],
                    [0, 0],
                    [1, 1],
                    [0.0, 0.0],
                    [1.0, 1.0],
                    ['', ''],
                    ['abc', 'abc'],
                    ['0', '0'],
                    ['1', '1'],
                    [[], []],
                    [[0], [0]],
                ]
            ],
            [
                ToNullFilter::TYPE_INTEGER,
                [
                    [null, null],
                    [false, false],
                    [true, true],
                    [0, null],
                    [1, 1],
                    [0.0, 0.0],
                    [1.0, 1.0],
                    ['', ''],
                    ['abc', 'abc'],
                    ['0', '0'],
                    ['1', '1'],
                    [[], []],
                    [[0], [0]],
                ]
            ],
            [
                ToNullFilter::TYPE_EMPTY_ARRAY,
                [
                    [null, null],
                    [false, false],
                    [true, true],
                    [0, 0],
                    [1, 1],
                    [0.0, 0.0],
                    [1.0, 1.0],
                    ['', ''],
                    ['abc', 'abc'],
                    ['0', '0'],
                    ['1', '1'],
                    [[], null],
                    [[0], [0]],
                ]
            ],
            [
                ToNullFilter::TYPE_STRING,
                [
                    [null, null],
                    [false, false],
                    [true, true],
                    [0, 0],
                    [1, 1],
                    [0.0, 0.0],
                    [1.0, 1.0],
                    ['', null],
                    ['abc', 'abc'],
                    ['0', '0'],
                    ['1', '1'],
                    [[], []],
                    [[0], [0]],
                ]
            ],
            [
                ToNullFilter::TYPE_ZERO_STRING,
                [
                    [null, null],
                    [false, false],
                    [true, true],
                    [0, 0],
                    [1, 1],
                    [0.0, 0.0],
                    [1.0, 1.0],
                    ['', ''],
                    ['abc', 'abc'],
                    ['0', null],
                    ['1', '1'],
                    [[], []],
                    [[0], [0]],
                ]
            ],
            [
                ToNullFilter::TYPE_FLOAT,
                [
                    [null, null],
                    [false, false],
                    [true, true],
                    [0, 0],
                    [1, 1],
                    [0.0, null],
                    [1.0, 1.0],
                    ['', ''],
                    ['abc', 'abc'],
                    ['0', '0'],
                    ['1', '1'],
                    [[], []],
                    [[0], [0]],
                ]
            ],
            [
                ToNullFilter::TYPE_ALL,
                [
                    [null, null],
                    [false, null],
                    [true, true],
                    [0, null],
                    [1, 1],
                    [0.0, null],
                    [1.0, 1.0],
                    ['', null],
                    ['abc', 'abc'],
                    ['0', null],
                    ['1', '1'],
                    [[], null],
                    [[0], [0]],
                ]
            ],
        ];
    }

    public static function combinedTypeTestProvider()
    {
        return [
            [
                [
                    [
                        ToNullFilter::TYPE_ZERO_STRING,
                        ToNullFilter::TYPE_STRING,
                        ToNullFilter::TYPE_BOOLEAN,
                    ],
                    [
                        'zero',
                        'string',
                        'boolean',
                    ],
                    ToNullFilter::TYPE_ZERO_STRING | ToNullFilter::TYPE_STRING | ToNullFilter::TYPE_BOOLEAN,
                    ToNullFilter::TYPE_ZERO_STRING + ToNullFilter::TYPE_STRING + ToNullFilter::TYPE_BOOLEAN,
                ],
                [
                    [null, null],
                    [false, null],
                    [true, true],
                    [0, 0],
                    [1, 1],
                    [0.0, 0.0],
                    [1.0, 1.0],
                    ['', null],
                    ['abc', 'abc'],
                    ['0', null],
                    ['1', '1'],
                    [[], []],
                    [[0], [0]],
                ]
            ]
        ];
    }
}
