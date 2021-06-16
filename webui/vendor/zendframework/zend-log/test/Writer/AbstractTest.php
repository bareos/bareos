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
use ReflectionObject;
use Zend\Log\Filter\Regex as RegexFilter;
use Zend\Log\FilterPluginManager;
use Zend\Log\Formatter\Simple as SimpleFormatter;
use Zend\Log\FormatterPluginManager;
use Zend\Log\Writer\FilterPluginManager as LegacyFilterPluginManager;
use Zend\Log\Writer\FormatterPluginManager as LegacyFormatterPluginManager;
use Zend\ServiceManager\ServiceManager;
use ZendTest\Log\TestAsset\ConcreteWriter;
use ZendTest\Log\TestAsset\ErrorGeneratingWriter;

class AbstractTest extends TestCase
{
    protected $writer;

    protected function setUp()
    {
        $this->writer = new ConcreteWriter();
    }

    public function testSetSimpleFormatterByName()
    {
        $instance = $this->writer->setFormatter('simple');
        $this->assertAttributeInstanceOf('Zend\Log\Formatter\Simple', 'formatter', $instance);
    }

    public function testAddFilter()
    {
        $this->writer->addFilter(1);
        $this->writer->addFilter(new RegexFilter('/mess/'));
        $this->expectException('Zend\Log\Exception\InvalidArgumentException');
        $this->writer->addFilter(new \stdClass());
    }

    public function testAddMockFilterByName()
    {
        $instance = $this->writer->addFilter('mock');
        $this->assertInstanceOf('ZendTest\Log\TestAsset\ConcreteWriter', $instance);
    }

    public function testAddRegexFilterWithParamsByName()
    {
        $instance = $this->writer->addFilter('regex', [ 'regex' => '/mess/' ]);
        $this->assertInstanceOf('ZendTest\Log\TestAsset\ConcreteWriter', $instance);
    }

    /**
     * @group ZF-8953
     */
    public function testFluentInterface()
    {
        $instance = $this->writer->addFilter(1)
                                  ->setFormatter(new SimpleFormatter());

        $this->assertInstanceOf('ZendTest\Log\TestAsset\ConcreteWriter', $instance);
    }

    public function testConvertErrorsToException()
    {
        $writer = new ErrorGeneratingWriter();
        $this->expectException('Zend\Log\Exception\RuntimeException');
        $writer->write(['message' => 'test']);

        $writer->setConvertWriteErrorsToExceptions(false);
        $this->expectException('PHPUnit_Framework_Error_Warning');
        $writer->write(['message' => 'test']);
    }

    public function testConstructorWithOptions()
    {
        $options = ['filters' => [
                             [
                                 'name' => 'mock',
                             ],
                             [
                                 'name' => 'priority',
                                 'options' => [
                                     'priority' => 3,
                                 ],
                             ],
                         ],
                        'formatter' => [
                             'name' => 'base',
                         ],
                    ];

        $writer = new ConcreteWriter($options);

        $this->assertAttributeInstanceOf('Zend\Log\Formatter\Base', 'formatter', $writer);

        $filters = $this->readAttribute($writer, 'filters');
        $this->assertCount(2, $filters);

        $this->assertInstanceOf('Zend\Log\Filter\Priority', $filters[1]);
        $this->assertEquals(3, $this->readAttribute($filters[1], 'priority'));
    }

    public function testConstructorWithPriorityFilter()
    {
        // Accept an int as a PriorityFilter
        $writer = new ConcreteWriter(['filters' => 3]);
        $filters = $this->readAttribute($writer, 'filters');
        $this->assertCount(1, $filters);
        $this->assertInstanceOf('Zend\Log\Filter\Priority', $filters[0]);
        $this->assertEquals(3, $this->readAttribute($filters[0], 'priority'));

        // Accept an int in an array of filters as a PriorityFilter
        $options = ['filters' => [3, ['name' => 'mock']]];

        $writer = new ConcreteWriter($options);
        $filters = $this->readAttribute($writer, 'filters');
        $this->assertCount(2, $filters);
        $this->assertInstanceOf('Zend\Log\Filter\Priority', $filters[0]);
        $this->assertEquals(3, $this->readAttribute($filters[0], 'priority'));
        $this->assertInstanceOf('Zend\Log\Filter\Mock', $filters[1]);
    }

    /**
     * @covers \Zend\Log\Writer\AbstractWriter::__construct
     */
    public function testConstructorWithFormatterManager()
    {
        // Arrange
        $pluginManager = new FormatterPluginManager(new ServiceManager());

        // Act
        $writer = new ConcreteWriter([
            'formatter_manager' => $pluginManager,
        ]);

        // Assert
        $this->assertSame($pluginManager, $writer->getFormatterPluginManager());
    }

    /**
     * @covers \Zend\Log\Writer\AbstractWriter::__construct
     * @expectedException Zend\Log\Exception\InvalidArgumentException
     * @expectedExceptionMessage Writer plugin manager must extend Zend\Log\FormatterPluginManager; received integer
     */
    public function testConstructorWithInvalidFormatterManager()
    {
        // Arrange
        // There is nothing to arrange.

        // Act
        $writer = new ConcreteWriter([
            'formatter_manager' => 123,
        ]);

        // Assert
        // No assert needed, expecting an exception.
    }

    /**
     * @covers \Zend\Log\Writer\AbstractWriter::__construct
     */
    public function testConstructorWithLegacyFormatterManager()
    {
        // Arrange
        $pluginManager = new LegacyFormatterPluginManager(new ServiceManager());

        // Act
        $writer = new ConcreteWriter([
            'formatter_manager' => $pluginManager,
        ]);

        // Assert
        $this->assertSame($pluginManager, $writer->getFormatterPluginManager());
    }

    /**
     * @covers \Zend\Log\Writer\AbstractWriter::__construct
     */
    public function testConstructorWithFilterManager()
    {
        // Arrange
        $pluginManager = new FilterPluginManager(new ServiceManager());

        // Act
        $writer = new ConcreteWriter([
            'filter_manager' => $pluginManager,
        ]);

        // Assert
        $this->assertSame($pluginManager, $writer->getFilterPluginManager());
    }

    /**
     * @covers \Zend\Log\Writer\AbstractWriter::__construct
     * @expectedException Zend\Log\Exception\InvalidArgumentException
     * @expectedExceptionMessage Writer plugin manager must extend Zend\Log\FilterPluginManager; received integer
     */
    public function testConstructorWithInvalidFilterManager()
    {
        // Arrange
        // There is nothing to arrange.

        // Act
        $writer = new ConcreteWriter([
            'filter_manager' => 123,
        ]);

        // Assert
        // Nothing to assert, expecting an exception.
    }

    /**
     * @covers \Zend\Log\Writer\AbstractWriter::__construct
     */
    public function testConstructorWithLegacyFilterManager()
    {
        // Arrange
        $pluginManager = new LegacyFilterPluginManager(new ServiceManager());

        // Act
        $writer = new ConcreteWriter([
            'filter_manager' => $pluginManager,
        ]);

        // Assert
        $this->assertSame($pluginManager, $writer->getFilterPluginManager());
    }

    /**
     * @covers \Zend\Log\Writer\AbstractWriter::getFormatter
     */
    public function testFormatterDefaultsToNull()
    {
        $r = new ReflectionObject($this->writer);
        $m = $r->getMethod('getFormatter');
        $m->setAccessible(true);
        $this->assertNull($m->invoke($this->writer));
    }

    /**
     * @covers \Zend\Log\Writer\AbstractWriter::getFormatter
     * @covers \Zend\Log\Writer\AbstractWriter::setFormatter
     */
    public function testCanSetFormatter()
    {
        $formatter = new SimpleFormatter;
        $this->writer->setFormatter($formatter);

        $r = new ReflectionObject($this->writer);
        $m = $r->getMethod('getFormatter');
        $m->setAccessible(true);
        $this->assertSame($formatter, $m->invoke($this->writer));
    }

    /**
     * @covers \Zend\Log\Writer\AbstractWriter::hasFormatter
     */
    public function testHasFormatter()
    {
        $r = new ReflectionObject($this->writer);
        $m = $r->getMethod('hasFormatter');
        $m->setAccessible(true);
        $this->assertFalse($m->invoke($this->writer));

        $this->writer->setFormatter(new SimpleFormatter);
        $this->assertTrue($m->invoke($this->writer));
    }
}
