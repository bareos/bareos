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
use Zend\Log\Processor\RequestId;

class RequestIdTest extends TestCase
{
    public function testProcess()
    {
        $processor = new RequestId();

        $event = [
            'timestamp'    => '',
            'priority'     => 1,
            'priorityName' => 'ALERT',
            'message'      => 'foo',
            'extra'        => [],
        ];

        $eventA = $processor->process($event);
        $this->assertArrayHasKey('requestId', $eventA['extra']);

        $eventB = $processor->process($event);
        $this->assertArrayHasKey('requestId', $eventB['extra']);

        $this->assertEquals($eventA['extra']['requestId'], $eventB['extra']['requestId']);
    }

    public function testProcessDoesNotOverwriteExistingRequestId()
    {
        $processor = new RequestId();

        $requestId = 'bar';

        $event = [
            'timestamp'    => '',
            'priority'     => 1,
            'priorityName' => 'ALERT',
            'message'      => 'foo',
            'extra'        => [
                'requestId' => $requestId,
            ],
        ];

        $processedEvent = $processor->process($event);

        $this->assertArrayHasKey('requestId', $processedEvent['extra']);
        $this->assertSame($requestId, $processedEvent['extra']['requestId']);
    }
}
