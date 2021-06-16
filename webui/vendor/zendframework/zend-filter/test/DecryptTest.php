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
use Zend\Filter\Decrypt as DecryptFilter;
use Zend\Filter\Exception;

class DecryptTest extends TestCase
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
    public function testBasicMcrypt()
    {
        $filter = new DecryptFilter(['adapter' => 'BlockCipher']);
        $valuesExpected = [
            'STRING' => 'STRING',
            'ABC1@3' => 'ABC1@3',
            'A b C'  => 'A B C'
        ];

        $enc = $filter->getEncryption();
        $filter->setKey('1234567890123456');
        foreach ($valuesExpected as $input => $output) {
            $this->assertNotEquals($output, $filter($input));
        }
    }

    /**
     * Ensures that the encryption works fine
     */
    public function testDecryptBlockCipher()
    {
        $decrypt = new DecryptFilter(['adapter' => 'BlockCipher', 'key' => 'testkey']);
        $decrypt->setVector('1234567890123456890');
        // @codingStandardsIgnoreStart
        $decrypted = $decrypt->filter('ec133eb7460682b0020b736ad6d2ef14c35de0f1e5976330ae1dd096ef3b4cb7MTIzNDU2Nzg5MDEyMzQ1NoZvxY1JkeL6TnQP3ug5F0k=');
        // @codingStandardsIgnoreEnd
        $this->assertEquals($decrypted, 'test');
    }

    /**
     * Ensures that the filter follows expected behavior
     *
     * @return void
     */
    public function testBasicOpenssl()
    {
        if (! extension_loaded('openssl')) {
            $this->markTestSkipped('Openssl extension not installed');
        }

        $filter = new DecryptFilter(['adapter' => 'Openssl']);
        $filter->setPassphrase('zPUp9mCzIrM7xQOEnPJZiDkBwPBV9UlITY0Xd3v4bfIwzJ12yPQCAkcR5BsePGVw
RK6GS5RwXSLrJu9Qj8+fk0wPj6IPY5HvA9Dgwh+dptPlXppeBm3JZJ+92l0DqR2M
ccL43V3Z4JN9OXRAfGWXyrBJNmwURkq7a2EyFElBBWK03OLYVMevQyRJcMKY0ai+
tmnFUSkH2zwnkXQfPUxg9aV7TmGQv/3TkK1SziyDyNm7GwtyIlfcigCCRz3uc77U
Izcez5wgmkpNElg/D7/VCd9E+grTfPYNmuTVccGOes+n8ISJJdW0vYX1xwWv5l
bK22CwD/l7SMBOz4M9XH0Jb0OhNxLza4XMDu0ANMIpnkn1KOcmQ4gB8fmAbBt');
        $filter->setPrivateKey(__DIR__ . '/_files/privatekey.pem');

        $key = $filter->getPrivateKey();
        $this->assertEquals(
            [__DIR__ . '/_files/privatekey.pem' =>
                  '-----BEGIN RSA PRIVATE KEY-----
MIICXgIBAAKBgQDKTIp7FntJt1BioBZ0lmWBE8CyzngeGCHNMcAC4JLbi1Y0LwT4
CSaQarbvAqBRmc+joHX+rcURm89wOibRaThrrZcvgl2pomzu7shJc0ObiRZC8H7p
xTkZ1HHjN8cRSQlOHkcdtE9yoiSGSO+zZ9K5ReU1DOsFFDD4V7XpcNU63QIDAQAB
AoGBALr0XY4/SpTnmpxqwhXg39GYBZ+5e/yj5KkTbxW5oT7P2EzFn1vyaPdSB9l+
ndaLxP68zg8dXGBXlC9tLm6dRQtocGupUPB1HOEQbUIlQdiKF/W7/8w6uzLNXdid
qCSLrSJ4cfkYKtS29Xi6qooRw2DOvUFngXy/ELtmTeiBcihpAkEA8+oUesTET+TO
IYM0+l5JrTOpCPZt+aY4JPmWoKz9bshJT/DP2KPgmqd8/Vy+i23yIfOwUxbpwbna
aKzNPi/uywJBANRSl7RNL7jh1BJRQC7+mvUVTE8iQwbyGtIipcLC7bxwhNQzuPKS
P4o/a1+HEVB9Nv1Em7DqKTwBnlkJvaFZ3/cCQQCcvx0SGEkgHqXpG2x8SQOH7t7+
B399I7iI6mxGLWVgQA389YBcdFPujxvfpi49ZBZqgzQY8WyfNlSJWCM9h4gpAkAu
qxzHN7QGmjSn9g36hmH+/rhwKGK9MxfsGkt+/KOOqNi5X8kGIFkxBPGP5LtMisk8
cAkcoMuBcgWhIn/46C1PAkEAzLK/ibrdMQLOdO4SuDgj/2nc53NZ3agl61ew8Os6
d/fxzPfuO/bLpADozTAnYT9Hu3wPrQVLeAfCp0ojqH7DYg==
-----END RSA PRIVATE KEY-----
'],
            $key
        );
    }

    /**
     * @return void
     */
    public function testSettingAdapterManually()
    {
        if (! extension_loaded('openssl')) {
            $this->markTestSkipped('Openssl extension not installed');
        }

        $filter = new DecryptFilter();
        $filter->setAdapter('Openssl');
        $this->assertEquals('Openssl', $filter->getAdapter());
        $this->assertInstanceOf('Zend\Filter\Encrypt\EncryptionAlgorithmInterface', $filter->getAdapterInstance());

        $filter->setAdapter('BlockCipher');
        $this->assertEquals('BlockCipher', $filter->getAdapter());
        $this->assertInstanceOf('Zend\Filter\Encrypt\EncryptionAlgorithmInterface', $filter->getAdapterInstance());

        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('does not implement');
        $filter->setAdapter('\stdClass');
    }

    /**
     * @return void
     */
    public function testCallingUnknownMethod()
    {
        $this->expectException(Exception\BadMethodCallException::class);
        $this->expectExceptionMessage('Unknown method');
        $filter = new DecryptFilter();
        $filter->getUnknownMethod();
    }

    public function returnUnfilteredDataProvider()
    {
        return [
            [null],
            [new \stdClass()],
            // @codingStandardsIgnoreStart
            [[
                'ec133eb7460682b0020b736ad6d2ef14c35de0f1e5976330ae1dd096ef3b4cb7MTIzNDU2Nzg5MDEyMzQ1NoZvxY1JkeL6TnQP3ug5F0k=',
                'decrypt me too, please'
            ]]
            // @codingStandardsIgnoreEnd
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $decrypt = new DecryptFilter(['adapter' => 'BlockCipher', 'key' => 'testkey']);
        $decrypt->setVector('1234567890123456890');

        $decrypted = $decrypt->filter($input);
        $this->assertEquals($input, $decrypted);
    }
}
