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
use Zend\Filter\Compress\Lzf as LzfCompression;

class LzfTest extends TestCase
{
    public function setUp()
    {
        if (! extension_loaded('lzf')) {
            $this->markTestSkipped('This adapter needs the lzf extension');
        }
    }

    /**
     * Basic usage
     *
     * @return void
     */
    public function testBasicUsage()
    {
        $filter  = new LzfCompression();

        $text       = 'compress me';
        $compressed = $filter->compress($text);
        $this->assertNotEquals($text, $compressed);

        $decompressed = $filter->decompress($compressed);
        $this->assertEquals($text, $decompressed);
    }

    /**
     * testing toString
     *
     * @return void
     */
    public function testLzfToString()
    {
        $filter = new LzfCompression();
        $this->assertEquals('Lzf', $filter->toString());
    }
}
