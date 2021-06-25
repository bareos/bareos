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
use Zend\View\Helper\Placeholder\Container\AbstractContainer;
use Zend\View\Renderer\PhpRenderer as View;
use Zend\View\Helper;

/**
 * Test class for Zend\View\Helper\Placeholder.
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class PlaceholderTest extends TestCase
{
    /**
     * @var Helper\Placeholder
     */
    public $placeholder;

    /**
     * Sets up the fixture, for example, open a network connection.
     * This method is called before a test is executed.
     *
     * @return void
     */
    public function setUp()
    {
        $this->placeholder = new Helper\Placeholder();
    }

    /**
     * Tears down the fixture, for example, close a network connection.
     * This method is called after a test is executed.
     *
     * @return void
     */
    public function tearDown()
    {
        unset($this->placeholder);
    }

    /**
     * @return void
     */
    public function testSetView()
    {
        $view = new View();
        $this->placeholder->setView($view);
        $this->assertSame($view, $this->placeholder->getView());
    }

    /**
     * @return void
     */
    public function testPlaceholderRetrievesContainer()
    {
        $container = $this->placeholder->__invoke('foo');
        $this->assertInstanceOf(AbstractContainer::class, $container);
    }

    /**
     * @return void
     */
    public function testPlaceholderRetrievesSameContainerOnSubsequentCalls()
    {
        $container1 = $this->placeholder->__invoke('foo');
        $container2 = $this->placeholder->__invoke('foo');
        $this->assertSame($container1, $container2);
    }

    public function testContainersCanBeDeleted()
    {
        $container = $this->placeholder->__invoke('foo');
        $container->set('Value');
        $this->assertTrue($this->placeholder->containerExists('foo'));
        $this->assertSame('Value', (string) $this->placeholder->__invoke('foo'));
        $this->placeholder->deleteContainer('foo');
        $this->assertFalse($this->placeholder->containerExists('foo'));
        $this->assertSame('', (string) $this->placeholder->__invoke('foo'));
    }

    public function testClearContainersRemovesAllContainers()
    {
        $this->placeholder->__invoke('foo');
        $this->placeholder->__invoke('bar');

        $this->assertTrue($this->placeholder->containerExists('foo'));
        $this->assertTrue($this->placeholder->containerExists('bar'));

        $this->placeholder->clearContainers();

        $this->assertFalse($this->placeholder->containerExists('foo'));
        $this->assertFalse($this->placeholder->containerExists('bar'));
    }
}
