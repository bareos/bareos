<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Client;

use PHPUnit\Framework\TestCase;
use Zend\Http\Client as HTTPClient;
use Zend\Http\Client\Adapter\AdapterInterface;
use Zend\Http\Client\Adapter\Socket;
use Zend\Http\Request;

/**
 * This are the test for the prototype of Zend\Http\Client
 *
 * @group      Zend_Http
 * @group      Zend_Http_Client
 */
class UseCaseTest extends TestCase
{
    /**
     * The bast URI for this test, containing all files in the files directory
     * Should be set in phpunit.xml or phpunit.xml.dist
     *
     * @var string
     */
    protected $baseuri;

    /**
     * Common HTTP client
     *
     * @var HTTPClient
     */
    protected $client;

    /**
     * Common HTTP client adapter
     *
     * @var AdapterInterface
     */
    protected $adapter;

    /**
     * Configuration array
     *
     * @var array
     */
    protected $config = [
        'adapter' => Socket::class,
    ];

    /**
     * Set up the test case
     */
    protected function setUp()
    {
        if (getenv('TESTS_ZEND_HTTP_CLIENT_BASEURI')
            && (filter_var(getenv('TESTS_ZEND_HTTP_CLIENT_BASEURI'), FILTER_VALIDATE_BOOLEAN) != false)
        ) {
            $this->baseuri = getenv('TESTS_ZEND_HTTP_CLIENT_BASEURI');
            $this->client  = new HTTPClient($this->baseuri);
        } else {
            // Skip tests
            $this->markTestSkipped(sprintf(
                '%s dynamic tests are not enabled in phpunit.xml',
                HTTPClient::class
            ));
        }
    }

    /**
     * Clean up the test environment
     */
    protected function tearDown()
    {
        $this->client = null;
    }

    public function testHttpGet()
    {
        $this->client->setMethod(Request::METHOD_GET);
        $response = $this->client->send();
        $this->assertTrue($response->isSuccess());
    }

    public function testStaticHttpGet()
    {
        //        $response= HTTPClient::get($this->baseuri);
//        $this->assertTrue($response->isSuccess());
    }

    public function testRequestHttpGet()
    {
        $client = new HTTPClient();
        $request = new Request();
        $request->setUri($this->baseuri);
        $request->setMethod(Request::METHOD_GET);
        $response = $client->send($request);
        $this->assertTrue($response->isSuccess());
    }
}
