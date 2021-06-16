<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log;

use ErrorException;
use Exception;
use PHPUnit\Framework\TestCase;
use Zend\Log\Exception\RuntimeException;
use Zend\Log\Filter\Mock as MockFilter;
use Zend\Log\Logger;
use Zend\Log\Processor\Backtrace;
use Zend\Log\Writer\Mock as MockWriter;
use Zend\Log\Writer\Stream as StreamWriter;
use Zend\Stdlib\SplPriorityQueue;
use Zend\Validator\Digits as DigitsFilter;

class LoggerTest extends TestCase
{
    /**
     * @var Logger
     */
    private $logger;

    protected function setUp()
    {
        $this->logger = new Logger;
    }

    public function testUsesWriterPluginManagerByDefault()
    {
        $this->assertInstanceOf('Zend\Log\WriterPluginManager', $this->logger->getWriterPluginManager());
    }

    public function testPassingShortNameToPluginReturnsWriterByThatName()
    {
        $writer = $this->logger->writerPlugin('mock');
        $this->assertInstanceOf('Zend\Log\Writer\Mock', $writer);
    }

    public function testPassWriterAsString()
    {
        $this->logger->addWriter('mock');
        $writers = $this->logger->getWriters();
        $this->assertInstanceOf('Zend\Stdlib\SplPriorityQueue', $writers);
    }

    public function testEmptyWriter()
    {
        $this->expectException(RuntimeException::class);
        $this->expectExceptionMessage('No log writer specified');
        $this->logger->log(Logger::INFO, 'test');
    }

    public function testSetWriters()
    {
        $writer1 = $this->logger->writerPlugin('mock');
        $writer2 = $this->logger->writerPlugin('null');
        $writers = new SplPriorityQueue();
        $writers->insert($writer1, 1);
        $writers->insert($writer2, 2);
        $this->logger->setWriters($writers);

        $writers = $this->logger->getWriters();
        $this->assertInstanceOf('Zend\Stdlib\SplPriorityQueue', $writers);
        $writer = $writers->extract();
        $this->assertInstanceOf('Zend\Log\Writer\Noop', $writer);
        $writer = $writers->extract();
        $this->assertInstanceOf('Zend\Log\Writer\Mock', $writer);
    }

    public function testAddWriterWithPriority()
    {
        $writer1 = $this->logger->writerPlugin('mock');
        $this->logger->addWriter($writer1, 1);
        $writer2 = $this->logger->writerPlugin('null');
        $this->logger->addWriter($writer2, 2);
        $writers = $this->logger->getWriters();

        $this->assertInstanceOf('Zend\Stdlib\SplPriorityQueue', $writers);
        $writer = $writers->extract();
        $this->assertInstanceOf('Zend\Log\Writer\Noop', $writer);
        $writer = $writers->extract();
        $this->assertInstanceOf('Zend\Log\Writer\Mock', $writer);
    }

    public function testAddWithSamePriority()
    {
        $writer1 = $this->logger->writerPlugin('mock');
        $this->logger->addWriter($writer1, 1);
        $writer2 = $this->logger->writerPlugin('null');
        $this->logger->addWriter($writer2, 1);
        $writers = $this->logger->getWriters();

        $this->assertInstanceOf('Zend\Stdlib\SplPriorityQueue', $writers);
        $writer = $writers->extract();
        $this->assertInstanceOf('Zend\Log\Writer\Mock', $writer);
        $writer = $writers->extract();
        $this->assertInstanceOf('Zend\Log\Writer\Noop', $writer);
    }

    public function testLogging()
    {
        $writer = new MockWriter;
        $this->logger->addWriter($writer);
        $this->logger->log(Logger::INFO, 'tottakai');

        $this->assertEquals(count($writer->events), 1);
        $this->assertContains('tottakai', $writer->events[0]['message']);
    }

    public function testLoggingArray()
    {
        $writer = new MockWriter;
        $this->logger->addWriter($writer);
        $this->logger->log(Logger::INFO, ['test']);

        $this->assertEquals(count($writer->events), 1);
        $this->assertContains('test', $writer->events[0]['message']);
    }

    public function testAddFilter()
    {
        $writer = new MockWriter;
        $filter = new MockFilter;
        $writer->addFilter($filter);
        $this->logger->addWriter($writer);
        $this->logger->log(Logger::INFO, ['test']);

        $this->assertEquals(count($filter->events), 1);
        $this->assertContains('test', $filter->events[0]['message']);
    }

    public function testAddFilterByName()
    {
        $writer = new MockWriter;
        $writer->addFilter('mock');
        $this->logger->addWriter($writer);
        $this->logger->log(Logger::INFO, ['test']);

        $this->assertEquals(count($writer->events), 1);
        $this->assertContains('test', $writer->events[0]['message']);
    }

    /**
     * provideTestFilters
     */
    public function provideTestFilters()
    {
        $data = [
            ['priority', ['priority' => Logger::INFO]],
            ['regex', [ 'regex' => '/[0-9]+/' ]],
        ];

        // Conditionally enabled until zend-validator is forwards-compatible
        // with zend-servicemanager v3.
        if (class_exists(DigitsFilter::class)) {
            $data[] = ['validator', ['validator' => new DigitsFilter]];
        }

        return $data;
    }

    /**
     * @dataProvider provideTestFilters
     */
    public function testAddFilterByNameWithParams($filter, $options)
    {
        $writer = new MockWriter;
        $writer->addFilter($filter, $options);
        $this->logger->addWriter($writer);

        $this->logger->log(Logger::INFO, '123');
        $this->assertEquals(count($writer->events), 1);
        $this->assertContains('123', $writer->events[0]['message']);
    }

    public static function provideAttributes()
    {
        return [
            [[]],
            [['user' => 'foo', 'ip' => '127.0.0.1']],
            [new \ArrayObject(['id' => 42])],
        ];
    }

    /**
     * @dataProvider provideAttributes
     */
    public function testLoggingCustomAttributesForUserContext($extra)
    {
        $writer = new MockWriter;
        $this->logger->addWriter($writer);
        $this->logger->log(Logger::ERR, 'tottakai', $extra);

        $this->assertEquals(count($writer->events), 1);
        $this->assertInternalType('array', $writer->events[0]['extra']);
        $this->assertEquals(count($writer->events[0]['extra']), count($extra));
    }

    public static function provideInvalidArguments()
    {
        return [
            [new \stdClass(), ['valid']],
            ['valid', null],
            ['valid', true],
            ['valid', 10],
            ['valid', 'invalid'],
            ['valid', new \stdClass()],
        ];
    }

    /**
     * @dataProvider provideInvalidArguments
     */
    public function testPassingInvalidArgumentToLogRaisesException($message, $extra)
    {
        $this->expectException('Zend\Log\Exception\InvalidArgumentException');
        $this->logger->log(Logger::ERR, $message, $extra);
    }

    public function testRegisterErrorHandler()
    {
        $writer = new MockWriter;
        $this->logger->addWriter($writer);

        $previous = Logger::registerErrorHandler($this->logger);
        $this->assertNotNull($previous);
        $this->assertNotFalse($previous);

        // check for single error handler instance
        $this->assertFalse(Logger::registerErrorHandler($this->logger));

        // generate a warning
        echo $test; // $test is not defined

        Logger::unregisterErrorHandler();

        $this->assertEquals($writer->events[0]['message'], 'Undefined variable: test');
    }

    public function testOptionsWithMock()
    {
        $options = ['writers' => [
                             'first_writer' => [
                                 'name'     => 'mock',
                             ]
                        ]];
        $logger = new Logger($options);

        $writers = $logger->getWriters()->toArray();
        $this->assertCount(1, $writers);
        $this->assertInstanceOf('Zend\Log\Writer\Mock', $writers[0]);
    }

    public function testOptionsWithWriterOptions()
    {
        $options = ['writers' => [
                              [
                                 'name'     => 'stream',
                                 'options'  => [
                                     'stream' => 'php://output',
                                     'log_separator' => 'foo'
                                 ],
                              ]
                         ]];
        $logger = new Logger($options);

        $writers = $logger->getWriters()->toArray();
        $this->assertCount(1, $writers);
        $this->assertInstanceOf('Zend\Log\Writer\Stream', $writers[0]);
        $this->assertEquals('foo', $writers[0]->getLogSeparator());
    }

    public function testOptionsWithMockAndProcessor()
    {
        $options = [
            'writers' => [
                'first_writer' => [
                    'name' => 'mock',
                ],
            ],
            'processors' => [
                'first_processor' => [
                    'name' => 'requestid',
                ],
            ]
        ];
        $logger = new Logger($options);
        $processors = $logger->getProcessors()->toArray();
        $this->assertCount(1, $processors);
        $this->assertInstanceOf('Zend\Log\Processor\RequestId', $processors[0]);
    }

    public function testAddProcessor()
    {
        $processor = new Backtrace();
        $this->logger->addProcessor($processor);

        $processors = $this->logger->getProcessors()->toArray();
        $this->assertEquals($processor, $processors[0]);
    }

    public function testAddProcessorByName()
    {
        $this->logger->addProcessor('backtrace');

        $processors = $this->logger->getProcessors()->toArray();
        $this->assertInstanceOf('Zend\Log\Processor\Backtrace', $processors[0]);

        $writer = new MockWriter;
        $this->logger->addWriter($writer);
        $this->logger->log(Logger::ERR, 'foo');
    }

    public function testProcessorOutputAdded()
    {
        $processor = new Backtrace();
        $this->logger->addProcessor($processor);
        $writer = new MockWriter;
        $this->logger->addWriter($writer);

        $this->logger->log(Logger::ERR, 'foo');
        $this->assertEquals(__FILE__, $writer->events[0]['extra']['file']);
    }

    public function testExceptionHandler()
    {
        $writer = new MockWriter;
        $this->logger->addWriter($writer);

        $this->assertTrue(Logger::registerExceptionHandler($this->logger));

        // check for single error handler instance
        $this->assertFalse(Logger::registerExceptionHandler($this->logger));

        // get the internal exception handler
        $exceptionHandler = set_exception_handler(function ($e) {
        });
        set_exception_handler($exceptionHandler);

        // reset the exception handler
        Logger::unregisterExceptionHandler();

        // call the exception handler
        $exceptionHandler(new Exception('error', 200, new Exception('previos', 100)));
        $exceptionHandler(new ErrorException('user notice', 1000, E_USER_NOTICE, __FILE__, __LINE__));

        // check logged messages
        $expectedEvents = [
            ['priority' => Logger::ERR,    'message' => 'previos',     'file' => __FILE__],
            ['priority' => Logger::ERR,    'message' => 'error',       'file' => __FILE__],
            ['priority' => Logger::NOTICE, 'message' => 'user notice', 'file' => __FILE__],
        ];
        for ($i = 0; $i < count($expectedEvents); $i++) {
            $expectedEvent = $expectedEvents[$i];
            $event         = $writer->events[$i];

            $this->assertEquals($expectedEvent['priority'], $event['priority'], 'Unexpected priority');
            $this->assertEquals($expectedEvent['message'], $event['message'], 'Unexpected message');
            $this->assertEquals($expectedEvent['file'], $event['extra']['file'], 'Unexpected file');
        }
    }

    public function testLogExtraArrayKeyWithNonArrayValue()
    {
        $stream = fopen("php://memory", "r+");
        $options = [
            'writers' => [
                [
                    'name'     => 'stream',
                    'options'  => [
                        'stream' => $stream
                    ],
                ],
            ],
        ];
        $logger = new Logger($options);

        $this->assertInstanceOf('Zend\Log\Logger', $logger->info('Hi', ['extra' => '']));
        fclose($stream);
    }

    /**
     * @group 5383
     */
    public function testErrorHandlerWithStreamWriter()
    {
        $options      = ['errorhandler' => true];
        $logger       = new Logger($options);
        $stream       = fopen('php://memory', 'w+');
        $streamWriter = new StreamWriter($stream);

        // error handler does not like this feature so turn it off
        $streamWriter->setConvertWriteErrorsToExceptions(false);
        $logger->addWriter($streamWriter);

        // we raise two notices - both should be logged
        echo $test;
        echo $second;

        rewind($stream);
        $contents = stream_get_contents($stream);
        $this->assertContains('test', $contents);
        $this->assertContains('second', $contents);
    }

    /**
     * @runInSeparateProcess
     */
    public function testRegisterFatalShutdownFunction()
    {
        if (PHP_VERSION_ID >= 70000) {
            $this->markTestSkipped('PHP7: cannot test as code now raises E_ERROR');
        }

        $writer = new MockWriter;
        $this->logger->addWriter($writer);

        $result = Logger::registerFatalErrorShutdownFunction($this->logger);
        $this->assertTrue($result);

        // check for single error handler instance
        $this->assertFalse(Logger::registerFatalErrorShutdownFunction($this->logger));

        register_shutdown_function(function () use ($writer) {
            $this->assertEquals(
                'Call to undefined method ZendTest\Log\LoggerTest::callToNonExistingMethod()',
                $writer->events[0]['message']
            );
        });

        // Temporarily hide errors, because we don't want the fatal error to fail the test
        @$this->callToNonExistingMethod();
    }

    /**
     * @runInSeparateProcess
     *
     * @group 6424
     */
    public function testRegisterFatalErrorShutdownFunctionHandlesCompileTimeErrors()
    {
        if (PHP_VERSION_ID >= 70000) {
            $this->markTestSkipped('PHP7: cannot test as code now raises E_ERROR');
        }

        $writer = new MockWriter;
        $this->logger->addWriter($writer);

        $result = Logger::registerFatalErrorShutdownFunction($this->logger);
        $this->assertTrue($result);

        // check for single error handler instance
        $this->assertFalse(Logger::registerFatalErrorShutdownFunction($this->logger));

        register_shutdown_function(function () use ($writer) {
            $this->assertStringMatchesFormat(
                'syntax error%A',
                $writer->events[0]['message']
            );
        });

        // Temporarily hide errors, because we don't want the fatal error to fail the test
        @eval('this::code::is::invalid {}');
    }

    /**
     * @group ZF2-7238
     */
    public function testCatchExceptionNotValidPriority()
    {
        $this->expectException('Zend\Log\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('$priority must be an integer >= 0 and < 8; received -1');
        $writer = new MockWriter();
        $this->logger->addWriter($writer);
        $this->logger->log(-1, 'Foo');
    }
}
