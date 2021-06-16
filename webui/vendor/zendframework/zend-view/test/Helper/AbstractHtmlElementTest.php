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
use Zend\View\Helper\AbstractHtmlElement;
use Zend\View\Renderer\PhpRenderer;

/**
 * Tests for {@see \Zend\View\Helper\AbstractHtmlElement}
 *
 * @covers \Zend\View\Helper\AbstractHtmlElement
 */
class AbstractHtmlElementTest extends TestCase
{
    /**
     * @var AbstractHtmlElement|\PHPUnit_Framework_MockObject_MockObject
     */
    protected $helper;

    /**
     * {@inheritDoc}
     */
    public function setUp()
    {
        $this->helper = $this->getMockForAbstractClass(AbstractHtmlElement::class);

        $this->helper->setView(new PhpRenderer());
    }

    /**
     * @group 5991
     */
    public function testWillEscapeValueAttributeValuesCorrectly()
    {
        $reflectionMethod = new \ReflectionMethod($this->helper, 'htmlAttribs');

        $reflectionMethod->setAccessible(true);

        $this->assertSame(
            ' data-value="breaking&#x20;your&#x20;HTML&#x20;like&#x20;a&#x20;boss&#x21;&#x20;&#x5C;"',
            $reflectionMethod->invoke($this->helper, ['data-value' => 'breaking your HTML like a boss! \\'])
        );
    }
}
