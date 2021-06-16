<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */
namespace ZendTest\Filter\File;

use PHPUnit\Framework\TestCase;
use Zend\Filter\Exception;
use Zend\Filter\File\Decrypt as FileDecrypt;
use Zend\Filter\File\Encrypt as FileEncrypt;

class EncryptTest extends TestCase
{
    public $fileToEncrypt;
    public $testDir;
    public $testFile;

    public function setUp()
    {
        if (! extension_loaded('mcrypt') && ! extension_loaded('openssl')) {
            $this->markTestSkipped('This filter needs the mcrypt or openssl extension');
        }

        $this->fileToEncrypt = dirname(__DIR__) . '/_files/encryption.txt';
        $this->testDir = sys_get_temp_dir();
        $this->testFile = sprintf('%s/%s.txt', sys_get_temp_dir(), uniqid('zfilter'));
    }

    public function tearDown()
    {
        if (file_exists($this->testFile)) {
            unlink($this->testFile);
        }
    }

    /**
     * Ensures that the filter follows expected behavior
     *
     * @return void
     */
    public function testBasic()
    {
        $filter = new FileEncrypt();
        $filter->setFilename($this->testFile);

        $this->assertEquals($this->testFile, $filter->getFilename());

        $filter->setKey('1234567890123456');
        $this->assertEquals($this->testFile, $filter->filter($this->fileToEncrypt));

        $this->assertEquals('Encryption', file_get_contents($this->fileToEncrypt));

        $this->assertNotEquals('Encryption', file_get_contents($this->testFile));
    }

    public function testEncryptionWithDecryption()
    {
        $filter = new FileEncrypt();
        $filter->setFilename($this->testFile);
        $filter->setKey('1234567890123456');
        $this->assertEquals($this->testFile, $filter->filter($this->fileToEncrypt));

        $this->assertNotEquals('Encryption', file_get_contents($this->testFile));

        $filter = new FileDecrypt();
        $filter->setKey('1234567890123456');
        $input = $filter->filter($this->testFile);
        $this->assertEquals($this->testFile, $input);

        $this->assertEquals('Encryption', trim(file_get_contents($this->testFile)));
    }

    /**
     *
     * @return void
     */
    public function testNonExistingFile()
    {
        $filter = new FileEncrypt();
        $filter->setKey('1234567890123456');

        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('not found');
        $filter->filter(sprintf('%s/%s.txt', $this->testDir, uniqid()));
    }

    /**
     *
     * @return void
     */
    public function testEncryptionInSameFile()
    {
        $filter = new FileEncrypt();
        $filter->setKey('1234567890123456');

        copy($this->fileToEncrypt, $this->testFile);
        $filter->filter($this->testFile);

        $this->assertNotEquals('Encryption', trim(file_get_contents($this->testFile)));
    }

    public function returnUnfilteredDataProvider()
    {
        return [
            [null],
            [new \stdClass()],
            [[
                sprintf('%s/%s.txt', sys_get_temp_dir(), uniqid()),
                sprintf('%s/%s.txt', sys_get_temp_dir(), uniqid()),
            ]]
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $filter = new FileEncrypt();
        $filter->setKey('1234567890123456');

        $this->assertEquals($input, $filter($input));
    }
}
