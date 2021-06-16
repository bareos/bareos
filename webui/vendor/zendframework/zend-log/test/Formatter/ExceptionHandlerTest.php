<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log\Formatter;

use DateTime;
use PHPUnit\Framework\TestCase;
use Zend\Log\Formatter\ExceptionHandler;

class ExceptionHandlerTest extends TestCase
{
    public function testFormat()
    {
        $date = new DateTime();

        $event = [
            'timestamp'    => $date,
            'message'      => 'test',
            'priority'     => 1,
            'priorityName' => 'CRIT',
            'extra' => [
                'file'  => 'test.php',
                'line'  => 1,
                'trace' => [
                    [
                        'file'     => 'test.php',
                        'line'     => 1,
                        'function' => 'test',
                        'class'    => 'Test',
                        'type'     => '::',
                        'args'     => [1]
                    ],
                    [
                        'file'     => 'test.php',
                        'line'     => 2,
                        'function' => 'test',
                        'class'    => 'Test',
                        'type'     => '::',
                        'args'     => [1]
                    ]
                ]
            ]
        ];

        // The formatter ends with unix style line endings so make sure we expect that
        // output as well:
        $expected = $date->format('c') . " CRIT (1) test in test.php on line 1\n";
        $expected .= "[Trace]\n";
        $expected .= "File  : test.php\n";
        $expected .= "Line  : 1\n";
        $expected .= "Func  : test\n";
        $expected .= "Class : Test\n";
        $expected .= "Type  : static\n";
        $expected .= "Args  : Array\n";
        $expected .= "(\n";
        $expected .= "    [0] => 1\n";
        $expected .= ")\n\n";
        $expected .= "File  : test.php\n";
        $expected .= "Line  : 2\n";
        $expected .= "Func  : test\n";
        $expected .= "Class : Test\n";
        $expected .= "Type  : static\n";
        $expected .= "Args  : Array\n";
        $expected .= "(\n";
        $expected .= "    [0] => 1\n";
        $expected .= ")\n\n";

        $formatter = new ExceptionHandler();
        $output = $formatter->format($event);

        $this->assertEquals($expected, $output);
    }

    /**
     * @dataProvider provideDateTimeFormats
     */
    public function testSetDateTimeFormat($dateTimeFormat)
    {
        $date = new DateTime();

        $event = [
            'timestamp'    => $date,
            'message'      => 'test',
            'priority'     => 1,
            'priorityName' => 'CRIT',
            'extra' => [
                'file'  => 'test.php',
                'line'  => 1,
            ],
        ];

        $expected = $date->format($dateTimeFormat) . ' CRIT (1) test in test.php on line 1';

        $formatter = new ExceptionHandler();

        $this->assertSame($formatter, $formatter->setDateTimeFormat($dateTimeFormat));
        $this->assertEquals($dateTimeFormat, $formatter->getDateTimeFormat());
        $this->assertEquals($expected, $formatter->format($event));
    }

    public function provideDateTimeFormats()
    {
        return [
            ['r'],
            ['U'],
        ];
    }
}
