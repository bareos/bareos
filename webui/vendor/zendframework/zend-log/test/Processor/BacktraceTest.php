<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log\Processor;

use PHPUnit\Framework\TestCase;
use Zend\Log\Processor\Backtrace;

class BacktraceTest extends TestCase
{
    private $processor;

    protected function setUp()
    {
        $this->processor = new Backtrace();
    }

    public function testProcess()
    {
        $event = [
                'timestamp'    => '',
                'priority'     => 1,
                'priorityName' => 'ALERT',
                'message'      => 'foo',
                'extra'        => []
        ];

        $event = $this->processor->process($event);

        $this->assertArrayHasKey('file', $event['extra']);
        $this->assertArrayHasKey('line', $event['extra']);
        $this->assertArrayHasKey('class', $event['extra']);
        $this->assertArrayHasKey('function', $event['extra']);
    }

    public function testConstructorAcceptsOptionalIgnoredNamespaces()
    {
        $this->assertSame(['Zend\\Log'], $this->processor->getIgnoredNamespaces());

        $processor = new Backtrace(['ignoredNamespaces' => ['Foo\\Bar']]);
        $this->assertSame(['Zend\\Log', 'Foo\\Bar'], $processor->getIgnoredNamespaces());
    }
}
