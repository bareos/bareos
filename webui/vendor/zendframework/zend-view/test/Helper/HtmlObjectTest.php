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
use Zend\View\Renderer\PhpRenderer as View;
use Zend\View\Helper\Doctype;
use Zend\View\Helper\HtmlObject;
use Zend\View\Renderer\RendererInterface;

/**
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class HtmlObjectTest extends TestCase
{
    /**
     * @var HtmlObject
     */
    public $helper;

    /**
     * Sets up the fixture, for example, open a network connection.
     * This method is called before a test is executed.
     *
     * @access protected
     */
    protected function setUp()
    {
        $this->view   = new View();
        $this->helper = new HtmlObject();
        $this->helper->setView($this->view);
    }

    public function tearDown()
    {
        unset($this->helper);
    }

    public function testViewObjectIsSet()
    {
        $this->assertInstanceof(RendererInterface::class, $this->helper->getView());
    }

    public function testMakeHtmlObjectWithoutAttribsWithoutParams()
    {
        $htmlObject = $this->helper->__invoke('datastring', 'typestring');

        $this->assertContains('<object data="datastring" type="typestring">', $htmlObject);
        $this->assertContains('</object>', $htmlObject);
    }

    public function testMakeHtmlObjectWithAttribsWithoutParams()
    {
        $attribs = ['attribkey1' => 'attribvalue1',
                         'attribkey2' => 'attribvalue2'];

        $htmlObject = $this->helper->__invoke('datastring', 'typestring', $attribs);

        $this->assertContains(
            '<object data="datastring" type="typestring" attribkey1="attribvalue1" attribkey2="attribvalue2">',
            $htmlObject
        );
        $this->assertContains('</object>', $htmlObject);
    }

    public function testMakeHtmlObjectWithoutAttribsWithParamsHtml()
    {
        $this->view->plugin('doctype')->__invoke(Doctype::HTML4_STRICT);

        $params = ['paramname1' => 'paramvalue1',
                        'paramname2' => 'paramvalue2'];

        $htmlObject = $this->helper->__invoke('datastring', 'typestring', [], $params);

        $this->assertContains('<object data="datastring" type="typestring">', $htmlObject);
        $this->assertContains('</object>', $htmlObject);

        foreach ($params as $key => $value) {
            $param = '<param name="' . $key . '" value="' . $value . '">';

            $this->assertContains($param, $htmlObject);
        }
    }

    public function testMakeHtmlObjectWithoutAttribsWithParamsXhtml()
    {
        $this->view->plugin('doctype')->__invoke(Doctype::XHTML1_STRICT);

        $params = ['paramname1' => 'paramvalue1',
                        'paramname2' => 'paramvalue2'];

        $htmlObject = $this->helper->__invoke('datastring', 'typestring', [], $params);

        $this->assertContains('<object data="datastring" type="typestring">', $htmlObject);
        $this->assertContains('</object>', $htmlObject);

        foreach ($params as $key => $value) {
            $param = '<param name="' . $key . '" value="' . $value . '" />';

            $this->assertContains($param, $htmlObject);
        }
    }

    public function testMakeHtmlObjectWithContent()
    {
        $htmlObject = $this->helper->__invoke('datastring', 'typestring', [], [], 'testcontent');

        $this->assertContains('<object data="datastring" type="typestring">', $htmlObject);
        $this->assertContains('testcontent', $htmlObject);
        $this->assertContains('</object>', $htmlObject);
    }
}
