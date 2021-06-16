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
 * Test class for Zend\View\Helper\Doctype.
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class DoctypeTest extends TestCase
{
    /**
     * @var Helper\Doctype
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
        Helper\Doctype::unsetDoctypeRegistry();
        $this->helper = new Helper\Doctype();
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

    public function testDoctypeMethodReturnsObjectInstance()
    {
        $doctype = $this->helper->__invoke();
        $this->assertInstanceOf(Helper\Doctype::class, $doctype);
    }

    public function testPassingDoctypeSetsDoctype()
    {
        $doctype = $this->helper->__invoke(Helper\Doctype::XHTML1_STRICT);
        $this->assertEquals(Helper\Doctype::XHTML1_STRICT, $doctype->getDoctype());
    }

    public function testIsXhtmlReturnsTrueForXhtmlDoctypes()
    {
        $types = [
            Helper\Doctype::XHTML1_STRICT,
            Helper\Doctype::XHTML1_TRANSITIONAL,
            Helper\Doctype::XHTML1_FRAMESET,
            Helper\Doctype::XHTML1_RDFA,
            Helper\Doctype::XHTML1_RDFA11,
            Helper\Doctype::XHTML5
        ];

        foreach ($types as $type) {
            $doctype = $this->helper->__invoke($type);
            $this->assertEquals($type, $doctype->getDoctype());
            $this->assertTrue($doctype->isXhtml());
        }

        // @codingStandardsIgnoreStart
        $doctype = $this->helper->__invoke('<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://framework.zend.com/foo/DTD/xhtml1-custom.dtd">');
        // @codingStandardsIgnoreEnd
        $this->assertEquals('CUSTOM_XHTML', $doctype->getDoctype());
        $this->assertTrue($doctype->isXhtml());
    }

    public function testIsXhtmlReturnsFalseForNonXhtmlDoctypes()
    {
        $types = [
            Helper\Doctype::HTML4_STRICT,
            Helper\Doctype::HTML4_LOOSE,
            Helper\Doctype::HTML4_FRAMESET,
        ];

        foreach ($types as $type) {
            $doctype = $this->helper->__invoke($type);
            $this->assertEquals($type, $doctype->getDoctype());
            $this->assertFalse($doctype->isXhtml());
        }

        // @codingStandardsIgnoreStart
        $doctype = $this->helper->__invoke('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 10.0 Strict//EN" "http://framework.zend.com/foo/DTD/html10-custom.dtd">');
        // @codingStandardsIgnoreEnd
        $this->assertEquals('CUSTOM', $doctype->getDoctype());
        $this->assertFalse($doctype->isXhtml());
    }

    public function testIsHtml5()
    {
        foreach ([Helper\Doctype::HTML5, Helper\Doctype::XHTML5] as $type) {
            $doctype = $this->helper->__invoke($type);
            $this->assertEquals($type, $doctype->getDoctype());
            $this->assertTrue($doctype->isHtml5());
        }

        $types = [
            Helper\Doctype::HTML4_STRICT,
            Helper\Doctype::HTML4_LOOSE,
            Helper\Doctype::HTML4_FRAMESET,
            Helper\Doctype::XHTML1_STRICT,
            Helper\Doctype::XHTML1_TRANSITIONAL,
            Helper\Doctype::XHTML1_FRAMESET
        ];


        foreach ($types as $type) {
            $doctype = $this->helper->__invoke($type);
            $this->assertEquals($type, $doctype->getDoctype());
            $this->assertFalse($doctype->isHtml5());
        }
    }

    public function testIsRdfa()
    {
        // ensure default registered Doctype is false
        $this->assertFalse($this->helper->isRdfa());

        $this->assertTrue($this->helper->__invoke(Helper\Doctype::XHTML1_RDFA)->isRdfa());
        $this->assertTrue($this->helper->__invoke(Helper\Doctype::XHTML1_RDFA11)->isRdfa());
        $this->assertTrue($this->helper->__invoke(Helper\Doctype::XHTML5)->isRdfa());
        $this->assertTrue($this->helper->__invoke(Helper\Doctype::HTML5)->isRdfa());

        // build-in doctypes
        $doctypes = [
            Helper\Doctype::XHTML11,
            Helper\Doctype::XHTML1_STRICT,
            Helper\Doctype::XHTML1_TRANSITIONAL,
            Helper\Doctype::XHTML1_FRAMESET,
            Helper\Doctype::XHTML_BASIC1,
            Helper\Doctype::HTML4_STRICT,
            Helper\Doctype::HTML4_LOOSE,
            Helper\Doctype::HTML4_FRAMESET,
        ];

        foreach ($doctypes as $type) {
            $this->assertFalse($this->helper->__invoke($type)->isRdfa());
        }

        // @codingStandardsIgnoreStart
        // custom doctype
        $doctype = $this->helper->__invoke('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 10.0 Strict//EN" "http://framework.zend.com/foo/DTD/html10-custom.dtd">');
        // @codingStandardsIgnoreEnd
        $this->assertFalse($doctype->isRdfa());
    }

    public function testCanRegisterCustomHtml5Doctype()
    {
        $doctype = $this->helper->__invoke('<!DOCTYPE html>');
        $this->assertEquals('CUSTOM', $doctype->getDoctype());
        $this->assertTrue($doctype->isHtml5());
    }

    public function testCanRegisterCustomXhtmlDoctype()
    {
        // @codingStandardsIgnoreStart
        $doctype = $this->helper->__invoke('<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://framework.zend.com/foo/DTD/xhtml1-custom.dtd">');
        // @codingStandardsIgnoreEnd
        $this->assertEquals('CUSTOM_XHTML', $doctype->getDoctype());
        $this->assertTrue($doctype->isXhtml());
    }

    public function testCanRegisterCustomHtmlDoctype()
    {
        // @codingStandardsIgnoreStart
        $doctype = $this->helper->__invoke('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 10.0 Strict//EN" "http://framework.zend.com/foo/DTD/html10-custom.dtd">');
        // @codingStandardsIgnoreEnd
        $this->assertEquals('CUSTOM', $doctype->getDoctype());
        $this->assertFalse($doctype->isXhtml());
    }

    public function testMalformedCustomDoctypeRaisesException()
    {
        try {
            $doctype = $this->helper->__invoke('<!FOO HTML>');
            $this->fail('Malformed doctype should raise exception');
        } catch (\Exception $e) {
        }
    }

    public function testStringificationReturnsDoctypeString()
    {
        $doctype = $this->helper->__invoke(Helper\Doctype::XHTML1_STRICT);
        $string   = $doctype->__toString();
        // @codingStandardsIgnoreStart
        $this->assertEquals('<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">', $string);
        // @codingStandardsIgnoreEnd
    }
}
