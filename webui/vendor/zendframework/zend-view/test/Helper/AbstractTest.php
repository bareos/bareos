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
use Zend\View\Renderer\RendererInterface;
use ZendTest\View\Helper\TestAsset\ConcreteHelper;

/**
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class AbstractTest extends TestCase
{
    /**
     * @var ConcreteHelper
     */
    protected $helper;

    public function setUp()
    {
        $this->helper = new ConcreteHelper();
    }

    public function testViewSettersGetters()
    {
        $viewMock = $this->prophesize(RendererInterface::class)->reveal();

        $this->helper->setView($viewMock);
        $this->assertEquals($viewMock, $this->helper->getView());
    }
}
