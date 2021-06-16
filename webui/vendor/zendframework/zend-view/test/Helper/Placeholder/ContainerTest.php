<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Helper\Placeholder;

use PHPUnit\Framework\TestCase;

/**
 * Test class for Zend\View\Helper\Placeholder\Container.
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class ContainerTest extends TestCase
{
    /**
     * @var \Zend\View\Helper\Placeholder\Container
     */
    public $container;

    /**
     * Sets up the fixture, for example, open a network connection.
     * This method is called before a test is executed.
     *
     * @return void
     */
    public function setUp()
    {
        $this->container = new \Zend\View\Helper\Placeholder\Container([]);
    }

    /**
     * Tears down the fixture, for example, close a network connection.
     * This method is called after a test is executed.
     *
     * @return void
     */
    public function tearDown()
    {
        unset($this->container);
    }

    /**
     * @return void
     */
    public function testSetSetsASingleValue()
    {
        $this->container['foo'] = 'bar';
        $this->container['bar'] = 'baz';
        $this->assertEquals('bar', $this->container['foo']);
        $this->assertEquals('baz', $this->container['bar']);

        $this->container->set('foo');
        $this->assertEquals(1, count($this->container));
        $this->assertEquals('foo', $this->container[0]);
    }

    /**
     * @return void
     */
    public function testGetValueReturnsScalarWhenOneElementRegistered()
    {
        $this->container->set('foo');
        $this->assertEquals('foo', $this->container->getValue());
    }

    /**
     * @return void
     */
    public function testGetValueReturnsArrayWhenMultipleValuesPresent()
    {
        $this->container['foo'] = 'bar';
        $this->container['bar'] = 'baz';
        $expected = ['foo' => 'bar', 'bar' => 'baz'];
        $return   = $this->container->getValue();
        $this->assertEquals($expected, $return);
    }

    /**
     * @return void
     */
    public function testPrefixAccesorsWork()
    {
        $this->assertEquals('', $this->container->getPrefix());
        $this->container->setPrefix('<ul><li>');
        $this->assertEquals('<ul><li>', $this->container->getPrefix());
    }

    /**
     * @return void
     */
    public function testSetPrefixImplementsFluentInterface()
    {
        $result = $this->container->setPrefix('<ul><li>');
        $this->assertSame($this->container, $result);
    }

    /**
     * @return void
     */
    public function testPostfixAccesorsWork()
    {
        $this->assertEquals('', $this->container->getPostfix());
        $this->container->setPostfix('</li></ul>');
        $this->assertEquals('</li></ul>', $this->container->getPostfix());
    }

    /**
     * @return void
     */
    public function testSetPostfixImplementsFluentInterface()
    {
        $result = $this->container->setPostfix('</li></ul>');
        $this->assertSame($this->container, $result);
    }

    /**
     * @return void
     */
    public function testPrependImplementsFluentInterface()
    {
        $result = $this->container->prepend('test');
        $this->assertSame($this->container, $result);
    }

    public function testAppendImplementsFluentInterface()
    {
        $result = $this->container->append('test');
        $this->assertSame($this->container, $result);
    }

    /**
     * @return void
     */
    public function testSetImplementsFluentInterface()
    {
        $result = $this->container->set('test');
        $this->assertSame($this->container, $result);
    }

    /**
     * @return void
     */
    public function testSeparatorAccesorsWork()
    {
        $this->assertEquals('', $this->container->getSeparator());
        $this->container->setSeparator('</li><li>');
        $this->assertEquals('</li><li>', $this->container->getSeparator());
    }

    /**
     * @return void
     */
    public function testSetSeparatorImplementsFluentInterface()
    {
        $result = $this->container->setSeparator('</li><li>');
        $this->assertSame($this->container, $result);
    }

    /**
     * @return void
     */
    public function testIndentAccesorsWork()
    {
        $this->assertEquals('', $this->container->getIndent());
        $this->container->setIndent('    ');
        $this->assertEquals('    ', $this->container->getIndent());
        $this->container->setIndent(5);
        $this->assertEquals('     ', $this->container->getIndent());
    }

    /**
     * @return void
     */
    public function testSetIndentImplementsFluentInterface()
    {
        $result = $this->container->setIndent('    ');
        $this->assertSame($this->container, $result);
    }

    /**
     * @return void
     */
    public function testCapturingToPlaceholderStoresContent()
    {
        $this->container->captureStart();
        echo 'This is content intended for capture';
        $this->container->captureEnd();

        $value = $this->container->getValue();
        $this->assertContains('This is content intended for capture', $value);
    }

    /**
     * @return void
     */
    public function testCapturingToPlaceholderAppendsContent()
    {
        $this->container[] = 'foo';
        $originalCount = count($this->container);

        $this->container->captureStart();
        echo 'This is content intended for capture';
        $this->container->captureEnd();

        $this->assertEquals($originalCount + 1, count($this->container));

        $value     = $this->container->getValue();
        $keys      = array_keys($value);
        $lastIndex = array_pop($keys);
        $this->assertEquals('foo', $value[$lastIndex - 1]);
        $this->assertContains('This is content intended for capture', $value[$lastIndex]);
    }

    /**
     * @return void
     */
    public function testCapturingToPlaceholderUsingPrependPrependsContent()
    {
        $this->container[] = 'foo';
        $originalCount = count($this->container);

        $this->container->captureStart('PREPEND');
        echo 'This is content intended for capture';
        $this->container->captureEnd();

        $this->assertEquals($originalCount + 1, count($this->container));

        $value     = $this->container->getValue();
        $keys      = array_keys($value);
        $lastIndex = array_pop($keys);
        $this->assertEquals('foo', $value[$lastIndex]);
        $this->assertContains('This is content intended for capture', $value[$lastIndex - 1]);
    }

    /**
     * @return void
     */
    public function testCapturingToPlaceholderUsingSetOverwritesContent()
    {
        $this->container[] = 'foo';
        $this->container->captureStart('SET');
        echo 'This is content intended for capture';
        $this->container->captureEnd();

        $this->assertEquals(1, count($this->container));

        $value = $this->container->getValue();
        $this->assertContains('This is content intended for capture', $value);
    }

    /**
     * @return void
     */
    public function testCapturingToPlaceholderKeyUsingSetCapturesContent()
    {
        $this->container->captureStart('SET', 'key');
        echo 'This is content intended for capture';
        $this->container->captureEnd();

        $this->assertEquals(1, count($this->container));
        $this->assertTrue(isset($this->container['key']));
        $value = $this->container['key'];
        $this->assertContains('This is content intended for capture', $value);
    }

    /**
     * @return void
     */
    public function testCapturingToPlaceholderKeyUsingSetReplacesContentAtKey()
    {
        $this->container['key'] = 'Foobar';
        $this->container->captureStart('SET', 'key');
        echo 'This is content intended for capture';
        $this->container->captureEnd();

        $this->assertEquals(1, count($this->container));
        $this->assertTrue(isset($this->container['key']));
        $value = $this->container['key'];
        $this->assertContains('This is content intended for capture', $value);
    }

    /**
     * @return void
     */
    public function testCapturingToPlaceholderKeyUsingAppendAppendsContentAtKey()
    {
        $this->container['key'] = 'Foobar ';
        $this->container->captureStart('APPEND', 'key');
        echo 'This is content intended for capture';
        $this->container->captureEnd();

        $this->assertEquals(1, count($this->container));
        $this->assertTrue(isset($this->container['key']));
        $value = $this->container['key'];
        $this->assertContains('Foobar This is content intended for capture', $value);
    }

    /**
     * @return void
     */
    public function testNestedCapturesThrowsException()
    {
        $this->container[] = 'foo';
        $caught = false;
        try {
            $this->container->captureStart('SET');
            $this->container->captureStart('SET');
            $this->container->captureEnd();
            $this->container->captureEnd();
        } catch (\Exception $e) {
            $this->container->captureEnd();
            $caught = true;
        }

        $this->assertTrue($caught, 'Nested captures should throw exceptions');
    }

    /**
     * @return void
     */
    public function testToStringWithNoModifiersAndSingleValueReturnsValue()
    {
        $this->container->set('foo');
        $value = $this->container->toString();
        $this->assertEquals($this->container->getValue(), $value);
    }

    /**
     * @return void
     */
    public function testToStringWithModifiersAndSingleValueReturnsFormattedValue()
    {
        $this->container->set('foo');
        $this->container->setPrefix('<li>')
                        ->setPostfix('</li>');
        $value = $this->container->toString();
        $this->assertEquals('<li>foo</li>', $value);
    }

    /**
     * @return void
     */
    public function testToStringWithNoModifiersAndCollectionReturnsImplodedString()
    {
        $this->container[] = 'foo';
        $this->container[] = 'bar';
        $this->container[] = 'baz';
        $value = $this->container->toString();
        $this->assertEquals('foobarbaz', $value);
    }

    /**
     * @return void
     */
    public function testToStringWithModifiersAndCollectionReturnsFormattedString()
    {
        $this->container[] = 'foo';
        $this->container[] = 'bar';
        $this->container[] = 'baz';
        $this->container->setPrefix('<ul><li>')
                        ->setSeparator('</li><li>')
                        ->setPostfix('</li></ul>');
        $value = $this->container->toString();
        $this->assertEquals('<ul><li>foo</li><li>bar</li><li>baz</li></ul>', $value);
    }

    /**
     * @return void
     */
    public function testToStringWithModifiersAndCollectionReturnsFormattedStringWithIndentation()
    {
        $this->container[] = 'foo';
        $this->container[] = 'bar';
        $this->container[] = 'baz';
        $this->container->setPrefix('<ul><li>')
                        ->setSeparator('</li>' . PHP_EOL . '<li>')
                        ->setPostfix('</li></ul>')
                        ->setIndent('    ');
        $value = $this->container->toString();
        $expectedValue = '    <ul><li>foo</li>' . PHP_EOL . '    <li>bar</li>' . PHP_EOL . '    <li>baz</li></ul>';
        $this->assertEquals($expectedValue, $value);
    }

    /**
     * @return void
     */
    public function testToStringProxiesToToString()
    {
        $this->container[] = 'foo';
        $this->container[] = 'bar';
        $this->container[] = 'baz';
        $this->container->setPrefix('<ul><li>')
                        ->setSeparator('</li><li>')
                        ->setPostfix('</li></ul>');
        $value = $this->container->__toString();
        $this->assertEquals('<ul><li>foo</li><li>bar</li><li>baz</li></ul>', $value);
    }

    /**
     * @return void
     */
    public function testPrependPushesValueToTopOfContainer()
    {
        $this->container['foo'] = 'bar';
        $this->container->prepend('baz');

        $expected = ['baz', 'foo' => 'bar'];
        $array = $this->container->getArrayCopy();
        $this->assertSame($expected, $array);
    }

    public function testIndentationIsHonored()
    {
        $this->container->setIndent(4)
                        ->setPrefix("<ul>\n    <li>")
                        ->setSeparator("</li>\n    <li>")
                        ->setPostfix("</li>\n</ul>");
        $this->container->append('foo');
        $this->container->append('bar');
        $this->container->append('baz');
        $string = $this->container->toString();

        $lis = substr_count($string, "\n        <li>");
        $this->assertEquals(3, $lis);
        $this->assertContains("    <ul>\n", $string, $string);
        $this->assertContains("\n    </ul>", $string, $string);
    }

    /**
     * @see https://github.com/zendframework/zend-view/pull/133
     */
    public function testNoPrefixOrPostfixAreRenderedIfNoItemsArePresentInTheContainer()
    {
        $this->container
            ->setPrefix("<h1>")
            ->setPostfix("</h1>");
        $string = $this->container->toString();
        $this->assertEquals('', $string);

        $this->container->set('');
        $string = $this->container->toString();
        $this->assertEquals('<h1></h1>', $string);
    }
}
