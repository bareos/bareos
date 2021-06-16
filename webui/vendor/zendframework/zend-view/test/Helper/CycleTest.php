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
use Zend\View\Helper;

/**
 * Test class for Zend\View\Helper\Cycle.
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class CycleTest extends TestCase
{
    /**
     * @var Helper\Cycle
     */
    public $helper;

    /**
     * Sets up the fixture, for example, open a network connection.
     * This method is called before a test is executed.
     *
     * @return void
     */
    public function setUp()
    {
        $this->helper = new Helper\Cycle();
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

    public function testCycleMethodReturnsObjectInstance()
    {
        $cycle = $this->helper->__invoke();
        $this->assertInstanceOf(Helper\Cycle::class, $cycle);
    }

    public function testAssignAndGetValues()
    {
        $this->helper->assign(['a', 1, 'asd']);
        $this->assertEquals(['a', 1, 'asd'], $this->helper->getAll());
    }

    public function testCycleMethod()
    {
        $this->helper->__invoke(['a', 1, 'asd']);
        $this->assertEquals(['a', 1, 'asd'], $this->helper->getAll());
    }

    public function testToString()
    {
        $this->helper->__invoke(['a', 1, 'asd']);
        $this->assertEquals('a', (string) $this->helper->toString());
    }

    public function testNextValue()
    {
        $this->helper->assign(['a', 1, 3]);
        $this->assertEquals('a', (string) $this->helper->next());
        $this->assertEquals(1, (string) $this->helper->next());
        $this->assertEquals(3, (string) $this->helper->next());
        $this->assertEquals('a', (string) $this->helper->next());
        $this->assertEquals(1, (string) $this->helper->next());
    }

    public function testPrevValue()
    {
        $this->helper->assign([4, 1, 3]);
        $this->assertEquals(3, (string) $this->helper->prev());
        $this->assertEquals(1, (string) $this->helper->prev());
        $this->assertEquals(4, (string) $this->helper->prev());
        $this->assertEquals(3, (string) $this->helper->prev());
        $this->assertEquals(1, (string) $this->helper->prev());
    }

    public function testRewind()
    {
        $this->helper->assign([5, 8, 3]);
        $this->assertEquals(5, (string) $this->helper->next());
        $this->assertEquals(8, (string) $this->helper->next());
        $this->helper->rewind();
        $this->assertEquals(5, (string) $this->helper->next());
        $this->assertEquals(8, (string) $this->helper->next());
    }

    public function testMixedMethods()
    {
        $this->helper->assign([5, 8, 3]);
        $this->assertEquals(5, (string) $this->helper->next());
        $this->assertEquals(5, (string) $this->helper->current());
        $this->assertEquals(8, (string) $this->helper->next());
        $this->assertEquals(5, (string) $this->helper->prev());
    }

    public function testTwoCycles()
    {
        $this->helper->assign([5, 8, 3]);
        $this->assertEquals(5, (string) $this->helper->next());
        $this->assertEquals(2, (string) $this->helper->__invoke([2, 38, 1], 'cycle2')->next());
        $this->assertEquals(8, (string) $this->helper->__invoke()->next());
        $this->assertEquals(38, (string) $this->helper->setName('cycle2')->next());
    }

    public function testTwoCyclesInLoop()
    {
        $expected = [5,4,2,3];
        $expected2 = [7,34,8,6];
        for ($i = 0; $i < 4; $i++) {
            $this->assertEquals($expected[$i], (string) $this->helper->__invoke($expected)->next());
            $this->assertEquals($expected2[$i], (string) $this->helper->__invoke($expected2, 'cycle2')->next());
        }
    }
}
