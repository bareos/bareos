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
use Zend\View;
use Zend\View\Helper;

/**
 * Test class for Zend\View\Helper\HeadScript.
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class HeadScriptTest extends TestCase
{
    /**
     * @var Helper\HeadScript
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
        $this->basePath = __DIR__ . '/_files/modules';
        $this->helper = new Helper\HeadScript();
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

    public function testHeadScriptReturnsObjectInstance()
    {
        $placeholder = $this->helper->__invoke();
        $this->assertInstanceOf(Helper\HeadScript::class, $placeholder);
    }

    public function testSetPrependAppendAndOffsetSetThrowExceptionsOnInvalidItems()
    {
        try {
            $this->helper->append('foo');
            $this->fail('Append should throw exception with invalid item');
        } catch (View\Exception\ExceptionInterface $e) {
        }
        try {
            $this->helper->offsetSet(1, 'foo');
            $this->fail('OffsetSet should throw exception with invalid item');
        } catch (View\Exception\ExceptionInterface $e) {
        }
        try {
            $this->helper->prepend('foo');
            $this->fail('Prepend should throw exception with invalid item');
        } catch (View\Exception\ExceptionInterface $e) {
        }
        try {
            $this->helper->set('foo');
            $this->fail('Set should throw exception with invalid item');
        } catch (View\Exception\ExceptionInterface $e) {
        }
    }

    // @codingStandardsIgnoreStart
    protected function _inflectAction($type)
    {
        // @codingStandardsIgnoreEnd
        return ucfirst(strtolower($type));
    }

    // @codingStandardsIgnoreStart
    protected function _testOverloadAppend($type)
    {
        // @codingStandardsIgnoreEnd
        $action = 'append' . $this->_inflectAction($type);
        $string = 'foo';
        for ($i = 0; $i < 3; ++$i) {
            $string .= ' foo';
            $this->helper->$action($string);
            $values = $this->helper->getArrayCopy();
            $this->assertEquals($i + 1, count($values));
            if ('file' == $type) {
                $this->assertEquals($string, $values[$i]->attributes['src']);
            } elseif ('script' == $type) {
                $this->assertEquals($string, $values[$i]->source);
            }
            $this->assertEquals('text/javascript', $values[$i]->type);
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
            $this->helper->$action($string);
            $values = $this->helper->getArrayCopy();
            $this->assertEquals($i + 1, count($values));
            $first = array_shift($values);
            if ('file' == $type) {
                $this->assertEquals($string, $first->attributes['src']);
            } elseif ('script' == $type) {
                $this->assertEquals($string, $first->source);
            }
            $this->assertEquals('text/javascript', $first->type);
        }
    }

    // @codingStandardsIgnoreStart
    protected function _testOverloadSet($type)
    {
        // @codingStandardsIgnoreEnd
        $action = 'set' . $this->_inflectAction($type);
        $string = 'foo';
        for ($i = 0; $i < 3; ++$i) {
            $this->helper->appendScript($string);
            $string .= ' foo';
        }
        $this->helper->$action($string);
        $values = $this->helper->getArrayCopy();
        $this->assertEquals(1, count($values));
        if ('file' == $type) {
            $this->assertEquals($string, $values[0]->attributes['src']);
        } elseif ('script' == $type) {
            $this->assertEquals($string, $values[0]->source);
        }
        $this->assertEquals('text/javascript', $values[0]->type);
    }

    // @codingStandardsIgnoreStart
    protected function _testOverloadOffsetSet($type)
    {
        // @codingStandardsIgnoreEnd
        $action = 'offsetSet' . $this->_inflectAction($type);
        $string = 'foo';
        $this->helper->$action(5, $string);
        $values = $this->helper->getArrayCopy();
        $this->assertEquals(1, count($values));
        if ('file' == $type) {
            $this->assertEquals($string, $values[5]->attributes['src']);
        } elseif ('script' == $type) {
            $this->assertEquals($string, $values[5]->source);
        }
        $this->assertEquals('text/javascript', $values[5]->type);
    }

    public function testOverloadAppendFileAppendsScriptsToStack()
    {
        $this->_testOverloadAppend('file');
    }

    public function testOverloadAppendScriptAppendsScriptsToStack()
    {
        $this->_testOverloadAppend('script');
    }

    public function testOverloadPrependFileAppendsScriptsToStack()
    {
        $this->_testOverloadPrepend('file');
    }

    public function testOverloadPrependScriptAppendsScriptsToStack()
    {
        $this->_testOverloadPrepend('script');
    }

    public function testOverloadSetFileOverwritesStack()
    {
        $this->_testOverloadSet('file');
    }

    public function testOverloadSetScriptOverwritesStack()
    {
        $this->_testOverloadSet('script');
    }

    public function testOverloadOffsetSetFileWritesToSpecifiedIndex()
    {
        $this->_testOverloadOffsetSet('file');
    }

    public function testOverloadOffsetSetScriptWritesToSpecifiedIndex()
    {
        $this->_testOverloadOffsetSet('script');
    }

    public function testOverloadingThrowsExceptionWithInvalidMethod()
    {
        try {
            $this->helper->fooBar('foo');
            $this->fail('Invalid method should raise exception');
        } catch (View\Exception\ExceptionInterface $e) {
        }
    }

    public function testOverloadingWithTooFewArgumentsRaisesException()
    {
        try {
            $this->helper->setScript();
            $this->fail('Too few arguments should raise exception');
        } catch (View\Exception\ExceptionInterface $e) {
        }

        try {
            $this->helper->offsetSetScript(5);
            $this->fail('Too few arguments should raise exception');
        } catch (View\Exception\ExceptionInterface $e) {
        }
    }

    public function testHeadScriptAppropriatelySetsScriptItems()
    {
        $this->helper->__invoke('FILE', 'foo', 'set')
                     ->__invoke('SCRIPT', 'bar', 'prepend')
                     ->__invoke('SCRIPT', 'baz', 'append');
        $items = $this->helper->getArrayCopy();
        for ($i = 0; $i < 3; ++$i) {
            $item = $items[$i];
            switch ($i) {
                case 0:
                    $this->assertObjectHasAttribute('source', $item);
                    $this->assertEquals('bar', $item->source);
                    break;
                case 1:
                    $this->assertObjectHasAttribute('attributes', $item);
                    $this->assertTrue(isset($item->attributes['src']));
                    $this->assertEquals('foo', $item->attributes['src']);
                    break;
                case 2:
                    $this->assertObjectHasAttribute('source', $item);
                    $this->assertEquals('baz', $item->source);
                    break;
            }
        }
    }

    public function testToStringRendersValidHtml()
    {
        $this->helper->__invoke('FILE', 'foo', 'set')
                     ->__invoke('SCRIPT', 'bar', 'prepend')
                     ->__invoke('SCRIPT', 'baz', 'append');
        $string = $this->helper->toString();

        $scripts = substr_count($string, '<script ');
        $this->assertEquals(3, $scripts);
        $scripts = substr_count($string, '</script>');
        $this->assertEquals(3, $scripts);
        $scripts = substr_count($string, 'src="');
        $this->assertEquals(1, $scripts);
        $scripts = substr_count($string, '><');
        $this->assertEquals(1, $scripts);

        $this->assertContains('src="foo"', $string);
        $this->assertContains('bar', $string);
        $this->assertContains('baz', $string);

        $doc = new \DOMDocument;
        $dom = $doc->loadHtml($string);
        $this->assertTrue($dom);
    }

    public function testCapturingCapturesToObject()
    {
        $this->helper->captureStart();
        echo 'foobar';
        $this->helper->captureEnd();
        $values = $this->helper->getArrayCopy();
        $this->assertEquals(1, count($values), var_export($values, 1));
        $item = array_shift($values);
        $this->assertContains('foobar', $item->source);
    }

    public function testIndentationIsHonored()
    {
        $this->helper->setIndent(4);
        $this->helper->appendScript('
var foo = "bar";
    document.write(foo.strlen());');
        $this->helper->appendScript('
var bar = "baz";
document.write(bar.strlen());');
        $string = $this->helper->toString();

        $scripts = substr_count($string, '    <script');
        $this->assertEquals(2, $scripts);
        $this->assertContains('    //', $string);
        $this->assertContains('var', $string);
        $this->assertContains('document', $string);
        $this->assertContains('    document', $string);
    }

    public function testDoesNotAllowDuplicateFiles()
    {
        $this->helper->__invoke('FILE', '/js/prototype.js');
        $this->helper->__invoke('FILE', '/js/prototype.js');
        $this->assertEquals(1, count($this->helper));
    }

    public function testRenderingDoesNotRenderArbitraryAttributesByDefault()
    {
        $this->helper->__invoke()->appendFile('/js/foo.js', 'text/javascript', ['bogus' => 'deferred']);
        $test = $this->helper->__invoke()->toString();
        $this->assertNotContains('bogus="deferred"', $test);
    }

    public function testCanRenderArbitraryAttributesOnRequest()
    {
        $this->helper->__invoke()->appendFile('/js/foo.js', 'text/javascript', ['bogus' => 'deferred'])
             ->setAllowArbitraryAttributes(true);
        $test = $this->helper->__invoke()->toString();
        $this->assertContains('bogus="deferred"', $test);
    }

    public function testCanPerformMultipleSerialCaptures()
    {
        $this->helper->__invoke()->captureStart();
        echo "this is something captured";
        $this->helper->__invoke()->captureEnd();

        $this->helper->__invoke()->captureStart();

        echo "this is something else captured";
        $this->helper->__invoke()->captureEnd();
    }

    public function testCannotNestCaptures()
    {
        $this->helper->__invoke()->captureStart();
        echo "this is something captured";
        try {
            $this->helper->__invoke()->captureStart();
            $this->helper->__invoke()->captureEnd();
            $this->fail('Should not be able to nest captures');
        } catch (View\Exception\ExceptionInterface $e) {
            $this->helper->__invoke()->captureEnd();
            $this->assertContains('Cannot nest', $e->getMessage());
        }
    }

    /**
     * @issue ZF-3928
     * @link http://framework.zend.com/issues/browse/ZF-3928
     */
    public function testTurnOffAutoEscapeDoesNotEncodeAmpersand()
    {
        $this->helper->setAutoEscape(false)->appendFile('test.js?id=123&foo=bar');
        $this->assertEquals(
            '<script type="text/javascript" src="test.js?id=123&foo=bar"></script>',
            $this->helper->toString()
        );
    }

    public function testConditionalScript()
    {
        $this->helper->__invoke()->appendFile('/js/foo.js', 'text/javascript', ['conditional' => 'lt IE 7']);
        $test = $this->helper->__invoke()->toString();
        $this->assertContains('<!--[if lt IE 7]>', $test);
    }

    public function testConditionalScriptWidthIndentation()
    {
        $this->helper->__invoke()->appendFile('/js/foo.js', 'text/javascript', ['conditional' => 'lt IE 7']);
        $this->helper->__invoke()->setIndent(4);
        $test = $this->helper->__invoke()->toString();
        $this->assertContains('    <!--[if lt IE 7]>', $test);
    }

    public function testConditionalScriptNoIE()
    {
        $this->helper->setAllowArbitraryAttributes(true);
        $this->helper->appendFile(
            '/js/foo.js',
            'text/javascript',
            ['conditional' => '!IE']
        );
        $test = $this->helper->toString();

        $this->assertContains('<!--[if !IE]><!--><', $test);
        $this->assertContains('<!--<![endif]-->', $test);
    }

    public function testConditionalScriptNoIEWidthSpace()
    {
        $this->helper->setAllowArbitraryAttributes(true);
        $this->helper->appendFile(
            '/js/foo.js',
            'text/javascript',
            ['conditional' => '! IE']
        );
        $test = $this->helper->toString();

        $this->assertContains('<!--[if ! IE]><!--><', $test);
        $this->assertContains('<!--<![endif]-->', $test);
    }

    /**
     * @issue ZF-5435
     */
    public function testContainerMaintainsCorrectOrderOfItems()
    {
        $this->helper->offsetSetFile(1, 'test1.js');
        $this->helper->offsetSetFile(20, 'test2.js');
        $this->helper->offsetSetFile(10, 'test3.js');
        $this->helper->offsetSetFile(5, 'test4.js');


        $test = $this->helper->toString();

        $attributeEscaper = $this->attributeEscaper;

        $expected = sprintf(
            '<script type="%2$s" src="%3$s"></script>%1$s'
            . '<script type="%2$s" src="%4$s"></script>%1$s'
            . '<script type="%2$s" src="%5$s"></script>%1$s'
            . '<script type="%2$s" src="%6$s"></script>',
            PHP_EOL,
            $attributeEscaper('text/javascript'),
            $attributeEscaper('test1.js'),
            $attributeEscaper('test4.js'),
            $attributeEscaper('test3.js'),
            $attributeEscaper('test2.js')
        );

        $this->assertEquals($expected, $test);
    }

    public function testConditionalWithAllowArbitraryAttributesDoesNotIncludeConditionalScript()
    {
        $this->helper->__invoke()->setAllowArbitraryAttributes(true);
        $this->helper->__invoke()->appendFile('/js/foo.js', 'text/javascript', ['conditional' => 'lt IE 7']);
        $test = $this->helper->__invoke()->toString();

        $this->assertNotContains('conditional', $test);
    }

    public function testNoEscapeWithAllowArbitraryAttributesDoesNotIncludeNoEscapeScript()
    {
        $this->helper->__invoke()->setAllowArbitraryAttributes(true);
        $this->helper->__invoke()->appendScript('// some script', 'text/javascript', ['noescape' => true]);
        $test = $this->helper->__invoke()->toString();

        $this->assertNotContains('noescape', $test);
    }

    public function testNoEscapeDefaultsToFalse()
    {
        $this->helper->__invoke()->appendScript('// some script' . PHP_EOL, 'text/javascript', []);
        $test = $this->helper->__invoke()->toString();

        $this->assertContains('//<!--', $test);
        $this->assertContains('//-->', $test);
    }

    public function testNoEscapeTrue()
    {
        $this->helper->__invoke()->appendScript('// some script' . PHP_EOL, 'text/javascript', ['noescape' => true]);
        $test = $this->helper->__invoke()->toString();

        $this->assertNotContains('//<!--', $test);
        $this->assertNotContains('//-->', $test);
    }

    /**
     * @group 6634
     */
    public function testSupportsCrossOriginAttribute()
    {
        $this->helper->__invoke()->appendScript(
            '// some script' . PHP_EOL,
            'text/javascript',
            ['crossorigin' => true]
        );
        $test = $this->helper->__invoke()->toString();

        $this->assertContains('crossorigin="', $test);
    }

    /**
     * @group 21
     */
    public function testOmitsTypeAttributeIfEmptyValueAndHtml5Doctype()
    {
        $view = new \Zend\View\Renderer\PhpRenderer();
        $view->plugin('doctype')->setDoctype(\Zend\View\Helper\Doctype::HTML5);
        $this->helper->setView($view);

        $this->helper->__invoke()->appendScript('// some script' . PHP_EOL, '');
        $test = $this->helper->__invoke()->toString();
        $this->assertNotContains('type', $test);
    }

    /**
     * @group 22
     */
    public function testSupportsAsyncAttribute()
    {
        $this->helper->__invoke()->appendScript(
            '// some script' . PHP_EOL,
            'text/javascript',
            ['async' => true]
        );
        $test = $this->helper->__invoke()->toString();
        $this->assertContains('async="', $test);
    }
}
