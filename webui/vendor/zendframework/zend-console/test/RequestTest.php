<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Console;

use PHPUnit\Framework\TestCase;
use Zend\Console\Request;

/**
 * @group      Zend_Console
 */
class RequestTest extends TestCase
{
    public function setUp()
    {
        if (ini_get('register_argc_argv') == false) {
            $this->markTestSkipped("Cannot Test Zend\\Console\\Getopt without 'register_argc_argv' ini option true.");
        }
    }

    public function testCanConstructRequestAndGetParams()
    {
        $_SERVER['argv'] = ['foo.php', 'foo' => 'baz', 'bar'];
        $_ENV["FOO_VAR"] = "bar";

        $request = new Request();
        $params = $request->getParams();

        $this->assertCount(2, $params);
        $this->assertEquals($params->toArray(), ['foo' => 'baz', 'bar']);
        $this->assertEquals($request->getParam('foo'), 'baz');
        $this->assertEquals($request->getScriptName(), 'foo.php');
        $this->assertGreaterThanOrEqual(1, count($request->env()));
        $this->assertEquals($request->env()->get('FOO_VAR'), 'bar');
        $this->assertEquals($request->getEnv('FOO_VAR'), 'bar');
    }
}
