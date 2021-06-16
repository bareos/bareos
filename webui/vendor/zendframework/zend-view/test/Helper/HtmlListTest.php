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

/**
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class HtmlListTest extends TestCase
{
    /**
     * @var Helper\HtmlList
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
        $this->helper = new Helper\HtmlList();
        $this->helper->setView($this->view);
    }

    public function tearDown()
    {
        unset($this->helper);
    }

    public function testMakeUnorderedList()
    {
        $items = ['one', 'two', 'three'];

        $list = $this->helper->__invoke($items);

        $this->assertContains('<ul>', $list);
        $this->assertContains('</ul>', $list);
        foreach ($items as $item) {
            $this->assertContains('<li>' . $item . '</li>', $list);
        }
    }

    public function testMakeOrderedList()
    {
        $items = ['one', 'two', 'three'];

        $list = $this->helper->__invoke($items, true);

        $this->assertContains('<ol>', $list);
        $this->assertContains('</ol>', $list);
        foreach ($items as $item) {
            $this->assertContains('<li>' . $item . '</li>', $list);
        }
    }

    public function testMakeUnorderedListWithAttribs()
    {
        $items = ['one', 'two', 'three'];
        $attribs = ['class' => 'selected', 'name' => 'list'];

        $list = $this->helper->__invoke($items, false, $attribs);

        $this->assertContains('<ul', $list);
        $this->assertContains('class="selected"', $list);
        $this->assertContains('name="list"', $list);
        $this->assertContains('</ul>', $list);
        foreach ($items as $item) {
            $this->assertContains('<li>' . $item . '</li>', $list);
        }
    }

    public function testMakeOrderedListWithAttribs()
    {
        $items = ['one', 'two', 'three'];
        $attribs = ['class' => 'selected', 'name' => 'list'];

        $list = $this->helper->__invoke($items, true, $attribs);

        $this->assertContains('<ol', $list);
        $this->assertContains('class="selected"', $list);
        $this->assertContains('name="list"', $list);
        $this->assertContains('</ol>', $list);
        foreach ($items as $item) {
            $this->assertContains('<li>' . $item . '</li>', $list);
        }
    }

    /*
     * @group ZF-5018
     */
    public function testMakeNestedUnorderedList()
    {
        $items = ['one', ['four', 'five', 'six'], 'two', 'three'];

        $list = $this->helper->__invoke($items);

        $this->assertContains('<ul>' . PHP_EOL, $list);
        $this->assertContains('</ul>' . PHP_EOL, $list);
        $this->assertContains('one<ul>' . PHP_EOL.'<li>four', $list);
        $this->assertContains('<li>six</li>' . PHP_EOL . '</ul>' .
            PHP_EOL . '</li>' . PHP_EOL . '<li>two', $list);
    }

    /*
     * @group ZF-5018
     */
    public function testMakeNestedDeepUnorderedList()
    {
        $items = ['one', ['four', ['six', 'seven', 'eight'], 'five'], 'two', 'three'];

        $list = $this->helper->__invoke($items);

        $this->assertContains('<ul>' . PHP_EOL, $list);
        $this->assertContains('</ul>' . PHP_EOL, $list);
        $this->assertContains('one<ul>' . PHP_EOL . '<li>four', $list);
        $this->assertContains('<li>four<ul>' . PHP_EOL . '<li>six', $list);
        $this->assertContains('<li>five</li>' . PHP_EOL . '</ul>' .
            PHP_EOL . '</li>' . PHP_EOL . '<li>two', $list);
    }

    public function testListWithValuesToEscapeForZF2283()
    {
        $items = ['one <small> test', 'second & third', 'And \'some\' "final" test'];

        $list = $this->helper->__invoke($items);

        $this->assertContains('<ul>', $list);
        $this->assertContains('</ul>', $list);

        $this->assertContains('<li>one &lt;small&gt; test</li>', $list);
        $this->assertContains('<li>second &amp; third</li>', $list);
        $this->assertContains('<li>And &#039;some&#039; &quot;final&quot; test</li>', $list);
    }

    public function testListEscapeSwitchedOffForZF2283()
    {
        $items = ['one <b>small</b> test'];

        $list = $this->helper->__invoke($items, false, false, false);

        $this->assertContains('<ul>', $list);
        $this->assertContains('</ul>', $list);

        $this->assertContains('<li>one <b>small</b> test</li>', $list);
    }

    /**
     * @group ZF-2527
     */
    public function testEscapeFlagHonoredForMultidimensionalLists()
    {
        $items = ['<b>one</b>', ['<b>four</b>', '<b>five</b>', '<b>six</b>'], '<b>two</b>', '<b>three</b>'];

        $list = $this->helper->__invoke($items, false, false, false);

        foreach ($items[1] as $item) {
            $this->assertContains($item, $list);
        }
    }

    /**
     * @group ZF-2527
     * Added the s modifier to match newlines after ZF-5018
     */
    public function testAttribsPassedIntoMultidimensionalLists()
    {
        $items = ['one', ['four', 'five', 'six'], 'two', 'three'];

        $list = $this->helper->__invoke($items, false, ['class' => 'foo']);

        foreach ($items[1] as $item) {
            $this->assertRegexp('#<ul[^>]*?class="foo"[^>]*>.*?(<li>' . $item . ')#s', $list);
        }
    }

    /**
     * @group ZF-2870
     */
    public function testEscapeFlagShouldBePassedRecursively()
    {
        $items = [
            '<b>one</b>',
            [
                '<b>four</b>',
                '<b>five</b>',
                '<b>six</b>',
                [
                    '<b>two</b>',
                    '<b>three</b>',
                ],
            ],
        ];

        $list = $this->helper->__invoke($items, false, false, false);

        $this->assertContains('<ul>', $list);
        $this->assertContains('</ul>', $list);

        array_walk_recursive($items, [$this, 'validateItems'], $list);
    }

    public function validateItems($value, $key, $userdata)
    {
        $this->assertContains('<li>' . $value, $userdata);
    }

    /**
     * @group ZF2-6063
     */
    public function testEmptyItems()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->helper->__invoke([]);
    }
}
