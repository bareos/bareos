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
use Zend\View\Renderer\PhpRenderer as View;
use Zend\View\Helper;
use Zend\View\Exception\ExceptionInterface as ViewException;

/**
 * Test class for Zend\View\Helper\HeadMeta.
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class HeadMetaTest extends TestCase
{
    /**
     * @var Helper\HeadMeta
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
        $this->error = false;
        Helper\Doctype::unsetDoctypeRegistry();
        $this->basePath = __DIR__ . '/_files/modules';
        $this->view     = new View();
        $this->view->plugin('doctype')->__invoke('XHTML1_STRICT');
        $this->helper   = new Helper\HeadMeta();
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

    public function handleErrors($errno, $errstr)
    {
        $this->error = $errstr;
    }

    public function testHeadMetaReturnsObjectInstance()
    {
        $placeholder = $this->helper->__invoke();
        $this->assertInstanceOf(Helper\HeadMeta::class, $placeholder);
    }

    public function testAppendPrependAndSetThrowExceptionsWhenNonMetaValueProvided()
    {
        try {
            $this->helper->append('foo');
            $this->fail('Non-meta value should not append');
        } catch (ViewException $e) {
        }
        try {
            $this->helper->offsetSet(3, 'foo');
            $this->fail('Non-meta value should not offsetSet');
        } catch (ViewException $e) {
        }
        try {
            $this->helper->prepend('foo');
            $this->fail('Non-meta value should not prepend');
        } catch (ViewException $e) {
        }
        try {
            $this->helper->set('foo');
            $this->fail('Non-meta value should not set');
        } catch (ViewException $e) {
        }
    }

    // @codingStandardsIgnoreStart
    protected function _inflectAction($type)
    {
        // @codingStandardsIgnoreEnd
        $type = str_replace('-', ' ', $type);
        $type = ucwords($type);
        $type = str_replace(' ', '', $type);
        return $type;
    }

    // @codingStandardsIgnoreStart
    protected function _testOverloadAppend($type)
    {
        // @codingStandardsIgnoreEnd
        $action = 'append' . $this->_inflectAction($type);
        $string = 'foo';
        for ($i = 0; $i < 3; ++$i) {
            $string .= ' foo';
            $this->helper->$action('keywords', $string);
            $values = $this->helper->getArrayCopy();
            $this->assertEquals($i + 1, count($values));

            $item   = $values[$i];
            $this->assertObjectHasAttribute('type', $item);
            $this->assertObjectHasAttribute('modifiers', $item);
            $this->assertObjectHasAttribute('content', $item);
            $this->assertObjectHasAttribute($item->type, $item);
            $this->assertEquals('keywords', $item->{$item->type});
            $this->assertEquals($string, $item->content);
        }
    }

    // @codingStandardsIgnoreStart
    protected function _testOverloadPrepend($type)
    {
        // @codingStandardsIgnoreEnd
        $action = 'prepend' . $this->_inflectAction($type);
        $string = 'foo';
        for ($i = 0; $i < 3; ++$i) {
            $string .= ' foo';
            $this->helper->$action('keywords', $string);
            $values = $this->helper->getArrayCopy();
            $this->assertEquals($i + 1, count($values));
            $item = array_shift($values);

            $this->assertObjectHasAttribute('type', $item);
            $this->assertObjectHasAttribute('modifiers', $item);
            $this->assertObjectHasAttribute('content', $item);
            $this->assertObjectHasAttribute($item->type, $item);
            $this->assertEquals('keywords', $item->{$item->type});
            $this->assertEquals($string, $item->content);
        }
    }

    // @codingStandardsIgnoreStart
    protected function _testOverloadSet($type)
    {
        // @codingStandardsIgnoreEnd
        $setAction = 'set' . $this->_inflectAction($type);
        $appendAction = 'append' . $this->_inflectAction($type);
        $string = 'foo';
        for ($i = 0; $i < 3; ++$i) {
            $this->helper->$appendAction('keywords', $string);
            $string .= ' foo';
        }
        $this->helper->$setAction('keywords', $string);
        $values = $this->helper->getArrayCopy();
        $this->assertEquals(1, count($values));
        $item = array_shift($values);

        $this->assertObjectHasAttribute('type', $item);
        $this->assertObjectHasAttribute('modifiers', $item);
        $this->assertObjectHasAttribute('content', $item);
        $this->assertObjectHasAttribute($item->type, $item);
        $this->assertEquals('keywords', $item->{$item->type});
        $this->assertEquals($string, $item->content);
    }

    public function testOverloadingAppendNameAppendsMetaTagToStack()
    {
        $this->_testOverloadAppend('name');
    }

    public function testOverloadingPrependNamePrependsMetaTagToStack()
    {
        $this->_testOverloadPrepend('name');
    }

    public function testOverloadingSetNameOverwritesMetaTagStack()
    {
        $this->_testOverloadSet('name');
    }

    public function testOverloadingAppendHttpEquivAppendsMetaTagToStack()
    {
        $this->_testOverloadAppend('http-equiv');
    }

    public function testOverloadingPrependHttpEquivPrependsMetaTagToStack()
    {
        $this->_testOverloadPrepend('http-equiv');
    }

    public function testOverloadingSetHttpEquivOverwritesMetaTagStack()
    {
        $this->_testOverloadSet('http-equiv');
    }

    public function testOverloadingThrowsExceptionWithFewerThanTwoArgs()
    {
        $this->expectException(Exception\ExceptionInterface::class);
        $this->helper->setName('foo');
    }

    public function testOverloadingThrowsExceptionWithInvalidMethodType()
    {
        $this->expectException(Exception\ExceptionInterface::class);
        $this->helper->setFoo('foo');
    }

    public function testCanBuildMetaTagsWithAttributes()
    {
        $this->helper->setName('keywords', 'foo bar', ['lang' => 'us_en', 'scheme' => 'foo', 'bogus' => 'unused']);
        $value = $this->helper->getValue();

        $this->assertObjectHasAttribute('modifiers', $value);
        $modifiers = $value->modifiers;
        $this->assertArrayHasKey('lang', $modifiers);
        $this->assertEquals('us_en', $modifiers['lang']);
        $this->assertArrayHasKey('scheme', $modifiers);
        $this->assertEquals('foo', $modifiers['scheme']);
    }

    public function testToStringReturnsValidHtml()
    {
        $this->helper->setName('keywords', 'foo bar', ['lang' => 'us_en', 'scheme' => 'foo', 'bogus' => 'unused'])
                     ->prependName('title', 'boo bah')
                     ->appendHttpEquiv('screen', 'projection');
        $string = $this->helper->toString();

        $metas = substr_count($string, '<meta ');
        $this->assertEquals(3, $metas);
        $metas = substr_count($string, '/>');
        $this->assertEquals(3, $metas);
        $metas = substr_count($string, 'name="');
        $this->assertEquals(2, $metas);
        $metas = substr_count($string, 'http-equiv="');
        $this->assertEquals(1, $metas);

        $attributeEscaper = $this->attributeEscaper;

        $this->assertContains('http-equiv="screen" content="projection"', $string);
        $this->assertContains('name="keywords" content="' . $attributeEscaper('foo bar') . '"', $string);
        $this->assertContains('lang="us_en"', $string);
        $this->assertContains('scheme="foo"', $string);
        $this->assertNotContains('bogus', $string);
        $this->assertNotContains('unused', $string);
        $this->assertContains('name="title" content="' . $attributeEscaper('boo bah') . '"', $string);
    }

    /**
     * @group ZF-6637
     */
    public function testToStringWhenInvalidKeyProvidedShouldConvertThrownException()
    {
        $this->helper->__invoke('some-content', 'tag value', 'not allowed key');
        set_error_handler([$this, 'handleErrors']);
        $string = @$this->helper->toString();
        $this->assertEquals('', $string);
        $this->assertInternalType('string', $this->error);
    }

    public function testHeadMetaHelperCreatesItemEntry()
    {
        $this->helper->__invoke('foo', 'keywords');
        $values = $this->helper->getArrayCopy();
        $this->assertEquals(1, count($values));
        $item = array_shift($values);
        $this->assertEquals('foo', $item->content);
        $this->assertEquals('name', $item->type);
        $this->assertEquals('keywords', $item->name);
    }

    public function testOverloadingOffsetInsertsAtOffset()
    {
        $this->helper->offsetSetName(100, 'keywords', 'foo');
        $values = $this->helper->getArrayCopy();
        $this->assertEquals(1, count($values));
        $this->assertArrayHasKey(100, $values);
        $item = $values[100];
        $this->assertEquals('foo', $item->content);
        $this->assertEquals('name', $item->type);
        $this->assertEquals('keywords', $item->name);
    }

    public function testIndentationIsHonored()
    {
        $this->helper->setIndent(4);
        $this->helper->appendName('keywords', 'foo bar');
        $this->helper->appendName('seo', 'baz bat');
        $string = $this->helper->toString();

        $scripts = substr_count($string, '    <meta name=');
        $this->assertEquals(2, $scripts);
    }

    public function testStringRepresentationReflectsDoctype()
    {
        $this->view->plugin('doctype')->__invoke('HTML4_STRICT');
        $this->helper->__invoke('some content', 'foo');

        $test = $this->helper->toString();

        $attributeEscaper = $this->attributeEscaper;

        $this->assertNotContains('/>', $test);
        $this->assertContains($attributeEscaper('some content'), $test);
        $this->assertContains('foo', $test);
    }

    /**
     * @issue ZF-2663
     */
    public function testSetNameDoesntClobber()
    {
        $view = new View();
        $view->plugin('headMeta')->setName('keywords', 'foo');
        $view->plugin('headMeta')->appendHttpEquiv('pragma', 'bar');
        $view->plugin('headMeta')->appendHttpEquiv('Cache-control', 'baz');
        $view->plugin('headMeta')->setName('keywords', 'bat');

        $this->assertEquals(
            '<meta http-equiv="pragma" content="bar" />' . PHP_EOL . '<meta http-equiv="Cache-control" content="baz" />'
            . PHP_EOL . '<meta name="keywords" content="bat" />',
            $view->plugin('headMeta')->toString()
        );
    }

    /**
     * @issue ZF-2663
     */
    public function testSetNameDoesntClobberPart2()
    {
        $view = new View();
        $view->plugin('headMeta')->setName('keywords', 'foo');
        $view->plugin('headMeta')->setName('description', 'foo');
        $view->plugin('headMeta')->appendHttpEquiv('pragma', 'baz');
        $view->plugin('headMeta')->appendHttpEquiv('Cache-control', 'baz');
        $view->plugin('headMeta')->setName('keywords', 'bar');

        $expected = sprintf(
            '<meta name="description" content="foo" />%1$s'
            . '<meta http-equiv="pragma" content="baz" />%1$s'
            . '<meta http-equiv="Cache-control" content="baz" />%1$s'
            . '<meta name="keywords" content="bar" />',
            PHP_EOL
        );

        $this->assertEquals($expected, $view->plugin('headMeta')->toString());
    }

    /**
     * @issue ZF-3780
     * @link http://framework.zend.com/issues/browse/ZF-3780
     */
    public function testPlacesMetaTagsInProperOrder()
    {
        $view = new View();
        $view->plugin('headMeta')->setName('keywords', 'foo');
        $view->plugin('headMeta')->__invoke(
            'some content',
            'bar',
            'name',
            [],
            \Zend\View\Helper\Placeholder\Container\AbstractContainer::PREPEND
        );

        $attributeEscaper = $this->attributeEscaper;

        $expected = sprintf(
            '<meta name="bar" content="%s" />%s'
            . '<meta name="keywords" content="foo" />',
            $attributeEscaper('some content'),
            PHP_EOL
        );
        $this->assertEquals($expected, $view->plugin('headMeta')->toString());
    }

    /**
     * @issue ZF-5435
     */
    public function testContainerMaintainsCorrectOrderOfItems()
    {
        $this->helper->offsetSetName(1, 'keywords', 'foo');
        $this->helper->offsetSetName(10, 'description', 'foo');
        $this->helper->offsetSetHttpEquiv(20, 'pragma', 'baz');
        $this->helper->offsetSetHttpEquiv(5, 'Cache-control', 'baz');

        $test = $this->helper->toString();

        $expected = sprintf(
            '<meta name="keywords" content="foo" />%1$s'
            . '<meta http-equiv="Cache-control" content="baz" />%1$s'
            . '<meta name="description" content="foo" />%1$s'
            . '<meta http-equiv="pragma" content="baz" />',
            PHP_EOL
        );

        $this->assertEquals($expected, $test);
    }

    /**
     * @issue ZF-7722
     */
    public function testCharsetValidateFail()
    {
        $view = new View();
        $view->plugin('doctype')->__invoke('HTML4_STRICT');

        $this->expectException(Exception\ExceptionInterface::class);
        $view->plugin('headMeta')->setCharset('utf-8');
    }

    /**
     * @issue ZF-7722
     */
    public function testCharset()
    {
        $view = new View();
        $view->plugin('doctype')->__invoke('HTML5');

        $view->plugin('headMeta')->setCharset('utf-8');
        $this->assertEquals(
            '<meta charset="utf-8">',
            $view->plugin('headMeta')->toString()
        );

        $view->plugin('doctype')->__invoke('XHTML5');

        $this->assertEquals(
            '<meta charset="utf-8"/>',
            $view->plugin('headMeta')->toString()
        );
    }

    public function testCharsetPosition()
    {
        $view = new View();
        $view->plugin('doctype')->__invoke('HTML5');

        $view->plugin('headMeta')
            ->setProperty('description', 'foobar')
            ->setCharset('utf-8');

        $this->assertEquals(
            '<meta charset="utf-8">' . PHP_EOL
            . '<meta property="description" content="foobar">',
            $view->plugin('headMeta')->toString()
        );
    }

    public function testCarsetWithXhtmlDoctypeGotException()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('XHTML* doctype has no attribute charset; please use appendHttpEquiv()');

        $view = new View();
        $view->plugin('doctype')->__invoke('XHTML1_RDFA');

        $view->plugin('headMeta')
             ->setCharset('utf-8');
    }

     /**
     * @group ZF-9743
     */
    public function testPropertyIsSupportedWithRdfaDoctype()
    {
        $this->view->doctype('XHTML1_RDFA');
        $this->helper->__invoke('foo', 'og:title', 'property');

        $attributeEscaper = $this->attributeEscaper;

        $expected = sprintf('<meta property="%s" content="foo" />', $attributeEscaper('og:title'));
        $this->assertEquals($expected, $this->helper->toString());
    }

    /**
     * @group ZF-9743
     */
    public function testPropertyIsNotSupportedByDefaultDoctype()
    {
        try {
            $this->helper->__invoke('foo', 'og:title', 'property');
            $this->fail('meta property attribute should not be supported on default doctype');
        } catch (ViewException $e) {
            $this->assertContains('Invalid value passed', $e->getMessage());
        }
    }

    /**
     * @group ZF-9743
     * @depends testPropertyIsSupportedWithRdfaDoctype
     */
    public function testOverloadingAppendPropertyAppendsMetaTagToStack()
    {
        $this->view->doctype('XHTML1_RDFA');
        $this->_testOverloadAppend('property');
    }

    /**
     * @group ZF-9743
     * @depends testPropertyIsSupportedWithRdfaDoctype
     */
    public function testOverloadingPrependPropertyPrependsMetaTagToStack()
    {
        $this->view->doctype('XHTML1_RDFA');
        $this->_testOverloadPrepend('property');
    }

    /**
     * @group ZF-9743
     * @depends testPropertyIsSupportedWithRdfaDoctype
     */
    public function testOverloadingSetPropertyOverwritesMetaTagStack()
    {
        $this->view->doctype('XHTML1_RDFA');
        $this->_testOverloadSet('property');
    }

     /**
     * @issue 3751
     */
    public function testItempropIsSupportedWithHtml5Doctype()
    {
        $this->view->doctype('HTML5');
        $this->helper->__invoke('HeadMeta with Microdata', 'description', 'itemprop');

        $attributeEscaper = $this->attributeEscaper;

        $expected = sprintf('<meta itemprop="description" content="%s">', $attributeEscaper('HeadMeta with Microdata'));
        $this->assertEquals($expected, $this->helper->toString());
    }

    /**
     * @issue 3751
     */
    public function testItempropIsNotSupportedByDefaultDoctype()
    {
        try {
            $this->helper->__invoke('HeadMeta with Microdata', 'description', 'itemprop');
            $this->fail('meta itemprop attribute should not be supported on default doctype');
        } catch (ViewException $e) {
            $this->assertContains('Invalid value passed', $e->getMessage());
        }
    }

    /**
     * @issue 3751
     * @depends testItempropIsSupportedWithHtml5Doctype
     */
    public function testOverloadingAppendItempropAppendsMetaTagToStack()
    {
        $this->view->doctype('HTML5');
        $this->_testOverloadAppend('itemprop');
    }

    /**
     * @issue 3751
     * @depends testItempropIsSupportedWithHtml5Doctype
     */
    public function testOverloadingPrependItempropPrependsMetaTagToStack()
    {
        $this->view->doctype('HTML5');
        $this->_testOverloadPrepend('itemprop');
    }

    /**
     * @issue 3751
     * @depends testItempropIsSupportedWithHtml5Doctype
     */
    public function testOverloadingSetItempropOverwritesMetaTagStack()
    {
        $this->view->doctype('HTML5');
        $this->_testOverloadSet('itemprop');
    }

    /**
     * @group ZF-11835
     */
    public function testConditional()
    {
        $html = $this->helper->appendHttpEquiv('foo', 'bar', ['conditional' => 'lt IE 7'])->toString();

        $this->assertRegExp("|^<!--\[if lt IE 7\]>|", $html);
        $this->assertRegExp("|<!\[endif\]-->$|", $html);
    }

    public function testConditionalNoIE()
    {
        $html = $this->helper->appendHttpEquiv('foo', 'bar', ['conditional' => '!IE'])->toString();

        $this->assertContains('<!--[if !IE]><!--><', $html);
        $this->assertContains('<!--<![endif]-->', $html);
    }

    public function testConditionalNoIEWidthSpace()
    {
        $html = $this->helper->appendHttpEquiv('foo', 'bar', ['conditional' => '! IE'])->toString();

        $this->assertContains('<!--[if ! IE]><!--><', $html);
        $this->assertContains('<!--<![endif]-->', $html);
    }

    public function testTurnOffAutoEscapeDoesNotEncode()
    {
        $this->helper->setAutoEscape(false)->appendHttpEquiv('foo', 'bar=baz');
        $this->assertEquals('<meta http-equiv="foo" content="bar=baz" />', $this->helper->toString());
    }
}
