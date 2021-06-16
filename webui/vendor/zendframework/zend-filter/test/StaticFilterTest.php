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
use Zend\Filter\Callback;
use Zend\Filter\Digits;
use Zend\Filter\FilterPluginManager;
use Zend\Filter\HtmlEntities;
use Zend\Filter\StaticFilter;
use Zend\ServiceManager\Exception;
use Zend\ServiceManager\ServiceManager;

class StaticFilterTest extends TestCase
{
    /**
     * Resets the default namespaces
     *
     * @return void
     */
    public function tearDown()
    {
        StaticFilter::setPluginManager(null);
    }

    public function testUsesFilterPluginManagerByDefault()
    {
        $plugins = StaticFilter::getPluginManager();
        $this->assertInstanceOf('Zend\Filter\FilterPluginManager', $plugins);
    }

    public function testCanSpecifyCustomPluginManager()
    {
        $plugins = new FilterPluginManager(new ServiceManager());
        StaticFilter::setPluginManager($plugins);
        $this->assertSame($plugins, StaticFilter::getPluginManager());
    }

    public function testCanResetPluginManagerByPassingNull()
    {
        $plugins = new FilterPluginManager(new ServiceManager());
        StaticFilter::setPluginManager($plugins);
        $this->assertSame($plugins, StaticFilter::getPluginManager());
        StaticFilter::setPluginManager(null);
        $registered = StaticFilter::getPluginManager();
        $this->assertNotSame($plugins, $registered);
        $this->assertInstanceOf('Zend\Filter\FilterPluginManager', $registered);
    }

    /**
     * Ensures that we can call the static method execute()
     * to instantiate a named validator by its class basename
     * and it returns the result of filter() with the input.
     */
    public function testStaticFactory()
    {
        $filteredValue = StaticFilter::execute('1a2b3c4d', Digits::class);
        $this->assertEquals('1234', $filteredValue);
    }

    /**
     * Ensures that a validator with constructor arguments can be called
     * with the static method get().
     */
    public function testStaticFactoryWithConstructorArguments()
    {
        // Test HtmlEntities with one ctor argument.
        $filteredValue = StaticFilter::execute('"O\'Reilly"', HtmlEntities::class, ['quotestyle' => ENT_COMPAT]);
        $this->assertEquals('&quot;O\'Reilly&quot;', $filteredValue);

        // Test HtmlEntities with a different ctor argument,
        // and make sure it gives the correct response
        // so we know it passed the arg to the ctor.
        $filteredValue = StaticFilter::execute('"O\'Reilly"', HtmlEntities::class, ['quotestyle' => ENT_QUOTES]);
        $this->assertEquals('&quot;O&#039;Reilly&quot;', $filteredValue);
    }

    /**
     * Ensures that if we specify a validator class basename that doesn't
     * exist in the namespace, get() throws an exception.
     *
     * Refactored to conform with ZF-2724.
     *
     * @group  ZF-2724
     */
    public function testStaticFactoryClassNotFound()
    {
        $this->expectException(Exception\ExceptionInterface::class);
        StaticFilter::execute('1234', 'UnknownFilter');
    }

    public function testUsesDifferentConfigurationOnEachRequest()
    {
        $first = StaticFilter::execute('foo', Callback::class, [
            'callback' => function ($value) {
                return 'FOO';
            },
        ]);
        $second = StaticFilter::execute('foo', Callback::class, [
            'callback' => function ($value) {
                return 'BAR';
            },
        ]);
        $this->assertNotSame($first, $second);
        $this->assertEquals('FOO', $first);
        $this->assertEquals('BAR', $second);
    }
}
