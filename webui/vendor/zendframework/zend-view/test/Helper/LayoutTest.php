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
use Zend\View\Helper\Layout;
use Zend\View\Model\ViewModel;
use Zend\View\Renderer\PhpRenderer;

/**
 * Test class for Zend\View\Helper\Layout
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class LayoutTest extends TestCase
{
    /**
     * Sets up the fixture, for example, open a network connection.
     * This method is called before a test is executed.
     *
     * @return void
     */
    public function setUp()
    {
        $this->renderer = $renderer = new PhpRenderer();
        $this->viewModelHelper = $renderer->plugin('view_model');
        $this->helper          = $renderer->plugin('layout');

        $this->parent = new ViewModel();
        $this->parent->setTemplate('layout');
        $this->viewModelHelper->setRoot($this->parent);
    }

    public function testCallingSetTemplateAltersRootModelTemplate()
    {
        $this->helper->setTemplate('alternate/layout');
        $this->assertEquals('alternate/layout', $this->parent->getTemplate());
    }

    public function testCallingGetLayoutReturnsRootModelTemplate()
    {
        $this->assertEquals('layout', $this->helper->getLayout());
    }

    public function testCallingInvokeProxiesToSetTemplate()
    {
        $helper = $this->helper;
        $helper('alternate/layout');
        $this->assertEquals('alternate/layout', $this->parent->getTemplate());
    }

    public function testCallingInvokeWithNoArgumentReturnsViewModel()
    {
        $helper = $this->helper;
        $result = $helper();
        $this->assertSame($this->parent, $result);
    }

    public function testRaisesExceptionIfViewModelHelperHasNoRoot()
    {
        $renderer         = new PhpRenderer();
        $viewModelHelper = $renderer->plugin('view_model');
        $helper          = $renderer->plugin('layout');

        $this->expectException(Exception\RuntimeException::class);
        $this->expectExceptionMessage('view model');
        $helper->setTemplate('foo/bar');
    }
}
