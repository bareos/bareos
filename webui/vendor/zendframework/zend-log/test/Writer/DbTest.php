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
use PHPUnit\Framework\TestCase;
use ReflectionMethod;
use Zend\Log\Formatter\FormatterInterface;
use Zend\Log\Writer\Db as DbWriter;
use ZendTest\Log\TestAsset\MockDbAdapter;

class DbTest extends TestCase
{
    protected function setUp()
    {
        $this->tableName = 'db-table-name';

        $this->db     = new MockDbAdapter();
        $this->writer = new DbWriter($this->db, $this->tableName);
    }

    public function testNotPassingTableNameToConstructorThrowsException()
    {
        $this->expectException('Zend\Log\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('table name');
        $writer = new DbWriter($this->db);
    }

    public function testNotPassingDbToConstructorThrowsException()
    {
        $this->expectException('Zend\Log\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Adapter');
        $writer = new DbWriter([]);
    }

    public function testPassingTableNameAsArgIsOK()
    {
        $options = [
            'db'    => $this->db,
            'table' => $this->tableName,
        ];
        $writer = new DbWriter($options);
        $this->assertInstanceOf('Zend\Log\Writer\Db', $writer);
        $this->assertAttributeEquals($this->tableName, 'tableName', $writer);
    }

    public function testWriteWithDefaults()
    {
        // log to the mock db adapter
        $fields = [
            'message'  => 'foo',
            'priority' => 42
        ];

        $this->writer->write($fields);

        // insert should be called once...
        $this->assertContains('query', array_keys($this->db->calls));
        $this->assertEquals(1, count($this->db->calls['query']));
        $this->assertContains('execute', array_keys($this->db->calls));
        $this->assertEquals(1, count($this->db->calls['execute']));
        $this->assertEquals([$fields], $this->db->calls['execute'][0]);
    }

    public function testWriteWithDefaultsUsingArray()
    {
        // log to the mock db adapter
        $message  = 'message-to-log';
        $priority = 2;
        $events = [
            'file' => 'test',
            'line' => 1
        ];
        $this->writer->write([
            'message'  => $message,
            'priority' => $priority,
            'events'   => $events
        ]);
        $this->assertContains('query', array_keys($this->db->calls));
        $this->assertEquals(1, count($this->db->calls['query']));

        $binds = [
            'message' => $message,
            'priority' => $priority,
            'events_line' => $events['line'],
            'events_file' => $events['file']
        ];
        $this->assertEquals([$binds], $this->db->calls['execute'][0]);
    }

    public function testWriteWithDefaultsUsingArrayAndSeparator()
    {
        $this->writer = new DbWriter($this->db, $this->tableName, null, '-');

        // log to the mock db adapter
        $message  = 'message-to-log';
        $priority = 2;
        $events = [
            'file' => 'test',
            'line' => 1
        ];
        $this->writer->write([
            'message'  => $message,
            'priority' => $priority,
            'events'   => $events
        ]);
        $this->assertContains('query', array_keys($this->db->calls));
        $this->assertEquals(1, count($this->db->calls['query']));

        $binds = [
            'message' => $message,
            'priority' => $priority,
            'events-line' => $events['line'],
            'events-file' => $events['file']
        ];
        $this->assertEquals([$binds], $this->db->calls['execute'][0]);
    }

    public function testWriteUsesOptionalCustomColumnNames()
    {
        $this->writer = new DbWriter($this->db, $this->tableName, [
            'message' => 'new-message-field',
            'priority' => 'new-priority-field'
        ]);

        // log to the mock db adapter
        $message  = 'message-to-log';
        $priority = 2;
        $this->writer->write([
            'message' => $message,
            'priority' => $priority
        ]);

        // insert should be called once...
        $this->assertContains('query', array_keys($this->db->calls));
        $this->assertEquals(1, count($this->db->calls['query']));

        // ...with the correct table and binds for the database
        $binds = [
            'new-message-field' => $message,
            'new-priority-field' => $priority
        ];
        $this->assertEquals([$binds], $this->db->calls['execute'][0]);
    }

    public function testWriteUsesParamsWithArray()
    {
        $this->writer = new DbWriter($this->db, $this->tableName, [
            'message' => 'new-message-field',
            'priority' => 'new-priority-field',
            'events' => [
                'line' => 'new-line',
                'file' => 'new-file'
            ]
        ]);

        // log to the mock db adapter
        $message  = 'message-to-log';
        $priority = 2;
        $events = [
            'file' => 'test',
            'line' => 1
        ];
        $this->writer->write([
            'message'  => $message,
            'priority' => $priority,
            'events'   => $events
        ]);
        $this->assertContains('query', array_keys($this->db->calls));
        $this->assertEquals(1, count($this->db->calls['query']));
        // ...with the correct table and binds for the database
        $binds = [
            'new-message-field' => $message,
            'new-priority-field' => $priority,
            'new-line' => $events['line'],
            'new-file' => $events['file']
        ];
        $this->assertEquals([$binds], $this->db->calls['execute'][0]);
    }

    public function testShutdownRemovesReferenceToDatabaseInstance()
    {
        $this->writer->write(['message' => 'this should not fail']);
        $this->writer->shutdown();

        $this->expectException('Zend\Log\Exception\RuntimeException');
        $this->expectExceptionMessage('Database adapter is null');
        $this->writer->write(['message' => 'this should fail']);
    }

    public function testWriteDateTimeAsTimestamp()
    {
        $date = new DateTime();
        $event = ['timestamp' => $date];
        $this->writer->write($event);

        $this->assertContains('query', array_keys($this->db->calls));
        $this->assertEquals(1, count($this->db->calls['query']));

        $this->assertEquals([[
            'timestamp' => $date->format(FormatterInterface::DEFAULT_DATETIME_FORMAT)
        ]], $this->db->calls['execute'][0]);
    }

    public function testWriteDateTimeAsExtraValue()
    {
        $date = new DateTime();
        $event = [
            'extra' => [
                'request_time' => $date
            ]
        ];
        $this->writer->write($event);

        $this->assertContains('query', array_keys($this->db->calls));
        $this->assertEquals(1, count($this->db->calls['query']));

        $this->assertEquals([[
            'extra_request_time' => $date->format(FormatterInterface::DEFAULT_DATETIME_FORMAT)
        ]], $this->db->calls['execute'][0]);
    }

    public function testConstructWithOptions()
    {
        $formatter = new \Zend\Log\Formatter\Simple();
        $filter    = new \Zend\Log\Filter\Mock();
        $writer = new DbWriter([
            'filters'   => $filter,
            'formatter' => $formatter,
            'table'     => $this->tableName,
            'db'        => $this->db,

        ]);
        $this->assertInstanceOf('Zend\Log\Writer\Db', $writer);
        $this->assertAttributeEquals($this->tableName, 'tableName', $writer);

        $filters = self::readAttribute($writer, 'filters');
        $this->assertCount(1, $filters);
        $this->assertEquals($filter, $filters[0]);

        $registeredFormatter = self::readAttribute($writer, 'formatter');
        $this->assertSame($formatter, $registeredFormatter);

        $registeredDb = self::readAttribute($writer, 'db');
        $this->assertSame($this->db, $registeredDb);
    }

    /**
     * @group 2589
     */
    public function testMapEventIntoColumnDoesNotTriggerArrayToStringConversion()
    {
        $this->writer = new DbWriter($this->db, $this->tableName, [
            'priority' => 'new-priority-field',
            'message'  => 'new-message-field',
            'extra'    => [
                'file'  => 'new-file',
                'line'  => 'new-line',
                'trace' => 'new-trace',
            ]
        ]);

        // log to the mock db adapter
        $priority = 2;
        $message  = 'message-to-log';
        $extra    = [
            'file'  => 'test.php',
            'line'  => 1,
            'trace' => [
                [
                    'function' => 'Bar',
                    'class'    => 'Foo',
                    'type'     => '->',
                    'args'     => [
                        'baz',
                    ],
                ],
            ],
        ];
        $this->writer->write([
            'priority' => $priority,
            'message'  => $message,
            'extra'    => $extra,
        ]);

        $this->assertContains('query', array_keys($this->db->calls));
        $this->assertEquals(1, count($this->db->calls['query']));

        foreach ($this->db->calls['execute'][0][0] as $fieldName => $fieldValue) {
            $this->assertInternalType('string', $fieldName);
            $this->assertInternalType('string', (string) $fieldValue);
        }
    }

    /**
     * @group 2589
     */
    public function testMapEventIntoColumnMustReturnScalarValues()
    {
        $event = [
            'priority' => 2,
            'message'  => 'message-to-log',
            'extra'    => [
                'file'  => 'test.php',
                'line'  => 1,
                'trace' => [
                    [
                        'function' => 'Bar',
                        'class'    => 'Foo',
                        'type'     => '->',
                        'args'     => [
                            'baz',
                        ],
                    ],
                ],
            ],
        ];

        $columnMap = [
            'priority' => 'new-priority-field',
            'message'  => 'new-message-field' ,
            'extra'    => [
                'file'  => 'new-file',
                'line'  => 'new-line',
                'trace' => 'new-trace',
            ]
        ];

        $method = new ReflectionMethod($this->writer, 'mapEventIntoColumn');
        $method->setAccessible(true);
        $data = $method->invoke($this->writer, $event, $columnMap);

        foreach ($data as $field => $value) {
            $this->assertInternalType('scalar', $value, sprintf(
                'Value of column "%s" should be scalar',
                $field
            ));
        }
    }

    public function testEventIntoColumnMustReturnScalarValues()
    {
        $event = [
            'priority' => 2,
            'message'  => 'message-to-log',
            'extra'    => [
                'file'  => 'test.php',
                'line'  => 1,
                'trace' => [
                    [
                        'function' => 'Bar',
                        'class'    => 'Foo',
                        'type'     => '->',
                        'args'     => [
                            'baz',
                        ],
                    ],
                ],
            ],
        ];

        $method = new ReflectionMethod($this->writer, 'eventIntoColumn');
        $method->setAccessible(true);
        $data = $method->invoke($this->writer, $event);

        foreach ($data as $field => $value) {
            $this->assertInternalType('scalar', $value, sprintf(
                'Value of column "%s" should be scalar',
                $field
            ));
        }
    }
}
