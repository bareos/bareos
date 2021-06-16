<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\PhpEnvironment;

use PHPUnit\Framework\TestCase;
use Zend\Http\Exception\InvalidArgumentException;
use Zend\Http\PhpEnvironment\Response;

class ResponseTest extends TestCase
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

    public function testReturnsOneOhVersionWhenDetectedInServerSuperglobal()
    {
        // HTTP/1.0
        $_SERVER['SERVER_PROTOCOL'] = 'HTTP/1.0';
        $response = new Response();
        $this->assertSame(Response::VERSION_10, $response->getVersion());
    }

    public function testReturnsOneOneVersionWhenDetectedInServerSuperglobal()
    {
        // HTTP/1.1
        $_SERVER['SERVER_PROTOCOL'] = 'HTTP/1.1';
        $response = new Response();
        $this->assertSame(Response::VERSION_11, $response->getVersion());
    }

    public function testFallsBackToVersionOneOhWhenServerSuperglobalVersionIsNotRecognized()
    {
        // unknown protocol or version -> fallback to HTTP/1.0
        $_SERVER['SERVER_PROTOCOL'] = 'zf/2.0';
        $response = new Response();
        $this->assertSame(Response::VERSION_10, $response->getVersion());
    }

    public function testFallsBackToVersionOneOhWhenNoVersionDetectedInServerSuperglobal()
    {
        // undefined -> fallback to HTTP/1.0
        unset($_SERVER['SERVER_PROTOCOL']);
        $response = new Response();
        $this->assertSame(Response::VERSION_10, $response->getVersion());
    }

    public function testCanExplicitlySetVersion()
    {
        $response = new Response();
        $response->setVersion(Response::VERSION_11);
        $this->assertSame(Response::VERSION_11, $response->getVersion());

        $response->setVersion(Response::VERSION_10);
        $this->assertSame(Response::VERSION_10, $response->getVersion());

        $this->expectException(InvalidArgumentException::class);
        $response->setVersion('zf/2.0');
    }
}
