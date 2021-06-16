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
use Zend\Log\Processor\ReferenceId;

class ReferenceIdTest extends TestCase
{
    public function testProcessMixesInReferenceId()
    {
        $processor      = new ReferenceId();
        $processedEvent = $processor->process([
            'timestamp'    => '',
            'priority'     => 1,
            'priorityName' => 'ALERT',
            'message'      => 'foo',
        ]);

        $this->assertArrayHasKey('extra', $processedEvent);
        $this->assertInternalType('array', $processedEvent['extra']);
        $this->assertArrayHasKey('referenceId', $processedEvent['extra']);

        $this->assertNotNull($processedEvent['extra']['referenceId']);
    }

    public function testProcessDoesNotOverwriteReferenceId()
    {
        $processor      = new ReferenceId();
        $referenceId    = 'bar';
        $processedEvent = $processor->process([
            'timestamp'    => '',
            'priority'     => 1,
            'priorityName' => 'ALERT',
            'message'      => 'foo',
            'extra'        => [
                'referenceId' => $referenceId,
            ],
        ]);

        $this->assertArrayHasKey('extra', $processedEvent);
        $this->assertInternalType('array', $processedEvent['extra']);
        $this->assertArrayHasKey('referenceId', $processedEvent['extra']);

        $this->assertSame($referenceId, $processedEvent['extra']['referenceId']);
    }

    public function testCanSetAndGetReferenceId()
    {
        $processor   = new ReferenceId();
        $referenceId = 'foo';

        $processor->setReferenceId($referenceId);

        $this->assertSame($referenceId, $processor->getReferenceId());
    }

    public function testProcessUsesSetReferenceId()
    {
        $referenceId = 'foo';
        $processor   = new ReferenceId();

        $processor->setReferenceId($referenceId);

        $processedEvent = $processor->process([
            'timestamp'    => '',
            'priority'     => 1,
            'priorityName' => 'ALERT',
            'message'      => 'foo',
        ]);

        $this->assertArrayHasKey('extra', $processedEvent);
        $this->assertInternalType('array', $processedEvent['extra']);
        $this->assertArrayHasKey('referenceId', $processedEvent['extra']);

        $this->assertSame($referenceId, $processedEvent['extra']['referenceId']);
    }
}
