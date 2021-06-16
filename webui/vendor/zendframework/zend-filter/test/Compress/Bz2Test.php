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
use Zend\Filter\Compress\Bz2 as Bz2Compression;
use Zend\Filter\Exception;

class Bz2Test extends TestCase
{
    public $target;

    public function setUp()
    {
        if (! extension_loaded('bz2')) {
            $this->markTestSkipped('This adapter needs the bz2 extension');
        }

        $this->target = sprintf('%s/%s.bz2', sys_get_temp_dir(), uniqid('zfilter'));
    }

    public function tearDown()
    {
        if (file_exists($this->target)) {
            unlink($this->target);
        }
    }

    /**
     * Basic usage
     *
     * @return void
     */
    public function testBasicUsage()
    {
        $filter  = new Bz2Compression();

        $content = $filter->compress('compress me');
        $this->assertNotEquals('compress me', $content);

        $content = $filter->decompress($content);
        $this->assertEquals('compress me', $content);
    }

    /**
     * Setting Options
     *
     * @return void
     */
    public function testBz2GetSetOptions()
    {
        $filter = new Bz2Compression();
        $this->assertEquals(['blocksize' => 4, 'archive' => null], $filter->getOptions());

        $this->assertEquals(4, $filter->getOptions('blocksize'));

        $this->assertNull($filter->getOptions('nooption'));

        $filter->setOptions(['blocksize' => 6]);
        $this->assertEquals(6, $filter->getOptions('blocksize'));

        $filter->setOptions(['archive' => 'test.txt']);
        $this->assertEquals('test.txt', $filter->getOptions('archive'));

        $filter->setOptions(['nooption' => 0]);
        $this->assertNull($filter->getOptions('nooption'));
    }

    /**
     * Setting Options through constructor
     *
     * @return void
     */
    public function testBz2GetSetOptionsInConstructor()
    {
        $filter2 = new Bz2Compression(['blocksize' => 8]);
        $this->assertEquals(['blocksize' => 8, 'archive' => null], $filter2->getOptions());
    }

    /**
     * Setting Blocksize
     *
     * @return void
     */
    public function testBz2GetSetBlocksize()
    {
        $filter = new Bz2Compression();
        $this->assertEquals(4, $filter->getBlocksize());
        $filter->setBlocksize(6);
        $this->assertEquals(6, $filter->getOptions('blocksize'));

        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('must be between');
        $filter->setBlocksize(15);
    }

    /**
     * Setting Archive
     *
     * @return void
     */
    public function testBz2GetSetArchive()
    {
        $filter = new Bz2Compression();
        $this->assertEquals(null, $filter->getArchive());
        $filter->setArchive('Testfile.txt');
        $this->assertEquals('Testfile.txt', $filter->getArchive());
        $this->assertEquals('Testfile.txt', $filter->getOptions('archive'));
    }

    /**
     * Setting Archive
     *
     * @return void
     */
    public function testBz2CompressToFile()
    {
        $filter   = new Bz2Compression();
        $archive = $this->target;
        $filter->setArchive($archive);

        $content = $filter->compress('compress me');
        $this->assertTrue($content);

        $filter2  = new Bz2Compression();
        $content2 = $filter2->decompress($archive);
        $this->assertEquals('compress me', $content2);

        $filter3 = new Bz2Compression();
        $filter3->setArchive($archive);
        $content3 = $filter3->decompress(null);
        $this->assertEquals('compress me', $content3);
    }

    /**
     * testing toString
     *
     * @return void
     */
    public function testBz2ToString()
    {
        $filter = new Bz2Compression();
        $this->assertEquals('Bz2', $filter->toString());
    }

    /**
     * Basic usage
     *
     * @return void
     */
    public function testBz2DecompressArchive()
    {
        $filter   = new Bz2Compression();
        $archive = $this->target;
        $filter->setArchive($archive);

        $content = $filter->compress('compress me');
        $this->assertTrue($content);

        $filter2  = new Bz2Compression();
        $content2 = $filter2->decompress($archive);
        $this->assertEquals('compress me', $content2);
    }
}
