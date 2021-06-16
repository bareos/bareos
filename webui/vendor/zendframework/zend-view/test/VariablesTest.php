<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View;

use PHPUnit\Framework\TestCase;
use Zend\View\Variables;
use Zend\Config\Config;

/**
 * @group      Zend_View
 */
class VariablesTest extends TestCase
{
    public function setup()
    {
        $this->error = false;
        $this->vars = new Variables;
    }

    public function testStrictVarsAreDisabledByDefault()
    {
        $this->assertFalse($this->vars->isStrict());
    }

    public function testCanSetStrictFlag()
    {
        $this->vars->setStrictVars(true);
        $this->assertTrue($this->vars->isStrict());
    }

    public function testAssignMergesValuesWithObject()
    {
        $this->vars['foo'] = 'bar';
        $this->vars->assign([
            'bar' => 'baz',
            'baz' => 'foo',
        ]);
        $this->assertEquals('bar', $this->vars['foo']);
        $this->assertEquals('baz', $this->vars['bar']);
        $this->assertEquals('foo', $this->vars['baz']);
    }

    public function testAssignCastsPlainObjectToArrayBeforeMerging()
    {
        $vars = new \stdClass;
        $vars->foo = 'bar';
        $vars->bar = 'baz';

        $this->vars->assign($vars);
        $this->assertEquals('bar', $this->vars['foo']);
        $this->assertEquals('baz', $this->vars['bar']);
    }

    public function testAssignCallsToArrayWhenPresentBeforeMerging()
    {
        $vars = [
            'foo' => 'bar',
            'bar' => 'baz',
        ];
        $config = new Config($vars);
        $this->vars->assign($config);
        $this->assertEquals('bar', $this->vars['foo']);
        $this->assertEquals('baz', $this->vars['bar']);
    }

    public function testNullIsReturnedForUndefinedVariables()
    {
        $this->assertNull($this->vars['foo']);
    }

    public function handleErrors($errcode, $errmsg)
    {
        $this->error = $errmsg;
    }

    public function testRetrievingUndefinedVariableRaisesErrorWhenStrictVarsIsRequested()
    {
        $this->vars->setStrictVars(true);
        set_error_handler([$this, 'handleErrors'], E_USER_NOTICE);
        $this->assertNull($this->vars['foo']);
        restore_error_handler();
        $this->assertContains('does not exist', $this->error);
    }

    public function values()
    {
        return [
            ['foo', 'bar'],
            ['xss', '<tag id="foo">\'value\'</tag>'],
        ];
    }

    public function testCallingClearEmptiesObject()
    {
        $this->vars->assign([
            'bar' => 'baz',
            'baz' => 'foo',
        ]);
        $this->assertEquals(2, count($this->vars));
        $this->vars->clear();
        $this->assertEquals(0, count($this->vars));
    }

    public function testAllowsSpecifyingClosureValuesAndReturningTheValue()
    {
        $this->vars->foo = function () {
            return 'bar';
        };

        $this->assertEquals('bar', $this->vars->foo);
    }

    public function testAllowsSpecifyingFunctorValuesAndReturningTheValue()
    {
        $this->vars->foo = new TestAsset\VariableFunctor('bar');
        $this->assertEquals('bar', $this->vars->foo);
    }
}
