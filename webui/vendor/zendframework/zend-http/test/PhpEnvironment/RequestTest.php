<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\PhpEnvironment;

use PHPUnit\Framework\TestCase;
use Zend\Http\Exception\InvalidArgumentException;
use Zend\Http\Header\GenericHeader;
use Zend\Http\Headers;
use Zend\Http\PhpEnvironment\Request;
use Zend\Stdlib\Parameters;

class RequestTest extends TestCase
{
    /**
     * Original environemnt
     *
     * @var array
     */
    protected $originalEnvironment;

    /**
     * Save the original environment and set up a clean one.
     */
    public function setUp()
    {
        $this->originalEnvironment = [
            'post'   => $_POST,
            'get'    => $_GET,
            'cookie' => $_COOKIE,
            'server' => $_SERVER,
            'env'    => $_ENV,
            'files'  => $_FILES,
        ];

        $_POST   = [];
        $_GET    = [];
        $_COOKIE = [];
        $_SERVER = [];
        $_ENV    = [];
        $_FILES  = [];
    }

    /**
     * Restore the original environment
     */
    public function tearDown()
    {
        $_POST   = $this->originalEnvironment['post'];
        $_GET    = $this->originalEnvironment['get'];
        $_COOKIE = $this->originalEnvironment['cookie'];
        $_SERVER = $this->originalEnvironment['server'];
        $_ENV    = $this->originalEnvironment['env'];
        $_FILES  = $this->originalEnvironment['files'];
    }

    /**
     * Data provider for testing base URL and path detection.
     */
    public static function baseUrlAndPathProvider()
    {
        return [
            [
                [
                    'REQUEST_URI'     => '/index.php/news/3?var1=val1&var2=val2',
                    'QUERY_URI'       => 'var1=val1&var2=val2',
                    'SCRIPT_NAME'     => '/index.php',
                    'PHP_SELF'        => '/index.php/news/3',
                    'SCRIPT_FILENAME' => '/var/web/html/index.php',
                ],
                '/index.php',
                '',
            ],
            [
                [
                    'REQUEST_URI'     => '/public/index.php/news/3?var1=val1&var2=val2',
                    'QUERY_URI'       => 'var1=val1&var2=val2',
                    'SCRIPT_NAME'     => '/public/index.php',
                    'PHP_SELF'        => '/public/index.php/news/3',
                    'SCRIPT_FILENAME' => '/var/web/html/public/index.php',
                ],
                '/public/index.php',
                '/public',
            ],
            [
                [
                    'REQUEST_URI'     => '/index.php/news/3?var1=val1&var2=val2',
                    'SCRIPT_NAME'     => '/home.php',
                    'PHP_SELF'        => '/index.php/news/3',
                    'SCRIPT_FILENAME' => '/var/web/html/index.php',
                ],
                '/index.php',
                '',
            ],
            [
                [
                    'REQUEST_URI'      => '/index.php/news/3?var1=val1&var2=val2',
                    'SCRIPT_NAME'      => '/home.php',
                    'PHP_SELF'         => '/home.php',
                    'ORIG_SCRIPT_NAME' => '/index.php',
                    'SCRIPT_FILENAME'  => '/var/web/html/index.php',
                ],
                '/index.php',
                '',
            ],
            [
                [
                    'REQUEST_URI'     => '/index.php/news/3?var1=val1&var2=val2',
                    'PHP_SELF'        => '/index.php/news/3',
                    'SCRIPT_FILENAME' => '/var/web/html/index.php',
                ],
                '/index.php',
                '',
            ],
            [
                [
                    'ORIG_PATH_INFO'  => '/index.php/news/3',
                    'QUERY_STRING'    => 'var1=val1&var2=val2',
                    'PHP_SELF'        => '/index.php/news/3',
                    'SCRIPT_FILENAME' => '/var/web/html/index.php',
                ],
                '/index.php',
                '',
            ],
            [
                [
                    'REQUEST_URI'     => '/article/archive?foo=index.php',
                    'QUERY_STRING'    => 'foo=index.php',
                    'SCRIPT_FILENAME' => '/var/www/zftests/index.php',
                ],
                '',
                '',
            ],
            [
                [
                    'REQUEST_URI'     => '/html/index.php/news/3?var1=val1&var2=val2',
                    'PHP_SELF'        => '/html/index.php/news/3',
                    'SCRIPT_FILENAME' => '/var/web/html/index.php',
                ],
                '/html/index.php',
                '/html',
            ],
            [
                [
                    'REQUEST_URI'     => '/dir/action',
                    'PHP_SELF'        => '/dir/index.php',
                    'SCRIPT_FILENAME' => '/var/web/dir/index.php',
                ],
                '/dir',
                '/dir',
            ],
            [
                [
                    'SCRIPT_NAME'     => '/~username/public/index.php',
                    'REQUEST_URI'     => '/~username/public/',
                    'PHP_SELF'        => '/~username/public/index.php',
                    'SCRIPT_FILENAME' => '/Users/username/Sites/public/index.php',
                    'ORIG_SCRIPT_NAME' => null,
                ],
                '/~username/public',
                '/~username/public',
            ],
            // ZF2-206
            [
                [
                    'SCRIPT_NAME'     => '/zf2tut/index.php',
                    'REQUEST_URI'     => '/zf2tut/',
                    'PHP_SELF'        => '/zf2tut/index.php',
                    'SCRIPT_FILENAME' => 'c:/ZF2Tutorial/public/index.php',
                    'ORIG_SCRIPT_NAME' => null,
                ],
                '/zf2tut',
                '/zf2tut',
            ],
            [
                [
                    'REQUEST_URI'     => '/html/index.php/news/3?var1=val1&var2=/index.php',
                    'PHP_SELF'        => '/html/index.php/news/3',
                    'SCRIPT_FILENAME' => '/var/web/html/index.php',
                ],
                '/html/index.php',
                '/html',
            ],
            [
                [
                    'REQUEST_URI'     => '/html/index.php/news/index.php',
                    'PHP_SELF'        => '/html/index.php/news/index.php',
                    'SCRIPT_FILENAME' => '/var/web/html/index.php',
                ],
                '/html/index.php',
                '/html',
            ],
            // Test when url quert contains a full http url
            [
                [
                    'REQUEST_URI' => '/html/index.php?url=http://test.example.com/path/&foo=bar',
                    'PHP_SELF' => '/html/index.php',
                    'SCRIPT_FILENAME' => '/var/web/html/index.php',
                ],
                '/html/index.php',
                '/html',
            ],
        ];
    }

    /**
     * @dataProvider baseUrlAndPathProvider
     *
     * @param array  $server
     * @param string $baseUrl
     * @param string $basePath
     */
    public function testBasePathDetection(array $server, $baseUrl, $basePath)
    {
        $_SERVER = $server;
        $request = new Request();

        $this->assertEquals($baseUrl, $request->getBaseUrl());
        $this->assertEquals($basePath, $request->getBasePath());
    }

    /**
     * Data provider for testing server provided headers.
     */
    public static function serverHeaderProvider()
    {
        return [
            [
                [
                    'HTTP_USER_AGENT' => 'Dummy',
                ],
                'User-Agent',
                'Dummy',
            ],
            [
                [
                    'HTTP_CUSTOM_COUNT' => '0',
                ],
                'Custom-Count',
                '0',
            ],
            [
                [
                    'CONTENT_TYPE' => 'text/html',
                ],
                'Content-Type',
                'text/html',
            ],
            [
                [
                    'CONTENT_LENGTH' => 0,
                ],
                'Content-Length',
                0,
            ],
            [
                [
                    'CONTENT_LENGTH' => 0,
                ],
                'Content-Length',
                0,
            ],
            [
                [
                    'CONTENT_LENGTH' => 12,
                ],
                'Content-Length',
                12,
            ],
            [
                [
                    'CONTENT_MD5' => md5('a'),
                ],
                'Content-MD5',
                md5('a'),
            ],
        ];
    }

    /**
     * @dataProvider serverHeaderProvider
     *
     * @param array  $server
     * @param string $name
     * @param string $value
     */
    public function testHeadersWithMinus(array $server, $name, $value)
    {
        $_SERVER = $server;
        $request = new Request();

        $header = $request->getHeaders()->get($name);
        $this->assertNotEquals($header, false);
        $this->assertEquals($name, $header->getFieldName($value));
        $this->assertEquals($value, $header->getFieldValue($value));
    }

    /**
     * @dataProvider serverHeaderProvider
     *
     * @param array  $server
     * @param string $name
     */
    public function testRequestStringHasCorrectHeaderName(array $server, $name)
    {
        $_SERVER = $server;
        $request = new Request();

        $this->assertContains($name, $request->toString());
    }

    /**
     * Data provider for testing server hostname.
     */
    public static function serverHostnameProvider()
    {
        return [
            [
                [
                    'SERVER_NAME' => 'test.example.com',
                    'REQUEST_URI' => 'http://test.example.com/news',
                ],
                'test.example.com',
                '80',
                '/news',
            ],
            [
                [
                    'HTTP_HOST' => 'test.example.com',
                    'REQUEST_URI' => 'http://test.example.com/news',
                ],
                'test.example.com',
                '80',
                '/news',
            ],
            [
                [
                    'SERVER_NAME' => 'test.example.com',
                    'HTTP_HOST' => 'requested.example.com',
                    'REQUEST_URI' => 'http://test.example.com/news',
                ],
                'requested.example.com',
                '80',
                '/news',
            ],
            [
                [
                    'SERVER_NAME' => 'test.example.com',
                    'HTTP_HOST' => '<script>alert("Spoofed host");</script>',
                    'REQUEST_URI' => 'http://test.example.com/news',
                ],
                'test.example.com',
                '80',
                '/news',
            ],
            [
                [
                    'SERVER_NAME' => '[1:2:3:4:5:6::6]',
                    'SERVER_ADDR' => '1:2:3:4:5:6::6',
                    'SERVER_PORT' => '80',
                    'REQUEST_URI' => 'http://[1:2:3:4:5:6::6]/news',
                ],
                '[1:2:3:4:5:6::6]',
                '80',
                '/news',
            ],
            // Test for broken $_SERVER implementation from Windows-Safari
            [
                [
                    'SERVER_NAME' => '[1:2:3:4:5:6:]',
                    'SERVER_ADDR' => '1:2:3:4:5:6::6',
                    'SERVER_PORT' => '6',
                    'REQUEST_URI' => 'http://[1:2:3:4:5:6::6]/news',
                ],
                '[1:2:3:4:5:6::6]',
                '80',
                '/news',
            ],
            [
                [
                    'SERVER_NAME' => 'test.example.com',
                    'SERVER_PORT' => '8080',
                    'REQUEST_URI' => 'http://test.example.com/news',
                ],
                'test.example.com',
                '8080',
                '/news',
            ],
            [
                [
                    'SERVER_NAME' => 'test.example.com',
                    'SERVER_PORT' => '443',
                    'HTTPS'       => 'on',
                    'REQUEST_URI' => 'https://test.example.com/news',
                ],
                'test.example.com',
                '443',
                '/news',
            ],
            // Test for HTTPS requests which are forwarded over a reverse proxy/load balancer
            [
                [
                    'SERVER_NAME' => 'test.example.com',
                    'SERVER_PORT' => '443',
                    'HTTP_X_FORWARDED_PROTO' => 'https',
                    'REQUEST_URI' => 'https://test.example.com/news',
                ],
                'test.example.com',
                '443',
                '/news',
            ],
            // Test when url query contains a full http url
            [
                [
                    'SERVER_NAME' => 'test.example.com',
                    'REQUEST_URI' => '/html/index.php?url=http://test.example.com/path/&foo=bar',
                ],
                'test.example.com',
                '80',
                '/html/index.php?url=http://test.example.com/path/&foo=bar',
            ],
        ];
    }

    /**
     * @dataProvider serverHostnameProvider
     *
     * @param array $server
     * @param string $expectedHost
     * @param string $expectedPort
     * @param string $expectedRequestUri
     */
    public function testServerHostnameProvider(array $server, $expectedHost, $expectedPort, $expectedRequestUri)
    {
        $_SERVER = $server;
        $request = new Request();

        $host = $request->getUri()->getHost();
        $this->assertEquals($expectedHost, $host);

        $uriParts = parse_url($_SERVER['REQUEST_URI']);
        if (isset($uriParts['scheme'])) {
            $scheme = $request->getUri()->getScheme();
            $this->assertEquals($uriParts['scheme'], $scheme);
        }

        $port = $request->getUri()->getPort();
        $this->assertEquals($expectedPort, $port);

        $requestUri = $request->getRequestUri();
        $this->assertEquals($expectedRequestUri, $requestUri);
    }

    /**
     * Data provider for testing mapping $_FILES
     *
     * @return array
     */
    public static function filesProvider()
    {
        return [
            // single file
            [
                [
                    'file' => [
                        'name' => 'test1.txt',
                        'type' => 'text/plain',
                        'tmp_name' => '/tmp/phpXXX',
                        'error' => 0,
                        'size' => 1,
                    ],
                ],
                [
                    'file' => [
                        'name' => 'test1.txt',
                        'type' => 'text/plain',
                        'tmp_name' => '/tmp/phpXXX',
                        'error' => 0,
                        'size' => 1,
                    ],
                ],
            ],
            // file name with brackets and int keys
            // file[], file[]
            [
                [
                    'file' => [
                        'name' => [
                            0 => 'test1.txt',
                            1 => 'test2.txt',
                        ],
                        'type' => [
                            0 => 'text/plain',
                            1 => 'text/plain',
                        ],
                        'tmp_name' => [
                            0 => '/tmp/phpXXX',
                            1 => '/tmp/phpXXX',
                        ],
                        'error' => [
                            0 => 0,
                            1 => 0,
                        ],
                        'size' => [
                            0 => 1,
                            1 => 1,
                        ],
                    ],
                ],
                [
                    'file' => [
                        0 => [
                            'name' => 'test1.txt',
                            'type' => 'text/plain',
                            'tmp_name' => '/tmp/phpXXX',
                            'error' => 0,
                            'size' => 1,
                        ],
                        1 => [
                            'name' => 'test2.txt',
                            'type' => 'text/plain',
                            'tmp_name' => '/tmp/phpXXX',
                            'error' => 0,
                            'size' => 1,
                        ],
                    ],
                ],
            ],
            // file name with brackets and string keys
            // file[one], file[two]
            [
                [
                    'file' => [
                        'name' => [
                            'one' => 'test1.txt',
                            'two' => 'test2.txt',
                        ],
                        'type' => [
                            'one' => 'text/plain',
                            'two' => 'text/plain',
                        ],
                        'tmp_name' => [
                            'one' => '/tmp/phpXXX',
                            'two' => '/tmp/phpXXX',
                        ],
                        'error' => [
                            'one' => 0,
                            'two' => 0,
                        ],
                        'size' => [
                            'one' => 1,
                            'two' => 1,
                        ],
                      ],
                ],
                [
                    'file' => [
                        'one' => [
                            'name' => 'test1.txt',
                            'type' => 'text/plain',
                            'tmp_name' => '/tmp/phpXXX',
                            'error' => 0,
                            'size' => 1,
                        ],
                        'two' => [
                            'name' => 'test2.txt',
                            'type' => 'text/plain',
                            'tmp_name' => '/tmp/phpXXX',
                            'error' => 0,
                            'size' => 1,
                        ],
                    ],
                ],
            ],
            // multilevel file name
            // file[], file[][], file[][][]
            [
                [
                    'file' => [
                        'name' => [
                            0 => 'test_0.txt',
                            1 => [
                                0 => 'test_10.txt',
                            ],
                            2 => [
                                0 => [
                                    0 => 'test_200.txt',
                                ],
                            ],
                        ],
                        'type' => [
                            0 => 'text/plain',
                            1 => [
                                0 => 'text/plain',
                            ],
                            2 => [
                                0 => [
                                    0 => 'text/plain',
                                ],
                            ],
                        ],
                        'tmp_name' => [
                            0 => '/tmp/phpXXX',
                            1 => [
                                0 => '/tmp/phpXXX',
                            ],
                            2 => [
                                0 => [
                                    0 => '/tmp/phpXXX',
                                ],
                            ],
                        ],
                        'error' => [
                            0 => 0,
                            1 => [
                                0 => 0,
                            ],
                            2 => [
                                0 => [
                                    0 => 0,
                                ],
                            ],
                        ],
                        'size' => [
                            0 => 1,
                            1 => [
                                0 => 1,
                            ],
                            2 => [
                                0 => [
                                    0 => 1,
                                ],
                            ],
                        ],
                    ],
                ],
                [
                    'file' => [
                        0 => [
                            'name' => 'test_0.txt',
                            'type' => 'text/plain',
                            'tmp_name' => '/tmp/phpXXX',
                            'error' => 0,
                            'size' => 1,
                        ],
                        1 => [
                            0 => [
                                'name' => 'test_10.txt',
                                'type' => 'text/plain',
                                'tmp_name' => '/tmp/phpXXX',
                                'error' => 0,
                                'size' => 1,
                            ],
                        ],
                        2 => [
                            0 => [
                                0 => [
                                    'name' => 'test_200.txt',
                                    'type' => 'text/plain',
                                    'tmp_name' => '/tmp/phpXXX',
                                    'error' => 0,
                                    'size' => 1,
                                ],
                            ],
                        ],
                    ],
                ],
            ],
        ];
    }

    /**
     * @dataProvider filesProvider
     *
     * @param array $files
     * @param array $expectedFiles
     */
    public function testRequestMapsPhpFies(array $files, array $expectedFiles)
    {
        $_FILES = $files;
        $request = new Request();
        $this->assertEquals($expectedFiles, $request->getFiles()->toArray());
    }

    public function testParameterRetrievalDefaultValue()
    {
        $request = new Request();
        $p = new Parameters([
            'foo' => 'bar',
        ]);
        $request->setQuery($p);
        $request->setPost($p);
        $request->setFiles($p);
        $request->setServer($p);
        $request->setEnv($p);

        $default = 15;
        $this->assertSame($default, $request->getQuery('baz', $default));
        $this->assertSame($default, $request->getPost('baz', $default));
        $this->assertSame($default, $request->getFiles('baz', $default));
        $this->assertSame($default, $request->getServer('baz', $default));
        $this->assertSame($default, $request->getEnv('baz', $default));
        $this->assertSame($default, $request->getHeaders('baz', $default));
        $this->assertSame($default, $request->getHeader('baz', $default));
    }

    public function testRetrievingASingleValueForParameters()
    {
        $request = new Request();
        $p = new Parameters([
            'foo' => 'bar',
        ]);
        $request->setQuery($p);
        $request->setPost($p);
        $request->setFiles($p);
        $request->setServer($p);
        $request->setEnv($p);

        $this->assertSame('bar', $request->getQuery('foo'));
        $this->assertSame('bar', $request->getPost('foo'));
        $this->assertSame('bar', $request->getFiles('foo'));
        $this->assertSame('bar', $request->getServer('foo'));
        $this->assertSame('bar', $request->getEnv('foo'));

        $headers = new Headers();
        $h = new GenericHeader('foo', 'bar');
        $headers->addHeader($h);

        $request->setHeaders($headers);
        $this->assertSame($headers, $request->getHeaders());
        $this->assertSame($h, $request->getHeaders()->get('foo'));
        $this->assertSame($h, $request->getHeader('foo'));
    }

    /**
     * @group ZF2-480
     */
    public function testBaseurlFallsBackToRootPathIfScriptFilenameIsNotSet()
    {
        $request = new Request();
        $server  = $request->getServer();
        $server->set('SCRIPT_NAME', null);
        $server->set('PHP_SELF', null);
        $server->set('ORIG_SCRIPT_NAME', null);
        $server->set('ORIG_SCRIPT_NAME', null);
        $server->set('SCRIPT_FILENAME', null);

        $this->assertEquals('', $request->getBaseUrl());
    }

    public function testAllowCustomMethodsFlagCanBeSetWithConstructor()
    {
        $_SERVER['REQUEST_METHOD'] = 'xcustomx';

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('Invalid HTTP method passed');

        new Request(false);
    }

    /**
     * @group 6896
     */
    public function testHandlesUppercaseHttpsFlags()
    {
        $_SERVER['HTTPS'] = 'OFF';

        $request = new Request();

        $this->assertSame('http', $request->getUri()->getScheme());
    }

    /**
     * @group 14
     */
    public function testDetectBaseUrlDoesNotRaiseErrorOnEmptyBaseUrl()
    {
        // This is testing that a PHP error is not raised. It would normally be
        // raised when we call `getBaseUrl()`; the assertion is essentially
        // just to make sure we get past that point.
        $_SERVER['SCRIPT_FILENAME'] = '';
        $_SERVER['SCRIPT_NAME']     = '';

        $request = new Request();
        $url     = $request->getBaseUrl();

        // If no baseUrl is detected at all, an empty string is returned.
        $this->assertEquals('', $url);
    }

    public function testDetectCorrectBaseUrlForConsoleRequests()
    {
        $_SERVER['argv']            = ['/home/user/package/vendor/bin/phpunit'];
        $_SERVER['argc']            = 1;
        $_SERVER['SCRIPT_FILENAME'] = '/home/user/package/vendor/bin/phpunit';
        $_SERVER['SCRIPT_NAME']     = '/home/user/package/vendor/bin/phpunit';
        $_SERVER['PHP_SELF']        = '/home/user/package/vendor/bin/phpunit';

        $request = new Request();
        $request->setRequestUri('/path/query/phpunit');

        $this->assertSame('', $request->getBaseUrl());
    }
}
