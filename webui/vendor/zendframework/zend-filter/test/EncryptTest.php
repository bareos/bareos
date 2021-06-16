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
use Zend\Filter\Encrypt as EncryptFilter;
use Zend\Filter\Exception;

class EncryptTest extends TestCase
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
        $filter = new EncryptFilter(['adapter' => 'BlockCipher', 'key' => 'testkey']);
        $valuesExpected = [
            'STRING' => 'STRING',
            'ABC1@3' => 'ABC1@3',
            'A b C'  => 'A B C',
            1        => 1,
            -1       => -1,
            1.0      => 1.0,
            -1.0     => -1.0,
        ];

        $enc = $filter->getEncryption();
        $filter->setVector('1234567890123456');
        $this->assertEquals('testkey', $enc['key']);
        foreach ($valuesExpected as $input => $output) {
            $this->assertNotEquals($output, $filter($input));
        }
    }

    /**
     * Ensures that the encryption works fine
     */
    public function testEncryptBlockCipher()
    {
        $encrypt = new EncryptFilter(['adapter' => 'BlockCipher', 'key' => 'testkey']);
        $encrypt->setVector('1234567890123456890');
        $encrypted = $encrypt->filter('test');
        // @codingStandardsIgnoreStart
        $this->assertEquals($encrypted, 'ec133eb7460682b0020b736ad6d2ef14c35de0f1e5976330ae1dd096ef3b4cb7MTIzNDU2Nzg5MDEyMzQ1NoZvxY1JkeL6TnQP3ug5F0k=');
        // @codingStandardsIgnoreEnd
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

        $filter = new EncryptFilter(['adapter' => 'Openssl']);
        $valuesExpected = [
            'STRING' => 'STRING',
            'ABC1@3' => 'ABC1@3',
            'A b C'  => 'A B C'
        ];

        $filter->setPublicKey(__DIR__ . '/_files/publickey.pem');
        $key = $filter->getPublicKey();
        $this->assertEquals(
            [__DIR__ . '/_files/publickey.pem' =>
                  '-----BEGIN CERTIFICATE-----
MIIC3jCCAkegAwIBAgIBADANBgkqhkiG9w0BAQQFADCBtDELMAkGA1UEBhMCTkwx
FjAUBgNVBAgTDU5vb3JkLUhvbGxhbmQxEDAOBgNVBAcTB1phYW5kYW0xFzAVBgNV
BAoTDk1vYmlsZWZpc2guY29tMR8wHQYDVQQLExZDZXJ0aWZpY2F0aW9uIFNlcnZp
Y2VzMRowGAYDVQQDExFNb2JpbGVmaXNoLmNvbSBDQTElMCMGCSqGSIb3DQEJARYW
Y29udGFjdEBtb2JpbGVmaXNoLmNvbTAeFw0wNzA2MDcxNzM1NTNaFw0wODA2MDYx
NzM1NTNaMIG0MQswCQYDVQQGEwJOTDEWMBQGA1UECBMNTm9vcmQtSG9sbGFuZDEQ
MA4GA1UEBxMHWmFhbmRhbTEXMBUGA1UEChMOTW9iaWxlZmlzaC5jb20xHzAdBgNV
BAsTFkNlcnRpZmljYXRpb24gU2VydmljZXMxGjAYBgNVBAMTEU1vYmlsZWZpc2gu
Y29tIENBMSUwIwYJKoZIhvcNAQkBFhZjb250YWN0QG1vYmlsZWZpc2guY29tMIGf
MA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDKTIp7FntJt1BioBZ0lmWBE8Cyznge
GCHNMcAC4JLbi1Y0LwT4CSaQarbvAqBRmc+joHX+rcURm89wOibRaThrrZcvgl2p
omzu7shJc0ObiRZC8H7pxTkZ1HHjN8cRSQlOHkcdtE9yoiSGSO+zZ9K5ReU1DOsF
FDD4V7XpcNU63QIDAQABMA0GCSqGSIb3DQEBBAUAA4GBAFQ22OU/PAN7rRDr23NS
2XkpSngwZWeHoFW1D2gRvHHRlqg5Q8KZHQAALd5PEFakehdn03NG6yEdnhXpqKT/
5jYy6v3b+zwEvY82EUieMldovdnpsS1EScjjvPfQ1lSgcTHT2QX5MjNv13xLnOgh
PIDs9E7uuizAKDhRRRvho8BS
-----END CERTIFICATE-----
'],
            $key
        );
        foreach ($valuesExpected as $input => $output) {
            $this->assertNotEquals($output, $filter($input));
        }
    }

    /**
     * @return void
     */
    public function testSettingAdapterManually()
    {
        if (! extension_loaded('openssl')) {
            $this->markTestSkipped('Openssl extension not installed');
        }

        $filter = new EncryptFilter();
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
        $filter = new EncryptFilter();
        $filter->getUnknownMethod();
    }

    public function returnUnfilteredDataProvider()
    {
        return [
            [null],
            [new \stdClass()],
            [[
                'encrypt me',
                'encrypt me too, please'
            ]]
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $encrypt = new EncryptFilter(['adapter' => 'BlockCipher', 'key' => 'testkey']);
        $encrypt->setVector('1234567890123456890');

        $encrypted = $encrypt->filter($input);
        $this->assertEquals($input, $encrypted);
    }
}
