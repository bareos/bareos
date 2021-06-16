<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Filter\Encrypt;

use PHPUnit\Framework\TestCase;
use Zend\Filter\Encrypt\BlockCipher as BlockCipherEncryption;
use Zend\Filter\Exception;

class BlockCipherTest extends TestCase
{
    public function setUp()
    {
        if (! extension_loaded('mcrypt') && ! extension_loaded('openssl')) {
            $this->markTestSkipped('This filter needs the mcrypt or openssl extension');
        }
    }

    /**
     * Ensures that the filter follows expected behavior
     *
     * @return void
     */
    public function testBasicBlockCipher()
    {
        $filter = new BlockCipherEncryption(['key' => 'testkey']);
        // @codingStandardsIgnoreStart
        $valuesExpected = [
            'STRING' => '5b68e3648f9136e5e9bfaa2242e5b668e7501b2c20e8f9e2c76638f017f62a8eWmVuZEZyYW1ld29yazIuMDpd5vWydswa0fyIo2dnF0Q=',
            'ABC1@3' => 'c7da11b89330f6bbbb15fcb6de574c7ec869ad7187a7d466e60f2437914d927aWmVuZEZyYW1ld29yazIuMKXsBdYXBLQx9elx0B20uxQ=',
            'A b C' => 'ca1b9df732facf9dfadc7c3fdf1ccdc211bf21f638d459f43fefc74bbc9c8e01WmVuZEZyYW1ld29yazIuMM1som/As52rdK/4g7uoYx4='
        ];
        // @codingStandardsIgnoreEnd
        $filter->setVector('ZendFramework2.0');
        $enc = $filter->getEncryption();
        $this->assertEquals('testkey', $enc['key']);
        foreach ($valuesExpected as $input => $output) {
            $this->assertEquals($output, $filter->encrypt($input));
        }
    }

    /**
     * Ensures that the vector can be set / returned
     *
     * @return void
     */
    public function testGetSetVector()
    {
        $filter = new BlockCipherEncryption(['key' => 'testkey']);
        $filter->setVector('1234567890123456');
        $this->assertEquals('1234567890123456', $filter->getVector());
    }

    public function testWrongSizeVector()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $filter = new BlockCipherEncryption(['key' => 'testkey']);
        $filter->setVector('testvect');
    }
    /**
     * Ensures that the filter allows default encryption
     *
     * @return void
     */
    public function testDefaultEncryption()
    {
        $filter = new BlockCipherEncryption(['key' => 'testkey']);
        $filter->setVector('1234567890123456');
        $this->assertEquals(
            ['key'           => 'testkey',
                  'algorithm'     => 'aes',
                  'vector'        => '1234567890123456',
                  'key_iteration' => 5000,
                  'hash'          => 'sha256'],
            $filter->getEncryption()
        );
    }

    /**
     * Ensures that the filter allows setting options de/encryption
     *
     * @return void
     */
    public function testGetSetEncryption()
    {
        $filter = new BlockCipherEncryption(['key' => 'testkey']);
        $filter->setVector('1234567890123456');
        $filter->setEncryption(
            ['algorithm' => 'blowfish']
        );
        $this->assertEquals(
            ['key'           => 'testkey',
                  'algorithm'     => 'blowfish',
                  'vector'        => '1234567890123456',
                  'key_iteration' => 5000,
                  'hash'          => 'sha256'],
            $filter->getEncryption()
        );
    }

    /**
     * Ensures that the filter allows de/encryption
     *
     * @return void
     */
    public function testEncryptionWithDecryption()
    {
        $filter = new BlockCipherEncryption(['key' => 'testkey']);
        $filter->setVector('1234567890123456');
        $output = $filter->encrypt('teststring');

        $this->assertNotEquals('teststring', $output);

        $input = $filter->decrypt($output);
        $this->assertEquals('teststring', trim($input));
    }

    /**
     * @return void
     */
    public function testConstructionWithStringKey()
    {
        $filter = new BlockCipherEncryption('testkey');
        $data = $filter->getEncryption();
        $this->assertEquals('testkey', $data['key']);
    }

    /**
     * @return void
     */
    public function testConstructionWithInteger()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('Invalid options argument');
        $filter = new BlockCipherEncryption(1234);
    }

    /**
     * @return void
     */
    public function testToString()
    {
        $filter = new BlockCipherEncryption('testkey');
        $this->assertEquals('BlockCipher', $filter->toString());
    }

    /**
     * @return void
     */
    public function testSettingEncryptionOptions()
    {
        $filter = new BlockCipherEncryption('testkey');
        $filter->setEncryption('newkey');
        $test = $filter->getEncryption();
        $this->assertEquals('newkey', $test['key']);

        try {
            $filter->setEncryption(1234);
            $filter->fail();
        } catch (\Zend\Filter\Exception\InvalidArgumentException $e) {
            $this->assertContains('Invalid options argument', $e->getMessage());
        }

        try {
            $filter->setEncryption(['algorithm' => 'unknown']);
            $filter->fail();
        } catch (\Zend\Filter\Exception\InvalidArgumentException $e) {
            $this->assertContains('The algorithm', $e->getMessage());
        }

        try {
            $filter->setEncryption(['mode' => 'unknown']);
        } catch (\Zend\Filter\Exception\InvalidArgumentException $e) {
            $this->assertContains('The mode', $e->getMessage());
        }
    }

    /**
     * @return void
     */
    public function testSettingEmptyVector()
    {
        $filter = new BlockCipherEncryption('newkey');
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('The salt (IV) cannot be empty');
        $filter->setVector('');
    }

    /**
     * Ensures that the filter allows de/encryption with compression
     *
     * @return void
     */
    public function testEncryptionWithDecryptionAndCompression()
    {
        if (! extension_loaded('bz2')) {
            $this->markTestSkipped('This adapter needs the bz2 extension');
        }

        $filter = new BlockCipherEncryption(['key' => 'testkey']);
        $filter->setVector('1234567890123456');
        $filter->setCompression('bz2');
        $output = $filter->encrypt('teststring');

        $this->assertNotEquals('teststring', $output);

        $input = $filter->decrypt($output);
        $this->assertEquals('teststring', trim($input));
    }
}
