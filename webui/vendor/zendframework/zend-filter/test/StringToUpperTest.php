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
use Zend\Filter\StringToUpper as StringToUpperFilter;

class StringToUpperTest extends TestCase
{
    // @codingStandardsIgnoreStart
    /**
     * Zend_Filter_StringToLower object
     *
     * @var StringToUpperFilter
     */
    protected $_filter;
    // @codingStandardsIgnoreEnd

    /**
     * Creates a new Zend_Filter_StringToUpper object for each test method
     *
     * @return void
     */
    public function setUp()
    {
        $this->_filter = new StringToUpperFilter();
    }

    /**
     * Ensures that the filter follows expected behavior
     *
     * @return void
     */
    public function testBasic()
    {
        $filter = $this->_filter;
        $valuesExpected = [
            'STRING' => 'STRING',
            'ABC1@3' => 'ABC1@3',
            'A b C'  => 'A B C'
        ];

        foreach ($valuesExpected as $input => $output) {
            $this->assertEquals($output, $filter($input));
        }
    }

    /**
     * Ensures that the filter follows expected behavior with
     * specified encoding
     *
     * @return void
     */
    public function testWithEncoding()
    {
        $filter = $this->_filter;
        $valuesExpected = [
            'ü'     => 'Ü',
            'ñ'     => 'Ñ',
            'üñ123' => 'ÜÑ123'
        ];

        try {
            $filter->setEncoding('UTF-8');
            foreach ($valuesExpected as $input => $output) {
                $this->assertEquals($output, $filter($input));
            }
        } catch (Exception\ExtensionNotLoadedException $e) {
            $this->assertContains('mbstring is required', $e->getMessage());
        }
    }

    /**
     * @return void
     */
    public function testFalseEncoding()
    {
        if (! function_exists('mb_strtolower')) {
            $this->markTestSkipped('mbstring required');
        }

        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('is not supported');
        $this->_filter->setEncoding('aaaaa');
    }

    /**
     * @ZF-8989
     */
    public function testInitiationWithEncoding()
    {
        $valuesExpected = [
            'ü'     => 'Ü',
            'ñ'     => 'Ñ',
            'üñ123' => 'ÜÑ123'
        ];

        try {
            $filter = new StringToUpperFilter(['encoding' => 'UTF-8']);
            foreach ($valuesExpected as $input => $output) {
                $this->assertEquals($output, $filter($input));
            }
        } catch (Exception\ExtensionNotLoadedException $e) {
            $this->assertContains('mbstring is required', $e->getMessage());
        }
    }

    /**
     *  @ZF-9058
     */
    public function testCaseInsensitiveEncoding()
    {
        $filter = $this->_filter;
        $valuesExpected = [
            'ü'     => 'Ü',
            'ñ'     => 'Ñ',
            'üñ123' => 'ÜÑ123'
        ];

        try {
            $filter->setEncoding('UTF-8');
            foreach ($valuesExpected as $input => $output) {
                $this->assertEquals($output, $filter($input));
            }

            $this->_filter->setEncoding('utf-8');
            foreach ($valuesExpected as $input => $output) {
                $this->assertEquals($output, $filter($input));
            }

            $this->_filter->setEncoding('UtF-8');
            foreach ($valuesExpected as $input => $output) {
                $this->assertEquals($output, $filter($input));
            }
        } catch (Exception\ExtensionNotLoadedException $e) {
            $this->assertContains('mbstring is required', $e->getMessage());
        }
    }

    /**
     * @group ZF-9854
     */
    public function testDetectMbInternalEncoding()
    {
        if (! function_exists('mb_internal_encoding')) {
            $this->markTestSkipped("Function 'mb_internal_encoding' not available");
        }

        $this->assertEquals(mb_internal_encoding(), $this->_filter->getEncoding());
    }

    public function returnUnfilteredDataProvider()
    {
        return [
            [null],
            [new \stdClass()],
            [[
                'lower case written',
                'This should stay the same'
            ]]
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $this->assertEquals($input, $this->_filter->filter($input));
    }

    /**
     * @group 7147
     */
    public function testFilterUsesGetEncodingMethod()
    {
        $filterMock = $this->getMockBuilder(StringToUpperFilter::class)
            ->setMethods(['getEncoding'])
            ->getMock();
        $filterMock->expects($this->once())
            ->method('getEncoding')
            ->with();
        $filterMock->filter('foo');
    }
}
