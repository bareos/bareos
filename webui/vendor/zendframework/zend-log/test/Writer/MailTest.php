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
use Zend\Log\Logger;
use Zend\Log\Writer\Mail as MailWriter;
use Zend\Mail\Message as MailMessage;
use Zend\Mail\Transport;

class MailTest extends TestCase
{
    const FILENAME = 'message.txt';

    /**
     * @var MailWriter
     */
    protected $writer;

    /**
     * @var Logger
     */
    protected $log;

    protected function setUp()
    {
        $message = new MailMessage();
        $transport = new Transport\File();
        $options   = new Transport\FileOptions([
            'path'      => __DIR__,
            'callback'  => function (Transport\File $transport) {
                return MailTest::FILENAME;
            },
        ]);
        $transport->setOptions($options);

        $this->writer = new MailWriter($message, $transport);
        $this->log = new Logger();
        $this->log->addWriter($this->writer);
    }

    protected function tearDown()
    {
        if (file_exists(__DIR__. '/' . self::FILENAME)) {
            unlink(__DIR__. '/' . self::FILENAME);
        }
    }

    /**
     * Tests normal logging, but with multiple messages for a level.
     *
     * @return void
     */
    public function testNormalLoggingMultiplePerLevel()
    {
        $this->log->info('an info message');
        $this->log->info('a second info message');
        unset($this->log);

        $contents = file_get_contents(__DIR__ . '/' . self::FILENAME);
        $this->assertContains('an info message', $contents);
        $this->assertContains('a second info message', $contents);
    }

    public function testSetSubjectPrependText()
    {
        $this->writer->setSubjectPrependText('test');

        $this->log->info('an info message');
        $this->log->info('a second info message');
        unset($this->log);

        $contents = file_get_contents(__DIR__ . '/' . self::FILENAME);
        $this->assertContains('an info message', $contents);
        $this->assertContains('Subject: test', $contents);
    }

    public function testConstructWithOptions()
    {
        $message   = new MailMessage();
        $transport = new Transport\File();
        $options   = new Transport\FileOptions([
                'path'      => __DIR__,
                'callback'  => function (Transport\File $transport) {
                    return MailTest::FILENAME;
                },
        ]);
        $transport->setOptions($options);

        $formatter = new \Zend\Log\Formatter\Simple();
        $filter    = new \Zend\Log\Filter\Mock();
        $writer = new MailWriter([
                'filters'   => $filter,
                'formatter' => $formatter,
                'mail'      => $message,
                'transport' => $transport,
        ]);

        $this->assertAttributeEquals($message, 'mail', $writer);
        $this->assertAttributeEquals($transport, 'transport', $writer);
        $this->assertAttributeEquals($formatter, 'formatter', $writer);

        $filters = self::readAttribute($writer, 'filters');
        $this->assertCount(1, $filters);
        $this->assertEquals($filter, $filters[0]);
    }

    public function testConstructWithMailAsArrayOptions()
    {
        $messageOptions = [
            'encoding'  => 'UTF-8',
            'from'      => 'matthew@example.com',
            'to'        => 'zf-devteam@example.com',
            'subject'   => 'subject',
            'body'      => 'body',
        ];

        $writer = new MailWriter([
            'mail' => $messageOptions,
        ]);

        $this->assertAttributeInstanceOf('Zend\Mail\Message', 'mail', $writer);
    }

    public function testConstructWithMailTransportAsArrayOptions()
    {
        $messageOptions = [
            'encoding'  => 'UTF-8',
            'from'      => 'matthew@example.com',
            'to'        => 'zf-devteam@example.com',
            'subject'   => 'subject',
            'body'      => 'body',
        ];

        $transportOptions = [
            'type' => 'smtp',
            'options' => [
                'host' => 'test.dev',
                'connection_class' => 'login',
                'connection_config' => [
                    'username' => 'foo',
                    'smtp_password' => 'bar',
                    'ssl' => 'tls'
                ]
            ]
        ];

        $writer = new MailWriter([
            'mail' => $messageOptions,
            'transport' => $transportOptions,
        ]);

        $this->assertAttributeInstanceOf('Zend\Mail\Transport\Smtp', 'transport', $writer);
    }
}
