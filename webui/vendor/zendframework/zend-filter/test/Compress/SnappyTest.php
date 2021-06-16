<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Filter\Compress;

use PHPUnit\Framework\TestCase;
use Zend\Filter\Compress\Snappy as SnappyCompression;
use Zend\Filter\Exception;

class SnappyTest extends TestCase
{
    public function setUp()
    {
        if (! extension_loaded('snappy')) {
            $this->markTestSkipped('This adapter needs the snappy extension');
        }
    }

    /**
     * Basic usage
     *
     * @return void
     */
    public function testBasicUsage()
    {
        $filter  = new SnappyCompression();

        $content = $filter->compress('compress me');
        $this->assertNotEquals('compress me', $content);

        $content = $filter->decompress($content);
        $this->assertEquals('compress me', $content);
    }

    /**
     * Snappy should return NULL on invalid arguments.
     *
     * @return void
     */
    public function testNonScalarInput()
    {
        $filter = new SnappyCompression();

        // restore_error_handler can emit an E_WARNING; let's ignore that, as
        // we want to test the returned value
        set_error_handler([$this, 'errorHandler'], E_WARNING);
        $content = $filter->compress([]);
        restore_error_handler();

        $this->assertNull($content);
    }

    /**
     * Snappy should handle empty input data correctly.
     *
     * @return void
     */
    public function testEmptyString()
    {
        $filter = new SnappyCompression();

        $content = $filter->compress(false);
        $content = $filter->decompress($content);
        $this->assertEquals('', $content, 'Snappy failed to decompress empty string.');
    }

    /**
     * Snappy should throw an exception when decompressing invalid data.
     *
     * @return void
     */
    public function testInvalidData()
    {
        $filter = new SnappyCompression();

        $this->expectException(Exception\RuntimeException::class);
        $this->expectExceptionMessage('Error while decompressing.');

        // restore_error_handler can emit an E_WARNING; let's ignore that, as
        // we want to test the returned value
        set_error_handler([$this, 'errorHandler'], E_WARNING);
        $content = $filter->decompress('123');
        restore_error_handler();
    }

    /**
     * testing toString
     *
     * @return void
     */
    public function testSnappyToString()
    {
        $filter = new SnappyCompression();
        $this->assertEquals('Snappy', $filter->toString());
    }

    /**
     * Null error handler; used when wanting to ignore specific error types
     */
    public function errorHandler($errno, $errstr)
    {
    }
}
