<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Console\Prompt;

use PHPUnit\Framework\TestCase;
use Prophecy\Prophecy\ObjectProphecy;
use Zend\Console\Adapter\AbstractAdapter;
use Zend\Console\Prompt\Password;

/**
 * Tests for {@see \Zend\Console\Prompt\Password}
 *
 * @covers \Zend\Console\Prompt\Password
 */
class PasswordTest extends TestCase
{
    /**
     * @var AbstractAdapter|ObjectProphecy
     */
    protected $adapter;

    public function setUp()
    {
        $this->adapter = $this->prophesize(AbstractAdapter::class);
    }

    public function testCanPromptPassword()
    {
        $this->adapter->writeLine('Password: ')->shouldBeCalledTimes(1);
        $this->adapter->readChar()->willReturn('f', 'o', 'o', PHP_EOL)->shouldBeCalledTimes(4);
        $this->adapter->clearLine()->willReturn(null);
        $this->adapter->write()->shouldNotBeCalled();

        $char = new Password('Password: ');

        $char->setConsole($this->adapter->reveal());

        $this->assertEquals('foo', $char->show());
    }

    public function testCanPromptPasswordRepeatedly()
    {
        $this->adapter->writeLine('New password? ')->shouldBeCalledTimes(2);
        $this->adapter->readChar()->willReturn('b', 'a', 'r', PHP_EOL, 'b', 'a', 'z', PHP_EOL)->shouldBeCalledTimes(8);
        $this->adapter->clearLine()->willReturn(null);
        $this->adapter->write()->shouldNotBeCalled();

        $char = new Password('New password? ');

        $char->setConsole($this->adapter->reveal());

        $this->assertEquals('bar', $char->show());
        $this->assertEquals('baz', $char->show());
    }

    public function testProducesStarSymbolOnInput()
    {
        $this->adapter->writeLine('New password? ')->shouldBeCalledTimes(1);
        $this->adapter->readChar()->willReturn('t', 'a', 'b', PHP_EOL)->shouldBeCalledTimes(4);
        $this->adapter->clearLine()->willReturn(null);
        $this->adapter->write('*')->shouldBeCalledTimes(1);
        $this->adapter->write('**')->shouldBeCalledTimes(1);
        $this->adapter->write('***')->shouldBeCalledTimes(1);

        $char = new Password('New password? ', true);

        $char->setConsole($this->adapter->reveal());

        $this->assertSame('tab', $char->show());
    }
}
