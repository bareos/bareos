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
use Zend\View\Helper\HtmlFlash;

/**
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class HtmlFlashTest extends TestCase
{
    /**
     * @var HtmlFlash
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
        $this->helper = new HtmlFlash();
        $this->helper->setView($this->view);
    }

    public function tearDown()
    {
        unset($this->helper);
    }

    public function testMakeHtmlFlash()
    {
        $htmlFlash = $this->helper->__invoke('/path/to/flash.swf');

        // @codingStandardsIgnoreStart
        $objectStartElement = '<object data="&#x2F;path&#x2F;to&#x2F;flash.swf" type="application&#x2F;x-shockwave-flash">';
        // @codingStandardsIgnoreEnd

        $this->assertContains($objectStartElement, $htmlFlash);
        $this->assertContains('</object>', $htmlFlash);
    }
}
