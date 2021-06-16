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
use Zend\Filter\Compress as CompressFilter;
use Zend\Filter\Exception;

class CompressTest extends TestCase
{
    public $tmpDir;

    public function setUp()
    {
        if (! extension_loaded('bz2')) {
            $this->markTestSkipped('This filter is tested with the bz2 extension');
        }

        $this->tmpDir = sprintf('%s/%s', sys_get_temp_dir(), uniqid('zfilter'));
        mkdir($this->tmpDir, 0775, true);
    }

    public function tearDown()
    {
        if (is_dir($this->tmpDir)) {
            if (file_exists($this->tmpDir . '/compressed.bz2')) {
                unlink($this->tmpDir . '/compressed.bz2');
            }
            rmdir($this->tmpDir);
        }
    }

    /**
     * Basic usage
     *
     * @return void
     */
    public function testBasicUsage()
    {
        $filter  = new CompressFilter('bz2');

        $text     = 'compress me';
        $compressed = $filter($text);
        $this->assertNotEquals($text, $compressed);

        $decompressed = $filter->decompress($compressed);
        $this->assertEquals($text, $decompressed);
    }

    /**
     * Setting Options
     *
     * @return void
     */
    public function testGetSetAdapterOptionsInConstructor()
    {
        $filter = new CompressFilter([
            'adapter' => 'bz2',
            'options' => [
                'blocksize' => 6,
                'archive'   => 'test.txt',
            ]
        ]);

        $this->assertEquals(
            ['blocksize' => 6, 'archive' => 'test.txt'],
            $filter->getAdapterOptions()
        );

        $adapter = $filter->getAdapter();
        $this->assertEquals(6, $adapter->getBlocksize());
        $this->assertEquals('test.txt', $adapter->getArchive());
    }

    /**
     * Setting Options through constructor
     *
     * @return void
     */
    public function testGetSetAdapterOptions()
    {
        $filter = new CompressFilter('bz2');
        $filter->setAdapterOptions([
            'blocksize' => 6,
            'archive'   => 'test.txt',
        ]);
        $this->assertEquals(
            ['blocksize' => 6, 'archive'   => 'test.txt'],
            $filter->getAdapterOptions()
        );
        $adapter = $filter->getAdapter();
        $this->assertEquals(6, $adapter->getBlocksize());
        $this->assertEquals('test.txt', $adapter->getArchive());
    }

    /**
     * Setting Blocksize
     *
     * @return void
     */
    public function testGetSetBlocksize()
    {
        $filter = new CompressFilter('bz2');
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
    public function testGetSetArchive()
    {
        $filter = new CompressFilter('bz2');
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
    public function testCompressToFile()
    {
        $filter   = new CompressFilter('bz2');
        $archive = $this->tmpDir . '/compressed.bz2';
        $filter->setArchive($archive);

        $content = $filter('compress me');
        $this->assertTrue($content);

        $filter2  = new CompressFilter('bz2');
        $content2 = $filter2->decompress($archive);
        $this->assertEquals('compress me', $content2);

        $filter3 = new CompressFilter('bz2');
        $filter3->setArchive($archive);
        $content3 = $filter3->decompress(null);
        $this->assertEquals('compress me', $content3);
    }

    /**
     * testing toString
     *
     * @return void
     */
    public function testToString()
    {
        $filter = new CompressFilter('bz2');
        $this->assertEquals('Bz2', $filter->toString());
    }

    /**
     * testing getAdapter
     *
     * @return void
     */
    public function testGetAdapter()
    {
        $filter = new CompressFilter('bz2');
        $adapter = $filter->getAdapter();
        $this->assertInstanceOf('Zend\Filter\Compress\CompressionAlgorithmInterface', $adapter);
        $this->assertEquals('Bz2', $filter->getAdapterName());
    }

    /**
     * Setting Adapter
     *
     * @return void
     */
    public function testSetAdapter()
    {
        if (! extension_loaded('zlib')) {
            $this->markTestSkipped('This filter is tested with the zlib extension');
        }

        $filter = new CompressFilter();
        $this->assertEquals('Gz', $filter->getAdapterName());


        $filter->setAdapter('\Zend\Filter\Boolean');

        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('does not implement');
        $adapter = $filter->getAdapter();
    }

    /**
     * Decompress archiv
     *
     * @return void
     */
    public function testDecompressArchive()
    {
        $filter   = new CompressFilter('bz2');
        $archive = $this->tmpDir . '/compressed.bz2';
        $filter->setArchive($archive);

        $content = $filter('compress me');
        $this->assertTrue($content);

        $filter2  = new CompressFilter('bz2');
        $content2 = $filter2->decompress($archive);
        $this->assertEquals('compress me', $content2);
    }

    /**
     * Setting invalid method
     *
     * @return void
     */
    public function testInvalidMethod()
    {
        $filter = new CompressFilter();

        $this->expectException(Exception\BadMethodCallException::class);
        $this->expectExceptionMessage('Unknown method');
        $filter->invalidMethod();
    }

    public function returnUnfilteredDataProvider()
    {
        return [
            [null],
            [new \stdClass()],
            [[
                'compress me',
                'compress me too, please'
            ]]
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $filter  = new CompressFilter('bz2');

        $this->assertEquals($input, $filter($input));
    }
}
