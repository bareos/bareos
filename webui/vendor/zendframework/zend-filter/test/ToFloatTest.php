<?php
/**
 * @see       https://github.com/zendframework/zend-filter for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-filter/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Filter;

use PHPUnit\Framework\TestCase;
use stdClass;
use Zend\Filter\ToFloat as ToFloatFilter;

class ToFloatTest extends TestCase
{
    public function filterableValuesProvider()
    {
        return [
            'string word' => ['string', 0],
            'string 1'    => ['1', 1],
            'string -1'   => ['-1', -1],
            'string 1.1'  => ['1.1', 1.1],
            'string -1.1' => ['-1.1', -1.1],
            'string 0.9'  => ['0.9', 0.9],
            'string -0.9' => ['-0.9', -0.9],
            'integer 1'   => [1, 1.0],
            'integer -1'  => [-1, -1.0],
            'true'        => [true, 1.0],
            'false'       => [false, 0.0],
            'float 1.1'   => [1.1, 1.1],
        ];
    }

    /**
     * Ensures that the filter follows expected behavior
     *
     * @dataProvider filterableValuesProvider
     * @param mixed $input
     * @param string $expectedOutput
     */
    public function testCanFilterScalarValuesAsExpected($input, $expectedOutput)
    {
        $filter = new ToFloatFilter();
        $this->assertEquals($expectedOutput, $filter($input));
    }

    public function unfilterableValuesProvider()
    {
        return [
            'null'   => [null],
            'array'  => [[
                '1',
                -1,
            ]],
            'object' => [new stdClass()],
        ];
    }

    /**
     * @dataProvider unfilterableValuesProvider
     * @param mixed $input
     */
    public function testReturnsUnfilterableInputVerbatim($input)
    {
        $filter = new ToFloatFilter();
        $this->assertEquals($input, $filter($input));
    }
}
