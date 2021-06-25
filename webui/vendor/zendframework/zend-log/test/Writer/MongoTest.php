<?php

/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log\Writer;

use DateTime;
use MongoDate;
use PHPUnit\Framework\TestCase;
use Zend\Log\Writer\Mongo as MongoWriter;

class MongoTest extends TestCase
{
    protected function setUp()
    {
        if (! extension_loaded('mongo')) {
            $this->markTestSkipped('The mongo PHP extension is not available');
        }

        $this->database = 'zf2_test';
        $this->collection = 'logs';

        $mongoClass = version_compare(phpversion('mongo'), '1.3.0', '<') ? 'Mongo' : 'MongoClient';

        $this->mongo = $this->getMockBuilder($mongoClass)
            ->disableOriginalConstructor()
            ->setMethods(['selectCollection'])
            ->getMock();

        $this->mongoCollection = $this->getMockBuilder('MongoCollection')
            ->disableOriginalConstructor()
            ->setMethods(['save'])
            ->getMock();

        $this->mongo->expects($this->any())
            ->method('selectCollection')
            ->with($this->database, $this->collection)
            ->will($this->returnValue($this->mongoCollection));
    }

    public function testFormattingIsNotSupported()
    {
        $writer = new MongoWriter($this->mongo, $this->database, $this->collection);

        $writer->setFormatter($this->createMock('Zend\Log\Formatter\FormatterInterface'));
        $this->assertAttributeEmpty('formatter', $writer);
    }

    public function testWriteWithDefaultSaveOptions()
    {
        $event = ['message' => 'foo', 'priority' => 42];

        $this->mongoCollection->expects($this->once())
            ->method('save')
            ->with($event, []);

        $writer = new MongoWriter($this->mongo, $this->database, $this->collection);

        $writer->write($event);
    }

    public function testWriteWithCustomSaveOptions()
    {
        $event = ['message' => 'foo', 'priority' => 42];
        $saveOptions = ['safe' => false, 'fsync' => false, 'timeout' => 100];

        $this->mongoCollection->expects($this->once())
            ->method('save')
            ->with($event, $saveOptions);

        $writer = new MongoWriter($this->mongo, $this->database, $this->collection, $saveOptions);

        $writer->write($event);
    }

    public function testWriteConvertsDateTimeToMongoDate()
    {
        $date = new DateTime();
        $event = ['timestamp' => $date];

        $this->mongoCollection->expects($this->once())
            ->method('save')
            ->with($this->contains(new MongoDate($date->getTimestamp()), false));

        $writer = new MongoWriter($this->mongo, $this->database, $this->collection);

        $writer->write($event);
    }
}
