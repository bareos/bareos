<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Helper;

use PHPUnit\Framework\TestCase;
use Zend\View\Exception;
use Zend\View\Helper;
use Zend\View\Renderer\PhpRenderer as View;
use Zend\View\Exception\ExceptionInterface as ViewException;

/**
 * Test class for Zend\View\Helper\HeadLink.
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class HeadLinkTest extends TestCase
{
    /**
     * @var Helper\HeadLink
     */
    public $helper;

    /**
     * @var Helper\EscapeHtmlAttr
     */
    public $attributeEscaper;

    /**
     * @var string
     */
    public $basePath;

    /**
     * Sets up the fixture, for example, open a network connection.
     * This method is called before a test is executed.
     *
     * @return void
     */
    public function setUp()
    {
        Helper\Doctype::unsetDoctypeRegistry();
        $this->basePath = __DIR__ . '/_files/modules';
        $this->view     = new View();
        $this->helper   = new Helper\HeadLink();
        $this->helper->setView($this->view);
        $this->attributeEscaper  = new Helper\EscapeHtmlAttr();
    }

    /**
     * Tears down the fixture, for example, close a network connection.
     * This method is called after a test is executed.
     *
     * @return void
     */
    public function tearDown()
    {
        unset($this->helper);
    }

    public function testHeadLinkReturnsObjectInstance()
    {
        $placeholder = $this->helper->__invoke();
        $this->assertInstanceOf(Helper\HeadLink::class, $placeholder);
    }

    public function testPrependThrowsExceptionWithoutArrayArgument()
    {
        $this->expectException(Exception\ExceptionInterface::class);
        $this->helper->prepend('foo');
    }

    public function testAppendThrowsExceptionWithoutArrayArgument()
    {
        $this->expectException(Exception\ExceptionInterface::class);
        $this->helper->append('foo');
    }

    public function testSetThrowsExceptionWithoutArrayArgument()
    {
        $this->expectException(Exception\ExceptionInterface::class);
        $this->helper->set('foo');
    }

    public function testOffsetSetThrowsExceptionWithoutArrayArgument()
    {
        $this->expectException(Exception\ExceptionInterface::class);
        $this->helper->offsetSet(1, 'foo');
    }

    public function testCreatingLinkStackViaHeadLinkCreatesAppropriateOutput()
    {
        $links = [
            'link1' => ['rel' => 'stylesheet', 'type' => 'text/css', 'href' => 'foo'],
            'link2' => ['rel' => 'stylesheet', 'type' => 'text/css', 'href' => 'bar'],
            'link3' => ['rel' => 'stylesheet', 'type' => 'text/css', 'href' => 'baz'],
        ];
        $this->helper->headLink($links['link1'])
                     ->headLink($links['link2'], 'PREPEND')
                     ->headLink($links['link3']);

        $string = $this->helper->toString();
        $lines  = substr_count($string, PHP_EOL);
        $this->assertEquals(2, $lines);
        $lines  = substr_count($string, '<link ');
        $this->assertEquals(3, $lines, $string);

        $attributeEscaper = $this->attributeEscaper;

        foreach ($links as $link) {
            $substr = ' href="' . $attributeEscaper($link['href']) . '"';
            $this->assertContains($substr, $string);
            $substr = ' rel="' . $attributeEscaper($link['rel']) . '"';
            $this->assertContains($substr, $string);
            $substr = ' type="' . $attributeEscaper($link['type']) . '"';
            $this->assertContains($substr, $string);
        }

        $order = [];
        foreach ($this->helper as $key => $value) {
            if (isset($value->href)) {
                $order[$key] = $value->href;
            }
        }
        $expected = ['bar', 'foo', 'baz'];
        $this->assertSame($expected, $order);
    }

    public function testCreatingLinkStackViaStyleSheetMethodsCreatesAppropriateOutput()
    {
        $links = [
            'link1' => ['rel' => 'stylesheet', 'type' => 'text/css', 'href' => 'foo'],
            'link2' => ['rel' => 'stylesheet', 'type' => 'text/css', 'href' => 'bar'],
            'link3' => ['rel' => 'stylesheet', 'type' => 'text/css', 'href' => 'baz'],
        ];
        $this->helper->appendStylesheet($links['link1']['href'])
                     ->prependStylesheet($links['link2']['href'])
                     ->appendStylesheet($links['link3']['href']);

        $string = $this->helper->toString();
        $lines  = substr_count($string, PHP_EOL);
        $this->assertEquals(2, $lines);
        $lines  = substr_count($string, '<link ');
        $this->assertEquals(3, $lines, $string);

        $attributeEscaper = $this->attributeEscaper;

        foreach ($links as $link) {
            $substr = ' href="' . $attributeEscaper($link['href']) . '"';
            $this->assertContains($substr, $string);
            $substr = ' rel="' . $attributeEscaper($link['rel']) . '"';
            $this->assertContains($substr, $string);
            $substr = ' type="' . $attributeEscaper($link['type']) . '"';
            $this->assertContains($substr, $string);
        }

        $order = [];
        foreach ($this->helper as $key => $value) {
            if (isset($value->href)) {
                $order[$key] = $value->href;
            }
        }
        $expected = ['bar', 'foo', 'baz'];
        $this->assertSame($expected, $order);
    }

    public function testCreatingLinkStackViaAlternateMethodsCreatesAppropriateOutput()
    {
        $links = [
            'link1' => ['title' => 'stylesheet', 'type' => 'text/css', 'href' => 'foo'],
            'link2' => ['title' => 'stylesheet', 'type' => 'text/css', 'href' => 'bar'],
            'link3' => ['title' => 'stylesheet', 'type' => 'text/css', 'href' => 'baz'],
        ];
        $where = 'append';
        foreach ($links as $link) {
            $method = $where . 'Alternate';
            $this->helper->$method($link['href'], $link['type'], $link['title']);
            $where = ('append' == $where) ? 'prepend' : 'append';
        }

        $string = $this->helper->toString();
        $lines  = substr_count($string, PHP_EOL);
        $this->assertEquals(2, $lines);
        $lines  = substr_count($string, '<link ');
        $this->assertEquals(3, $lines, $string);
        $lines  = substr_count($string, ' rel="alternate"');
        $this->assertEquals(3, $lines, $string);

        $attributeEscaper = $this->attributeEscaper;

        foreach ($links as $link) {
            $substr = ' href="' . $attributeEscaper($link['href']) . '"';
            $this->assertContains($substr, $string);
            $substr = ' title="' . $attributeEscaper($link['title']) . '"';
            $this->assertContains($substr, $string);
            $substr = ' type="' . $attributeEscaper($link['type']) . '"';
            $this->assertContains($substr, $string);
        }

        $order = [];
        foreach ($this->helper as $key => $value) {
            if (isset($value->href)) {
                $order[$key] = $value->href;
            }
        }
        $expected = ['bar', 'foo', 'baz'];
        $this->assertSame($expected, $order);
    }

    public function testOverloadingThrowsExceptionWithNoArguments()
    {
        $this->expectException(Exception\ExceptionInterface::class);
        $this->helper->appendStylesheet();
    }

    public function testOverloadingShouldAllowSingleArrayArgument()
    {
        $this->helper->setStylesheet(['href' => '/styles.css']);
        $link = $this->helper->getValue();
        $this->assertEquals('/styles.css', $link->href);
    }

    public function testOverloadingUsingSingleArrayArgumentWithInvalidValuesThrowsException()
    {
        $this->expectException(Exception\ExceptionInterface::class);
        $this->helper->setStylesheet(['bogus' => 'unused']);
    }

    public function testOverloadingOffsetSetWorks()
    {
        $this->helper->offsetSetStylesheet(100, '/styles.css');
        $items = $this->helper->getArrayCopy();
        $this->assertTrue(isset($items[100]));
        $link = $items[100];
        $this->assertEquals('/styles.css', $link->href);
    }

    public function testOverloadingThrowsExceptionWithInvalidMethod()
    {
        $this->expectException(Exception\ExceptionInterface::class);
        $this->helper->bogusMethod();
    }

    public function testStylesheetAttributesGetSet()
    {
        $this->helper->setStylesheet('/styles.css', 'projection', 'ie6');
        $item = $this->helper->getValue();
        $this->assertObjectHasAttribute('media', $item);
        $this->assertObjectHasAttribute('conditionalStylesheet', $item);

        $this->assertEquals('projection', $item->media);
        $this->assertEquals('ie6', $item->conditionalStylesheet);
    }

    public function testConditionalStylesheetNotCreatedByDefault()
    {
        $this->helper->setStylesheet('/styles.css');
        $item = $this->helper->getValue();
        $this->assertObjectHasAttribute('conditionalStylesheet', $item);
        $this->assertFalse($item->conditionalStylesheet);

        $attributeEscaper = $this->attributeEscaper;

        $string = $this->helper->toString();
        $this->assertContains($attributeEscaper('/styles.css'), $string);
        $this->assertNotContains('<!--[if', $string);
        $this->assertNotContains(']>', $string);
        $this->assertNotContains('<![endif]-->', $string);
    }

    public function testConditionalStylesheetCreationOccursWhenRequested()
    {
        $this->helper->setStylesheet('/styles.css', 'screen', 'ie6');
        $item = $this->helper->getValue();
        $this->assertObjectHasAttribute('conditionalStylesheet', $item);
        $this->assertEquals('ie6', $item->conditionalStylesheet);

        $attributeEscaper = $this->attributeEscaper;

        $string = $this->helper->toString();
        $this->assertContains($attributeEscaper('/styles.css'), $string);
        $this->assertContains('<!--[if ie6]>', $string);
        $this->assertContains('<![endif]-->', $string);
    }

    public function testConditionalStylesheetCreationNoIE()
    {
        $this->helper->setStylesheet('/styles.css', 'screen', '!IE');
        $item = $this->helper->getValue();
        $this->assertObjectHasAttribute('conditionalStylesheet', $item);
        $this->assertEquals('!IE', $item->conditionalStylesheet);

        $attributeEscaper = $this->attributeEscaper;

        $string = $this->helper->toString();
        $this->assertContains($attributeEscaper('/styles.css'), $string);
        $this->assertContains('<!--[if !IE]><!--><', $string);
        $this->assertContains('<!--<![endif]-->', $string);
    }

    public function testConditionalStylesheetCreationNoIEWidthSpaces()
    {
        $this->helper->setStylesheet('/styles.css', 'screen', '! IE');
        $item = $this->helper->getValue();
        $this->assertObjectHasAttribute('conditionalStylesheet', $item);
        $this->assertEquals('! IE', $item->conditionalStylesheet);

        $attributeEscaper = $this->attributeEscaper;

        $string = $this->helper->toString();
        $this->assertContains($attributeEscaper('/styles.css'), $string);
        $this->assertContains('<!--[if ! IE]><!--><', $string);
        $this->assertContains('<!--<![endif]-->', $string);
    }

    public function testSettingAlternateWithTooFewArgsRaisesException()
    {
        try {
            $this->helper->setAlternate('foo');
            $this->fail('Setting alternate with fewer than 3 args should raise exception');
        } catch (ViewException $e) {
        }
        try {
            $this->helper->setAlternate('foo', 'bar');
            $this->fail('Setting alternate with fewer than 3 args should raise exception');
        } catch (ViewException $e) {
        }
    }

    public function testIndentationIsHonored()
    {
        $this->helper->setIndent(4);
        $this->helper->appendStylesheet('/css/screen.css');
        $this->helper->appendStylesheet('/css/rules.css');
        $string = $this->helper->toString();

        $scripts = substr_count($string, '    <link ');
        $this->assertEquals(2, $scripts);
    }

    public function testLinkRendersAsPlainHtmlIfDoctypeNotXhtml()
    {
        $this->view->plugin('doctype')->__invoke('HTML4_STRICT');
        $this->helper->__invoke(['rel' => 'icon', 'src' => '/foo/bar'])
                     ->__invoke(['rel' => 'foo', 'href' => '/bar/baz']);
        $test = $this->helper->toString();
        $this->assertNotContains(' />', $test);
    }

    public function testDoesNotAllowDuplicateStylesheets()
    {
        $this->helper->appendStylesheet('foo');
        $this->helper->appendStylesheet('foo');
        $this->assertEquals(1, count($this->helper), var_export($this->helper->getContainer()->getArrayCopy(), 1));
    }

    /**
     * test for ZF-2889
     */
    public function testBooleanStylesheet()
    {
        $this->helper->appendStylesheet(['href' => '/bar/baz', 'conditionalStylesheet' => false]);
        $test = $this->helper->toString();
        $this->assertNotContains('[if false]', $test);
    }

    /**
     * test for ZF-3271
     *
     */
    public function testBooleanTrueConditionalStylesheet()
    {
        $this->helper->appendStylesheet(['href' => '/bar/baz', 'conditionalStylesheet' => true]);
        $test = $this->helper->toString();
        $this->assertNotContains('[if 1]', $test);
        $this->assertNotContains('[if true]', $test);
    }

    /**
     * @issue ZF-3928
     * @link http://framework.zend.com/issues/browse/ZF-3928
     */
    public function testTurnOffAutoEscapeDoesNotEncodeAmpersand()
    {
        $this->helper->setAutoEscape(false)->appendStylesheet('/css/rules.css?id=123&foo=bar');
        $this->assertContains('id=123&foo=bar', $this->helper->toString());
    }

    public function testSetAlternateWithExtras()
    {
        $this->helper->setAlternate('/mydocument.pdf', 'application/pdf', 'foo', ['media' => ['print', 'screen']]);
        $test = $this->helper->toString();
        $this->assertContains('media="print,screen"', $test);
    }

    public function testAppendStylesheetWithExtras()
    {
        $this->helper->appendStylesheet([
            'href' => '/bar/baz',
            'conditionalStylesheet' => false,
            'extras' => ['id' => 'my_link_tag']
        ]);
        $test = $this->helper->toString();
        $this->assertContains('id="my_link_tag"', $test);
    }

    public function testSetStylesheetWithMediaAsArray()
    {
        $this->helper->appendStylesheet('/bar/baz', ['screen', 'print']);
        $test = $this->helper->toString();
        $this->assertContains(' media="screen,print"', $test);
    }

    public function testSetPrevRelationship()
    {
        $this->helper->appendPrev('/foo/bar');
        $test = $this->helper->toString();

        $attributeEscaper = $this->attributeEscaper;

        $this->assertContains('href="' . $attributeEscaper('/foo/bar') . '"', $test);
        $this->assertContains('rel="prev"', $test);
    }

    public function testSetNextRelationship()
    {
        $this->helper->appendNext('/foo/bar');
        $test = $this->helper->toString();

        $attributeEscaper = $this->attributeEscaper;

        $this->assertContains('href="' . $attributeEscaper('/foo/bar') . '"', $test);
        $this->assertContains('rel="next"', $test);
    }

    /**
     * @issue ZF-5435
     */
    public function testContainerMaintainsCorrectOrderOfItems()
    {
        $this->helper->__invoke()->offsetSetStylesheet(1, '/test1.css');
        $this->helper->__invoke()->offsetSetStylesheet(10, '/test2.css');
        $this->helper->__invoke()->offsetSetStylesheet(20, '/test3.css');
        $this->helper->__invoke()->offsetSetStylesheet(5, '/test4.css');

        $attributeEscaper = $this->attributeEscaper;

        $test = $this->helper->toString();

        $expected = sprintf(
            '<link href="%3$s" media="screen" rel="stylesheet" type="%2$s">%1$s'
            . '<link href="%4$s" media="screen" rel="stylesheet" type="%2$s">%1$s'
            . '<link href="%5$s" media="screen" rel="stylesheet" type="%2$s">%1$s'
            . '<link href="%6$s" media="screen" rel="stylesheet" type="%2$s">',
            PHP_EOL,
            $attributeEscaper('text/css'),
            $attributeEscaper('/test1.css'),
            $attributeEscaper('/test4.css'),
            $attributeEscaper('/test2.css'),
            $attributeEscaper('/test3.css')
        );

        $this->assertEquals($expected, $test);
    }

    /**
     * @issue ZF-10345
     */
    public function testIdAttributeIsSupported()
    {
        $this->helper->appendStylesheet(['href' => '/bar/baz', 'id' => 'foo']);
        $this->assertContains('id="foo"', $this->helper->toString());
    }

    /**
     * @group 6635
     */
    public function testSizesAttributeIsSupported()
    {
        $this->helper->appendStylesheet(['rel' => 'icon', 'href' => '/bar/baz', 'sizes' => '123x456']);
        $this->assertContains('sizes="123x456"', $this->helper->toString());
    }

    public function testItempropAttributeIsSupported()
    {
        $this->helper->prependAlternate(['itemprop' => 'url', 'href' => '/bar/baz', 'rel' => 'canonical']);
        $this->assertContains('itemprop="url"', $this->helper->toString());
    }

    public function testAsAttributeIsSupported()
    {
        $this->helper->headLink(['as' => 'style', 'href' => '/foo/bar.css', 'rel' => 'preload']);
        $this->assertContains('as="style"', $this->helper->toString());
    }
}
