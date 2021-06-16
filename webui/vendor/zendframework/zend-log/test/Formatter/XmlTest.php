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
use Zend\Log\Formatter\Xml as XmlFormatter;
use ZendTest\Log\TestAsset\SerializableObject;

class XmlTest extends TestCase
{
    public function testDefaultFormat()
    {
        $date = new DateTime();
        $f = new XmlFormatter();
        $line = $f->format(['timestamp' => $date, 'message' => 'foo', 'priority' => 42]);

        $this->assertContains($date->format('c'), $line);
        $this->assertContains('foo', $line);
        $this->assertContains((string)42, $line);
    }

    public function testConfiguringElementMapping()
    {
        $f = new XmlFormatter('log', ['foo' => 'bar']);
        $line = $f->format(['bar' => 'baz']);
        $this->assertContains('<log><foo>baz</foo></log>', $line);
    }

    /**
     * @dataProvider provideDateTimeFormats
     */
    public function testConfiguringDateTimeFormat($dateTimeFormat)
    {
        $date = new DateTime();
        $f = new XmlFormatter('log', null, 'UTF-8', $dateTimeFormat);
        $this->assertContains($date->format($dateTimeFormat), $f->format(['timestamp' => $date]));
    }

    /**
     * @dataProvider provideDateTimeFormats
     */
    public function testSetDateTimeFormat($dateTimeFormat)
    {
        $date = new DateTime();
        $f = new XmlFormatter();
        $this->assertSame($f, $f->setDateTimeFormat($dateTimeFormat));
        $this->assertContains($dateTimeFormat, $f->getDateTimeFormat());
        $this->assertContains($date->format($dateTimeFormat), $f->format(['timestamp' => $date]));
    }

    public function provideDateTimeFormats()
    {
        return [
            ['r'],
            ['U'],
        ];
    }

    public function testXmlDeclarationIsStripped()
    {
        $f = new XmlFormatter();
        $line = $f->format(['message' => 'foo', 'priority' => 42]);

        $this->assertNotContains('<\?xml version=', $line);
    }

    public function testXmlValidates()
    {
        $f = new XmlFormatter();
        $line = $f->format(['message' => 'foo', 'priority' => 42]);

        $sxml = @simplexml_load_string($line);
        $this->assertInstanceOf('SimpleXMLElement', $sxml, 'Formatted XML is invalid');
    }

    /**
     * @group ZF-2062
     * @group ZF-4190
     */
    public function testHtmlSpecialCharsInMessageGetEscapedForValidXml()
    {
        $f = new XmlFormatter();
        $line = $f->format(['message' => '&key1=value1&key2=value2', 'priority' => 42]);

        $this->assertContains("&amp;", $line);
        $this->assertEquals(2, substr_count($line, "&amp;"));
    }

    /**
     * @group ZF-2062
     * @group ZF-4190
     */
    public function testFixingBrokenCharsSoXmlIsValid()
    {
        $f = new XmlFormatter();
        $line = $f->format(['message' => '&amp', 'priority' => 42]);

        $this->assertContains('&amp;amp', $line);
    }

    public function testConstructorWithArray()
    {
        $date = new DateTime();
        $options = [
            'rootElement' => 'log',
            'elementMap' => [
                'date' => 'timestamp',
                'word' => 'message',
                'priority' => 'priority'
            ],
            'dateTimeFormat' => 'r',
        ];
        $event = [
            'timestamp' => $date,
            'message' => 'tottakai',
            'priority' => 4
        ];
        $expected = sprintf(
            '<log><date>%s</date><word>tottakai</word><priority>4</priority></log>',
            $date->format('r')
        );

        $formatter = new XmlFormatter($options);
        $output = $formatter->format($event);
        $this->assertContains($expected, $output);
        $this->assertEquals('UTF-8', $formatter->getEncoding());
    }

    /**
     * @group ZF-11161
     */
    public function testNonScalarValuesAreExcludedFromFormattedString()
    {
        $options = [
            'rootElement' => 'log'
        ];
        $event = [
            'message' => 'tottakai',
            'priority' => 4,
            'context' => ['test' => 'one'],
            'reference' => new XmlFormatter()
        ];
        $expected = '<log><message>tottakai</message><priority>4</priority></log>';

        $formatter = new XmlFormatter($options);
        $output = $formatter->format($event);
        $this->assertContains($expected, $output);
    }

    /**
     * @group ZF-11161
     */
    public function testObjectsWithStringSerializationAreIncludedInFormattedString()
    {
        $options = [
            'rootElement' => 'log'
        ];
        $event = [
            'message' => 'tottakai',
            'priority' => 4,
            'context' => ['test' => 'one'],
            'reference' => new SerializableObject()
        ];
        $expected = '<log><message>tottakai</message><priority>4</priority><reference>'
            .'ZendTest\Log\TestAsset\SerializableObject</reference></log>';

        $formatter = new XmlFormatter($options);
        $output = $formatter->format($event);
        $this->assertContains($expected, $output);
    }

    /**
     * @group ZF2-453
     */
    public function testFormatWillRemoveExtraEmptyArrayFromEvent()
    {
        $formatter = new XmlFormatter;
        $d = new DateTime('2001-01-01T12:00:00-06:00');
        $event = [
            'timestamp'    => $d,
            'message'      => 'test',
            'priority'     => 1,
            'priorityName' => 'CRIT',
            'extra'        => []
        ];
        $expected = '<logEntry><timestamp>2001-01-01T12:00:00-06:00</timestamp><message>test</message>'
            .'<priority>1</priority><priorityName>CRIT</priorityName></logEntry>';
        $expected .= "\n" . PHP_EOL;
        $this->assertEquals($expected, $formatter->format($event));
    }

    public function testFormatWillAcceptSimpleArrayFromExtra()
    {
        $formatter = new XmlFormatter;
        $d = new DateTime('2001-01-01T12:00:00-06:00');
        $event = [
            'timestamp'    => $d,
            'message'      => 'test',
            'priority'     => 1,
            'priorityName' => 'CRIT',
            'extra'        => [
                'test' => 'one',
                'bar'  => 'foo',
                'wrong message' => 'dasdasd'
            ]
        ];

        $expected = '<logEntry><timestamp>2001-01-01T12:00:00-06:00</timestamp><message>test</message>'
            .'<priority>1</priority><priorityName>CRIT</priorityName><extra><test>one</test>'
            .'<bar>foo</bar></extra></logEntry>';
        $expected .= "\n" . PHP_EOL;
        $this->assertEquals($expected, $formatter->format($event));
    }

    public function testFormatWillAcceptNestedArrayFromExtraEvent()
    {
        $formatter = new XmlFormatter;

        $d = new DateTime('2001-01-01T12:00:00-06:00');

        $event = [
            'timestamp'    => $d,
            'message'      => 'test',
            'priority'     => 1,
            'priorityName' => 'CRIT',
            'extra'        => [
                'test'  => [
                    'one',
                    'two' => [
                        'three' => [
                            'four' => 'four'
                        ],
                        'five'  => ['']
                    ]
                ],
                '1111'                => '2222',
                'test_null'           => null,
                'test_int'            => 14,
                'test_object'         => new \stdClass(),
                new SerializableObject(),
                'serializable_object' => new SerializableObject(),
                null,
                'test_empty_array'    => [],
                'bar'                 => 'foo',
                'foobar'
            ]
        ];
        $expected = '<logEntry><timestamp>2001-01-01T12:00:00-06:00</timestamp><message>test</message>'
            .'<priority>1</priority><priorityName>CRIT</priorityName><extra><test><one/><two><three><four>four</four>'
            .'</three><five/></two></test><test_null/><test_int>14</test_int><test_object>'
            .'"Object" of type stdClass does not support __toString() method</test_object><serializable_object>'
            .'ZendTest\Log\TestAsset\SerializableObject</serializable_object><test_empty_array/>'
            .'<bar>foo</bar><foobar/></extra></logEntry>';
        $expected .= "\n" . PHP_EOL;
        $this->assertEquals($expected, $formatter->format($event));
    }

    public function testFormatWillEscapeAmpersand()
    {
        $formatter = new XmlFormatter;

        $d = new DateTime('2001-01-01T12:00:00-06:00');

        $event = [
            'timestamp'    => $d,
            'message'      => 'test',
            'priority'     => 1,
            'priorityName' => 'CRIT',
            'extra'        => [
                'test'  => [
                    'one',
                    'two' => [
                        'three' => [
                            'four' => 'four&four'
                        ],
                        'five'  => ['']
                    ]
                ],
                '1111'                => '2222',
                'test_null'           => null,
                'test_int'            => 14,
                'test_object'         => new \stdClass(),
                new SerializableObject(),
                'serializable_object' => new SerializableObject(),
                null,
                'test_empty_array'    => [],
                'bar'                 => 'foo',
                'foobar'
            ]
        ];

        // @codingStandardsIgnoreStart
        $expected = '<logEntry><timestamp>2001-01-01T12:00:00-06:00</timestamp><message>test</message><priority>1</priority><priorityName>CRIT</priorityName><extra><test><one/><two><three><four>four&amp;four</four></three><five/></two></test><test_null/><test_int>14</test_int><test_object>"Object" of type stdClass does not support __toString() method</test_object><serializable_object>ZendTest\Log\TestAsset\SerializableObject</serializable_object><test_empty_array/><bar>foo</bar><foobar/></extra></logEntry>';
        $expected .= "\n" . PHP_EOL;
        // @codingStandardsIgnoreEnd

        $this->assertEquals($expected, $formatter->format($event));
    }
}
