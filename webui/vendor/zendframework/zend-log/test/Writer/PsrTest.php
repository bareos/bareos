<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log\Writer;

use PHPUnit\Framework\TestCase;
use Psr\Log\LoggerInterface;
use Psr\Log\LogLevel;
use Psr\Log\NullLogger;
use Zend\Log\Filter\Mock as MockFilter;
use Zend\Log\Formatter\Simple as SimpleFormatter;
use Zend\Log\Logger;
use Zend\Log\Writer\Psr as PsrWriter;

/**
 * @coversDefaultClass \Zend\Log\Writer\Psr
 * @covers ::<!public>
 */
class PsrTest extends TestCase
{
    /**
     * @covers ::__construct
     */
    public function testConstructWithPsrLogger()
    {
        $psrLogger = $this->createMock(LoggerInterface::class);
        $writer    = new PsrWriter($psrLogger);
        $this->assertAttributeSame($psrLogger, 'logger', $writer);
    }

    /**
     * @covers ::__construct
     */
    public function testConstructWithOptions()
    {
        $psrLogger = $this->createMock(LoggerInterface::class);
        $formatter = new SimpleFormatter();
        $filter    = new MockFilter();
        $writer = new PsrWriter([
            'filters'   => $filter,
            'formatter' => $formatter,
            'logger'    => $psrLogger,
        ]);

        $this->assertAttributeSame($psrLogger, 'logger', $writer);
        $this->assertAttributeSame($formatter, 'formatter', $writer);

        $filters = self::readAttribute($writer, 'filters');
        $this->assertCount(1, $filters);
        $this->assertEquals($filter, $filters[0]);
    }

    /**
     * @covers ::__construct
     */
    public function testFallbackLoggerIsNullLogger()
    {
        $writer = new PsrWriter;
        $this->assertAttributeInstanceOf(NullLogger::class, 'logger', $writer);
    }

    /**
     * @dataProvider priorityToLogLevelProvider
     */
    public function testWriteLogMapsLevelsProperly($priority, $logLevel)
    {
        $message = 'foo';
        $extra   = ['bar' => 'baz'];

        $psrLogger = $this->createMock(LoggerInterface::class);
        $psrLogger->expects($this->once())
            ->method('log')
            ->with(
                $this->equalTo($logLevel),
                $this->equalTo($message),
                $this->equalTo($extra)
            );

        $writer = new PsrWriter($psrLogger);
        $logger = new Logger();
        $logger->addWriter($writer);

        $logger->log($priority, $message, $extra);
    }

    /**
     * Data provider
     *
     * @return array
     */
    public function priorityToLogLevelProvider()
    {
        return [
            'emergency' => [Logger::EMERG, LogLevel::EMERGENCY],
            'alert'     => [Logger::ALERT, LogLevel::ALERT],
            'critical'  => [Logger::CRIT, LogLevel::CRITICAL],
            'error'     => [Logger::ERR, LogLevel::ERROR],
            'warn'      => [Logger::WARN, LogLevel::WARNING],
            'notice'    => [Logger::NOTICE, LogLevel::NOTICE],
            'info'      => [Logger::INFO, LogLevel::INFO],
            'debug'     => [Logger::DEBUG, LogLevel::DEBUG],
        ];
    }
}
