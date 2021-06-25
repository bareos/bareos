<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log;

use Psr\Log\LogLevel;
use Psr\Log\Test\LoggerInterfaceTest;
use Zend\Log\Logger;
use Zend\Log\PsrLoggerAdapter;
use Zend\Log\Writer\Mock as MockWriter;

/**
 * @coversDefaultClass \Zend\Log\PsrLoggerAdapter
 * @covers ::<!public>
 */
class PsrLoggerAdapterTest extends LoggerInterfaceTest
{
    /**
     * @var array
     */
    protected $psrPriorityMap = [
        LogLevel::EMERGENCY => Logger::EMERG,
        LogLevel::ALERT     => Logger::ALERT,
        LogLevel::CRITICAL  => Logger::CRIT,
        LogLevel::ERROR     => Logger::ERR,
        LogLevel::WARNING   => Logger::WARN,
        LogLevel::NOTICE    => Logger::NOTICE,
        LogLevel::INFO      => Logger::INFO,
        LogLevel::DEBUG     => Logger::DEBUG,
    ];

    /**
     * Provides logger for LoggerInterface compat tests
     *
     * @return PsrLoggerAdapter
     */
    public function getLogger()
    {
        $this->mockWriter = new MockWriter;
        $logger           = new Logger;
        $logger->addProcessor('psrplaceholder');
        $logger->addWriter($this->mockWriter);
        return new PsrLoggerAdapter($logger);
    }

    /**
     * This must return the log messages in order.
     *
     * The simple formatting of the messages is: "<LOG LEVEL> <MESSAGE>".
     *
     * Example ->error('Foo') would yield "error Foo".
     *
     * @return string[]
     */
    public function getLogs()
    {
        $prefixMap = array_flip($this->psrPriorityMap);
        return array_map(function ($event) use ($prefixMap) {
            $prefix  = $prefixMap[$event['priority']];
            $message = $prefix . ' ' . $event['message'];
            return $message;
        }, $this->mockWriter->events);
    }

    protected function tearDown()
    {
        unset($this->mockWriter);
    }

    /**
     *
     * @covers ::__construct
     * @covers ::getLogger
     */
    public function testSetLogger()
    {
        $logger = new Logger;

        $adapter = new PsrLoggerAdapter($logger);
        $this->assertAttributeEquals($logger, 'logger', $adapter);

        $this->assertSame($logger, $adapter->getLogger($logger));
    }

    /**
     * @covers ::log
     * @dataProvider logLevelsToPriorityProvider
     */
    public function testPsrLogLevelsMapsToPriorities($logLevel, $priority)
    {
        $message = 'foo';
        $context = ['bar' => 'baz'];

        $logger = $this->getMockBuilder(Logger::class)
            ->setMethods(['log'])
            ->getMock();
        $logger->expects($this->once())
            ->method('log')
            ->with(
                $this->equalTo($priority),
                $this->equalTo($message),
                $this->equalTo($context)
            );

        $adapter = new PsrLoggerAdapter($logger);
        $adapter->log($logLevel, $message, $context);
    }

    /**
     * Data provider
     *
     * @return array
     */
    public function logLevelsToPriorityProvider()
    {
        $return = [];
        foreach ($this->psrPriorityMap as $level => $priority) {
            $return[] = [$level, $priority];
        }
        return $return;
    }
}
