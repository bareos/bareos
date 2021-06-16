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
use Zend\Log\Formatter\Simple as SimpleFormatter;
use Zend\Log\Logger;
use Zend\Log\Writer\Syslog as SyslogWriter;
use ZendTest\Log\TestAsset\CustomSyslogWriter;

class SyslogTest extends TestCase
{
    public function testWrite()
    {
        $fields = [
            'message' => 'foo',
            'priority' => LOG_NOTICE
        ];
        $writer = new SyslogWriter();
        $writer->write($fields);
    }

    /**
     * @group ZF-7603
     */
    public function testThrowExceptionValueNotPresentInFacilities()
    {
        $this->expectException('Zend\Log\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Invalid log facility provided');
        $writer = new SyslogWriter();
        $writer->setFacility(LOG_USER * 1000);
    }

    /**
     * @group ZF-7603
     */
    public function testThrowExceptionIfFacilityInvalidInWindows()
    {
        if ('WIN' != strtoupper(substr(PHP_OS, 0, 3))) {
            $this->markTestSkipped('Run only in windows');
        }
        $this->expectException('Zend\Log\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Only LOG_USER is a valid');
        $writer = new SyslogWriter();
        $writer->setFacility(LOG_AUTH);
    }

    /**
     * @group ZF-8953
     */
    public function testFluentInterface()
    {
        $writer   = new SyslogWriter();
        $instance = $writer->setFacility(LOG_USER)
                           ->setApplicationName('my_app');

        $this->assertInstanceOf('Zend\Log\Writer\Syslog', $instance);
    }

    /**
     * @group ZF-10769
     */
    public function testPastFacilityViaConstructor()
    {
        $writer = new CustomSyslogWriter(['facility' => LOG_USER]);
        $this->assertEquals(LOG_USER, $writer->getFacility());
    }

    /**
     * @group ZF-8382
     */
    public function testWriteWithFormatter()
    {
        $event = [
            'message' => 'tottakai',
            'priority' => Logger::ERR
        ];

        $writer = new SyslogWriter();
        $formatter = new SimpleFormatter('%message% (this is a test)');
        $writer->setFormatter($formatter);

        $writer->write($event);
    }

    /**
     * @group ZF2-534
     */
    public function testPassApplicationNameViaConstructor()
    {
        $writer   = new CustomSyslogWriter(['application' => 'test_app']);
        $this->assertEquals('test_app', $writer->getApplicationName());
    }

    public function testConstructWithOptions()
    {
        $formatter = new \Zend\Log\Formatter\Simple();
        $filter    = new \Zend\Log\Filter\Mock();
        $writer = new CustomSyslogWriter([
                'filters'   => $filter,
                'formatter' => $formatter,
                'application'  => 'test_app',
        ]);
        $this->assertEquals('test_app', $writer->getApplicationName());
        $this->assertAttributeEquals($formatter, 'formatter', $writer);

        $filters = self::readAttribute($writer, 'filters');
        $this->assertCount(1, $filters);
        $this->assertEquals($filter, $filters[0]);
    }

    public function testDefaultFormatter()
    {
        $writer   = new CustomSyslogWriter(['application' => 'test_app']);
        $this->assertAttributeInstanceOf('Zend\Log\Formatter\Simple', 'formatter', $writer);
    }
}
