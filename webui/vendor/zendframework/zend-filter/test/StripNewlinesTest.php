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
use Zend\Filter\StripNewlines as StripNewlinesFilter;

class StripNewlinesTest extends TestCase
{
    /**
     * Ensures that the filter follows expected behavior
     *
     * @return void
     */
    public function testBasic()
    {
        $filter = new StripNewLinesFilter();
        $valuesExpected = [
            '' => '',
            "\n" => '',
            "\r" => '',
            "\r\n" => '',
            '\n' => '\n',
            '\r' => '\r',
            '\r\n' => '\r\n',
            "Some text\nthat we have\r\nstuff in" => 'Some textthat we havestuff in'
        ];
        foreach ($valuesExpected as $input => $output) {
            $this->assertEquals($output, $filter($input));
        }
    }

    /**
     *
     * @return void
     */
    public function testArrayValues()
    {
        $filter = new StripNewLinesFilter();
        $expected = [
            "Some text\nthat we have\r\nstuff in" => 'Some textthat we havestuff in',
            "Some text\n" => 'Some text'
        ];
        $this->assertEquals(array_values($expected), $filter(array_keys($expected)));
    }

    public function returnUnfilteredDataProvider()
    {
        return [
            [null],
            [new \stdClass()]
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $filter = new StripNewLinesFilter();

        $this->assertEquals($input, $filter($input));
    }
}
