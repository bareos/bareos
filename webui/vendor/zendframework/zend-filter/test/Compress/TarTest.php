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
use Zend\Filter\Compress\Tar as TarCompression;
use Zend\Filter\Exception;

class TarTest extends TestCase
{
    public $tmp;

    public function setUp()
    {
        $this->tmp = sprintf('%s/%s', sys_get_temp_dir(), uniqid('zfilter'));
        mkdir($this->tmp, 0775, true);
    }

    public function tearDown()
    {
        $files = [
            $this->tmp . '/zipextracted.txt',
            $this->tmp . '/_compress/Compress/First/Second/zipextracted.txt',
            $this->tmp . '/_compress/Compress/First/Second',
            $this->tmp . '/_compress/Compress/First/zipextracted.txt',
            $this->tmp . '/_compress/Compress/First',
            $this->tmp . '/_compress/Compress/zipextracted.txt',
            $this->tmp . '/_compress/Compress',
            $this->tmp . '/_compress/zipextracted.txt',
            $this->tmp . '/_compress',
            $this->tmp . '/compressed.tar',
            $this->tmp . '/compressed.tar.gz',
            $this->tmp . '/compressed.tar.bz2'
        ];

        foreach ($files as $file) {
            if (file_exists($file)) {
                if (is_dir($file)) {
                    rmdir($file);
                } else {
                    unlink($file);
                }
            }
        }
    }

    /**
     * Basic usage
     *
     * @return void
     */
    public function testBasicUsage()
    {
        $filter  = new TarCompression(
            [
                'archive'  => $this->tmp . '/compressed.tar',
                'target'   => $this->tmp . '/zipextracted.txt'
            ]
        );

        $content = $filter->compress('compress me');
        $this->assertEquals(
            $this->tmp . DIRECTORY_SEPARATOR . 'compressed.tar',
            $content
        );

        $content = $filter->decompress($content);
        $this->assertEquals($this->tmp  . DIRECTORY_SEPARATOR, $content);
        $content = file_get_contents($this->tmp . '/zipextracted.txt');
        $this->assertEquals('compress me', $content);
    }

    /**
     * Setting Options
     *
     * @return void
     */
    public function testTarGetSetOptions()
    {
        $filter = new TarCompression();
        $this->assertEquals(
            [
                'archive' => null,
                'target'  => '.',
                'mode'    => null],
            $filter->getOptions()
        );

        $this->assertEquals(null, $filter->getOptions('archive'));

        $this->assertNull($filter->getOptions('nooption'));
        $filter->setOptions(['nooptions' => 'foo']);
        $this->assertNull($filter->getOptions('nooption'));

        $filter->setOptions(['archive' => 'temp.txt']);
        $this->assertEquals('temp.txt', $filter->getOptions('archive'));
    }

    /**
     * Setting Archive
     *
     * @return void
     */
    public function testTarGetSetArchive()
    {
        $filter = new TarCompression();
        $this->assertEquals(null, $filter->getArchive());
        $filter->setArchive('Testfile.txt');
        $this->assertEquals('Testfile.txt', $filter->getArchive());
        $this->assertEquals('Testfile.txt', $filter->getOptions('archive'));
    }

    /**
     * Setting Target
     *
     * @return void
     */
    public function testTarGetSetTarget()
    {
        $filter = new TarCompression();
        $this->assertEquals('.', $filter->getTarget());
        $filter->setTarget('Testfile.txt');
        $this->assertEquals('Testfile.txt', $filter->getTarget());
        $this->assertEquals('Testfile.txt', $filter->getOptions('target'));

        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('does not exist');
        $filter->setTarget('/unknown/path/to/file.txt');
    }

    /**
     * Setting Archive
     *
     * @return void
     */
    public function testTarCompressToFile()
    {
        $filter  = new TarCompression(
            [
                'archive'  => $this->tmp . '/compressed.tar',
                'target'   => $this->tmp . '/zipextracted.txt'
            ]
        );
        file_put_contents($this->tmp . '/zipextracted.txt', 'compress me');

        $content = $filter->compress($this->tmp . '/zipextracted.txt');
        $this->assertEquals(
            $this->tmp . DIRECTORY_SEPARATOR . 'compressed.tar',
            $content
        );

        $content = $filter->decompress($content);
        $this->assertEquals($this->tmp . DIRECTORY_SEPARATOR, $content);
        $content = file_get_contents($this->tmp . '/zipextracted.txt');
        $this->assertEquals('compress me', $content);
    }

    /**
     * Compress directory to Filename
     *
     * @return void
     */
    public function testTarCompressDirectory()
    {
        $filter  = new TarCompression(
            [
                'archive'  => $this->tmp . '/compressed.tar',
                'target'   => $this->tmp . '/_compress'
            ]
        );
        $content = $filter->compress(dirname(__DIR__) . '/_files/Compress');
        $this->assertEquals(
            $this->tmp . DIRECTORY_SEPARATOR . 'compressed.tar',
            $content
        );
    }

    public function testSetModeShouldWorkWithCaseInsensitive()
    {
        $filter = new TarCompression;
        $filter->setTarget($this->tmp . '/zipextracted.txt');

        foreach (['GZ', 'Bz2'] as $mode) {
            $archive = implode(DIRECTORY_SEPARATOR, [
                $this->tmp,
                'compressed.tar.',
            ]) . strtolower($mode);
            $filter->setArchive($archive);
            $filter->setMode($mode);
            $content = $filter->compress('compress me');
            $this->assertEquals($archive, $content);
        }
    }

    /**
     * testing toString
     *
     * @return void
     */
    public function testTarToString()
    {
        $filter = new TarCompression();
        $this->assertEquals('Tar', $filter->toString());
    }

    /**
     * @see https://github.com/zendframework/zend-filter/issues/41
     */
    public function testDecompressionDoesNotRequireArchive()
    {
        $filter = new TarCompression([
            'archive' => $this->tmp . '/compressed.tar',
            'target' => $this->tmp . '/zipextracted.txt',
        ]);

        $content = 'compress me ' . microtime(true);
        $compressed = $filter->compress($content);

        self::assertSame($this->tmp . DIRECTORY_SEPARATOR . 'compressed.tar', $compressed);

        $target = $this->tmp;
        $filter = new TarCompression([
            'target' => $target,
        ]);

        $decompressed = $filter->decompress($compressed);
        self::assertSame($target, $decompressed);
        // per documentation, tar includes full path
        $file = $target . DIRECTORY_SEPARATOR . $target . DIRECTORY_SEPARATOR . '/zipextracted.txt';
        self::assertFileExists($file);
        self::assertSame($content, file_get_contents($file));
    }
}
