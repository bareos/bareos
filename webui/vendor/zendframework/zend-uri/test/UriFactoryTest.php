<?php
/**
 * @see       https://github.com/zendframework/zend-uri for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-uri/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Uri;

use PHPUnit\Framework\TestCase;
use Zend\Uri\UriFactory;

/**
 * @group      Zend_Uri
 */
class UriFactoryTest extends TestCase
{
    /**
     * General composing / parsing tests
     */

    /**
     * Test registering a new Scheme
     *
     * @param        string $scheme
     * @param        string $class
     * @dataProvider registeringNewSchemeProvider
     */
    public function testRegisteringNewScheme($scheme, $class)
    {
        $this->assertAttributeNotContains($class, 'schemeClasses', '\Zend\Uri\UriFactory');
        UriFactory::registerScheme($scheme, $class);
        $this->assertAttributeContains($class, 'schemeClasses', '\Zend\Uri\UriFactory');
        UriFactory::unregisterScheme($scheme);
        $this->assertAttributeNotContains($class, 'schemeClasses', '\Zend\Uri\UriFactory');
    }

    /**
     * Provide the data for the RegisterNewScheme-test
     */
    public function registeringNewSchemeProvider()
    {
        return [
            ['ssh', 'Foo\Bar\Class'],
            ['ntp', 'No real class at all!!!'],
        ];
    }

    /**
     * Test creation of new URI with an existing scheme-classd
     *
     * @param string $uri           THe URI to create
     * @param string $expectedClass The class expected
     *
     * @dataProvider createUriWithFactoryProvider
     */
    public function testCreateUriWithFactory($uri, $expectedClass)
    {
        $class = UriFactory::factory($uri);
        $this->assertInstanceof($expectedClass, $class);
    }

    /**
     * Providethe data for the CreateUriWithFactory-test
     *
     * @return array
     */
    public function createUriWithFactoryProvider()
    {
        return [
            ['http://example.com', 'Zend\Uri\Http'],
            ['https://example.com', 'Zend\Uri\Http'],
            ['mailto://example.com', 'Zend\Uri\Mailto'],
            ['file://example.com', 'Zend\Uri\File'],
        ];
    }

    /**
     * Test, that unknown Schemes will result in an exception
     *
     * @param string $uri an uri with an unknown scheme
     * @expectedException \Zend\Uri\Exception\InvalidArgumentException
     * @dataProvider unknownSchemeThrowsExceptionProvider
     */
    public function testUnknownSchemeThrowsException($uri)
    {
        $url = UriFactory::factory($uri);
    }

    /**
     * Provide data to the unknownSchemeThrowsException-TEst
     *
     * @return array
     */
    public function unknownSchemeThrowsExceptionProvider()
    {
        return [
            ['foo://bar'],
            ['ssh://bar'],
        ];
    }
}
