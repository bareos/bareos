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
use Zend\Console\Prompt\Number;
use ZendTest\Console\TestAssets\ConsoleAdapter;

/**
 * @group      Zend_Console
 */
class NumberTest extends TestCase
{
    /**
     * @var ConsoleAdapter
     */
    protected $adapter;

    public function setUp()
    {
        $this->adapter = new ConsoleAdapter();
        $this->adapter->stream = fopen('php://memory', 'w+');
    }

    public function tearDown()
    {
        fclose($this->adapter->stream);
    }

    public function testCanReadNumber()
    {
        fwrite($this->adapter->stream, "123");

        $number = new Number();
        $number->setConsole($this->adapter);
        ob_start();
        $response = $number->show();
        $text = ob_get_clean();
        $this->assertEquals($text, "Please enter a number: ");
        $this->assertEquals('123', $response);
    }

    public function testCanReadNumberOnMultilign()
    {
        fwrite($this->adapter->stream, "a" . PHP_EOL);
        fwrite($this->adapter->stream, "123" . PHP_EOL);
        rewind($this->adapter->stream);
        $this->adapter->autoRewind = false;

        $number = new Number();
        $number->setConsole($this->adapter);
        ob_start();
        $response = $number->show();
        $text = ob_get_clean();
        $this->assertContains('a is not a number', $text);
        $this->assertEquals('123', $response);
    }

    public function testCanNotReadFloatByDefault()
    {
        fwrite($this->adapter->stream, "1.23" . PHP_EOL);
        fwrite($this->adapter->stream, "123" . PHP_EOL);
        rewind($this->adapter->stream);
        $this->adapter->autoRewind = false;

        $number = new Number();
        $number->setConsole($this->adapter);
        ob_start();
        $response = $number->show();
        $text = ob_get_clean();
        $this->assertContains('Please enter a non-floating number', $text);
        $this->assertEquals('123', $response);
    }

    public function testCanForceToReadFloat()
    {
        fwrite($this->adapter->stream, "1.23" . PHP_EOL);
        fwrite($this->adapter->stream, "123" . PHP_EOL);
        rewind($this->adapter->stream);
        $this->adapter->autoRewind = false;

        $number = new Number('Give me a number', false, true);
        $number->setConsole($this->adapter);
        ob_start();
        $response = $number->show();
        $text = ob_get_clean();
        $this->assertEquals($text, 'Give me a number');
        $this->assertEquals('1.23', $response);
    }

    public function testCanDefineAMax()
    {
        fwrite($this->adapter->stream, "1" . PHP_EOL);
        fwrite($this->adapter->stream, "11" . PHP_EOL);
        fwrite($this->adapter->stream, "6" . PHP_EOL);
        rewind($this->adapter->stream);
        $this->adapter->autoRewind = false;

        $number = new Number('Give me a number', false, false, 5, 10);
        $number->setConsole($this->adapter);
        ob_start();
        $response = $number->show();
        $text = ob_get_clean();

        $this->assertContains('Please enter a number not smaller than 5', $text);
        $this->assertContains('Please enter a number not greater than 10', $text);
        $this->assertEquals('6', $response);
    }
}
