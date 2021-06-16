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
use Zend\Filter\Compress\Rar as RarCompression;
use Zend\Filter\Exception;

class RarTest extends TestCase
{
    public $tmp;

    public function setUp()
    {
        if (! extension_loaded('rar')) {
            $this->markTestSkipped('This adapter needs the rar extension');
        }

        $this->tmp = sys_get_temp_dir() . DIRECTORY_SEPARATOR . uniqid('zfilter');
        mkdir($this->tmp);

        $files = [
            $this->tmp . '/zipextracted.txt',
            $this->tmp . '/_compress/Compress/First/Second/zipextracted.txt',
            $this->tmp . '/_compress/Compress/First/Second',
            $this->tmp . '/_compress/Compress/First/zipextracted.txt',
            $this->tmp . '/_compress/Compress/First',
            $this->tmp . '/_compress/Compress/zipextracted.txt',
            $this->tmp . '/_compress/Compress',
            $this->tmp . '/_compress/zipextracted.txt',
            $this->tmp . '/_compress'
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
            $this->tmp . '/_compress'
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
        $filter  = new RarCompression(
            [
                'archive'  => dirname(__DIR__) . '/_files/compressed.rar',
                'target'   => $this->tmp . '/zipextracted.txt',
                'callback' => [__CLASS__, 'rarCompress']
            ]
        );

        $content = $filter->compress('compress me');
        $this->assertEquals(
            dirname(__DIR__) . DIRECTORY_SEPARATOR . '_files' . DIRECTORY_SEPARATOR . 'compressed.rar',
            $content
        );

        $content = $filter->decompress($content);
        $this->assertTrue($content);
        $content = file_get_contents($this->tmp . '/zipextracted.txt');
        $this->assertEquals('compress me', $content);
    }

    /**
     * Setting Options
     *
     * @return void
     */
    public function testRarGetSetOptions()
    {
        $filter = new RarCompression();
        $this->assertEquals(
            [
                'archive'  => null,
                'callback' => null,
                'password' => null,
                'target'   => '.',
            ],
            $filter->getOptions()
        );

        $this->assertEquals(null, $filter->getOptions('archive'));

        $this->assertNull($filter->getOptions('nooption'));
        $filter->setOptions(['nooption' => 'foo']);
        $this->assertNull($filter->getOptions('nooption'));

        $filter->setOptions(['archive' => 'temp.txt']);
        $this->assertEquals('temp.txt', $filter->getOptions('archive'));
    }

    /**
     * Setting Archive
     *
     * @return void
     */
    public function testRarGetSetArchive()
    {
        $filter = new RarCompression();
        $this->assertEquals(null, $filter->getArchive());
        $filter->setArchive('Testfile.txt');
        $this->assertEquals('Testfile.txt', $filter->getArchive());
        $this->assertEquals('Testfile.txt', $filter->getOptions('archive'));
    }

    /**
     * Setting Password
     *
     * @return void
     */
    public function testRarGetSetPassword()
    {
        $filter = new RarCompression();
        $this->assertEquals(null, $filter->getPassword());
        $filter->setPassword('test');
        $this->assertEquals('test', $filter->getPassword());
        $this->assertEquals('test', $filter->getOptions('password'));
        $filter->setOptions(['password' => 'test2']);
        $this->assertEquals('test2', $filter->getPassword());
        $this->assertEquals('test2', $filter->getOptions('password'));
    }

    /**
     * Setting Target
     *
     * @return void
     */
    public function testRarGetSetTarget()
    {
        $filter = new RarCompression();
        $this->assertEquals('.', $filter->getTarget());
        $filter->setTarget('Testfile.txt');
        $this->assertEquals('Testfile.txt', $filter->getTarget());
        $this->assertEquals('Testfile.txt', $filter->getOptions('target'));

        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('does not exist');
        $filter->setTarget('/unknown/path/to/file.txt');
    }

    /**
     * Setting Callback
     *
     * @return void
     */
    public function testSettingCallback()
    {
        $filter = new RarCompression();

        $callback = [__CLASS__, 'rarCompress'];
        $filter->setCallback($callback);
        $this->assertEquals($callback, $filter->getCallback());
    }

    public function testSettingCallbackThrowsExceptionOnMissingCallback()
    {
        $filter = new RarCompression();

        $this->expectException(Exception\RuntimeException::class);
        $this->expectExceptionMessage('No compression callback available');
        $filter->compress('test.txt');
    }

    public function testSettingCallbackThrowsExceptionOnInvalidCallback()
    {
        $filter = new RarCompression();

        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('Invalid callback provided');
        $filter->setCallback('invalidCallback');
    }

    /**
     * Compress to Archive
     *
     * @return void
     */
    public function testRarCompressFile()
    {
        $filter  = new RarCompression(
            [
                'archive'  => dirname(__DIR__) . '/_files/compressed.rar',
                'target'   => $this->tmp . '/zipextracted.txt',
                'callback' => [__CLASS__, 'rarCompress']
            ]
        );
        file_put_contents($this->tmp . '/zipextracted.txt', 'compress me');

        $content = $filter->compress($this->tmp . '/zipextracted.txt');
        $this->assertEquals(
            dirname(__DIR__) . DIRECTORY_SEPARATOR . '_files' . DIRECTORY_SEPARATOR . 'compressed.rar',
            $content
        );

        $content = $filter->decompress($content);
        $this->assertTrue($content);
        $content = file_get_contents($this->tmp . '/zipextracted.txt');
        $this->assertEquals('compress me', $content);
    }

    /**
     * Compress directory to Filename
     *
     * @return void
     */
    public function testRarCompressDirectory()
    {
        $filter  = new RarCompression(
            [
                'archive'  => dirname(__DIR__) . '/_files/compressed.rar',
                'target'   => $this->tmp . '/_compress',
                'callback' => [__CLASS__, 'rarCompress']
            ]
        );
        $content = $filter->compress(dirname(__DIR__) . '/_files/Compress');
        $this->assertEquals(
            dirname(__DIR__) . DIRECTORY_SEPARATOR . '_files' . DIRECTORY_SEPARATOR . 'compressed.rar',
            $content
        );

        mkdir($this->tmp . '/_compress');
        $content = $filter->decompress($content);
        $this->assertTrue($content);

        $base = $this->tmp
            . DIRECTORY_SEPARATOR
            . '_compress'
            . DIRECTORY_SEPARATOR
            . 'Compress'
            . DIRECTORY_SEPARATOR;
        $this->assertFileExists($base);
        $this->assertFileExists($base . 'zipextracted.txt');
        $this->assertFileExists($base . 'First' . DIRECTORY_SEPARATOR . 'zipextracted.txt');
        $this->assertFileExists(
            $base . 'First' . DIRECTORY_SEPARATOR .  'Second' . DIRECTORY_SEPARATOR . 'zipextracted.txt'
        );
    }

    /**
     * testing toString
     *
     * @return void
     */
    public function testRarToString()
    {
        $filter = new RarCompression();
        $this->assertEquals('Rar', $filter->toString());
    }

    /**
     * Test callback for compression
     *
     * @return unknown
     */
    public static function rarCompress()
    {
        return true;
    }
}
