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
use Zend\I18n\Translator\Translator;
use Zend\View\Helper;

/**
 * Test class for Zend\View\Helper\HeadTitle.
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class HeadTitleTest extends TestCase
{
    /**
     * @var Helper\HeadTitle
     */
    public $helper;

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
        $this->helper = new Helper\HeadTitle();
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

    public function testHeadTitleReturnsObjectInstance()
    {
        $placeholder = $this->helper->__invoke();
        $this->assertInstanceOf(Helper\HeadTitle::class, $placeholder);
    }

    public function testCanSetTitleViaHeadTitle()
    {
        $placeholder = $this->helper->__invoke('Foo Bar', 'SET');
        $this->assertEquals('Foo Bar', $placeholder->renderTitle());
    }

    public function testToStringWrapsToTitleTag()
    {
        $placeholder = $this->helper->__invoke('Foo Bar', 'SET');
        $this->assertEquals('<title>Foo Bar</title>', $placeholder->toString());
    }

    public function testCanAppendTitleViaHeadTitle()
    {
        $placeholder = $this->helper->__invoke('Foo');
        $placeholder = $this->helper->__invoke('Bar');
        $this->assertEquals('FooBar', $placeholder->renderTitle());
    }

    public function testCanPrependTitleViaHeadTitle()
    {
        $placeholder = $this->helper->__invoke('Foo');
        $placeholder = $this->helper->__invoke('Bar', 'PREPEND');
        $this->assertEquals('BarFoo', $placeholder->renderTitle());
    }

    public function testReturnedPlaceholderRenderTitleContainsFullTitleElement()
    {
        $placeholder = $this->helper->__invoke('Foo');
        $placeholder = $this->helper->__invoke('Bar', 'APPEND')->setSeparator(' :: ');
        $this->assertEquals('Foo :: Bar', $placeholder->renderTitle());
    }

    public function testRenderTitleEscapesEntries()
    {
        $this->helper->__invoke('<script type="text/javascript">alert("foo");</script>');
        $string = $this->helper->renderTitle();
        $this->assertNotContains('<script', $string);
        $this->assertNotContains('</script>', $string);
    }

    public function testRenderTitleEscapesSeparator()
    {
        $this->helper->__invoke('Foo')
                     ->__invoke('Bar')
                     ->setSeparator(' <br /> ');
        $string = $this->helper->renderTitle();
        $this->assertNotContains('<br />', $string);
        $this->assertContains('Foo', $string);
        $this->assertContains('Bar', $string);
        $this->assertContains('br /', $string);
    }

    public function testIndentationIsHonored()
    {
        $this->helper->setIndent(4);
        $this->helper->__invoke('foo');
        $string = $this->helper->toString();

        $this->assertContains('    <title>', $string);
    }

    public function testAutoEscapeIsHonored()
    {
        $this->helper->__invoke('Some Title &copyright;');
        $this->assertEquals('Some Title &amp;copyright;', $this->helper->renderTitle());

        $this->assertTrue($this->helper->__invoke()->getAutoEscape());
        $this->helper->__invoke()->setAutoEscape(false);
        $this->assertFalse($this->helper->__invoke()->getAutoEscape());


        $this->assertEquals('Some Title &copyright;', $this->helper->renderTitle());
    }

    /**
     * @issue ZF-2918
     * @link http://framework.zend.com/issues/browse/ZF-2918
     */
    public function testZF2918()
    {
        $this->helper->__invoke('Some Title');
        $this->helper->setPrefix('Prefix: ');
        $this->helper->setPostfix(' :Postfix');

        $this->assertEquals('Prefix: Some Title :Postfix', $this->helper->renderTitle());
    }

    /**
     * @issue ZF-3577
     * @link http://framework.zend.com/issues/browse/ZF-3577
     */
    public function testZF3577()
    {
        $this->helper->setAutoEscape(true);
        $this->helper->__invoke('Some Title');
        $this->helper->setPrefix('Prefix & ');
        $this->helper->setPostfix(' & Postfix');

        $this->assertEquals('Prefix &amp; Some Title &amp; Postfix', $this->helper->renderTitle());
    }

    public function testCanTranslateTitle()
    {
        $this->markTestIncomplete('Re-enable after zend-i18n is updated to zend-servicemanager v3');

        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $loader = new TestAsset\ArrayTranslator();
        $loader->translations = [
            'Message_1' => 'Message 1 (en)',
        ];
        $translator = new Translator();
        $translator->getPluginManager()->setService('default', $loader);
        $translator->addTranslationFile('default', null);

        $this->helper->setTranslatorEnabled(true);
        $this->helper->setTranslator($translator);
        $this->helper->__invoke('Message_1');
        $this->assertEquals('Message 1 (en)', $this->helper->renderTitle());
    }

    public function testTranslatorMethods()
    {
        $translatorMock = $this->prophesize(Translator::class)->reveal();
        $this->helper->setTranslator($translatorMock, 'foo');

        $this->assertEquals($translatorMock, $this->helper->getTranslator());
        $this->assertEquals('foo', $this->helper->getTranslatorTextDomain());
        $this->assertTrue($this->helper->hasTranslator());
        $this->assertTrue($this->helper->isTranslatorEnabled());

        $this->helper->setTranslatorEnabled(false);
        $this->assertFalse($this->helper->isTranslatorEnabled());
    }

    /**
     * @group ZF-8036
     */
    public function testHeadTitleZero()
    {
        $this->helper->__invoke('0');
        $this->assertEquals('0', $this->helper->renderTitle());
    }

    public function testCanPrependTitlesUsingDefaultAttachOrder()
    {
        $this->helper->setDefaultAttachOrder('PREPEND');
        $placeholder = $this->helper->__invoke('Foo');
        $placeholder = $this->helper->__invoke('Bar');
        $this->assertEquals('BarFoo', $placeholder->renderTitle());
    }


    /**
     *  @group ZF-10284
     */
    public function testReturnTypeDefaultAttachOrder()
    {
        $this->assertInstanceOf(Helper\HeadTitle::class, $this->helper->setDefaultAttachOrder('PREPEND'));
        $this->assertEquals('PREPEND', $this->helper->getDefaultAttachOrder());
    }
}
