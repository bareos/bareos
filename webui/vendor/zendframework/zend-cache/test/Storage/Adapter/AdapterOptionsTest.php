<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use PHPUnit\Framework\TestCase;
use Zend\Cache\Storage\StorageInterface;
use Zend\Cache\Storage\Event;
use Zend\Cache\Storage\Adapter\AbstractAdapter;
use Zend\Cache\Storage\Adapter\AdapterOptions;
use Zend\Cache\Exception;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\AdapterOptions<extended>
 */
class AdapterOptionsTest extends TestCase
{
    /**
     * Mock of the storage
     *
     * @var StorageInterface
     */
    protected $storage;

    /**
     * Adapter options
     *
     * @var null|AdapterOptions
     */
    protected $options;

    public function setUp()
    {
        $this->options = new AdapterOptions();
    }

    public function testKeyPattern()
    {
        // test default value
        $this->assertSame('', $this->options->getKeyPattern());

        $this->assertSame($this->options, $this->options->setKeyPattern('/./'));
        $this->assertSame('/./', $this->options->getKeyPattern());
    }

    public function testSetKeyPatternAllowEmptyString()
    {
        // first change to something different as an empty string is the default
        $this->options->setKeyPattern('/.*/');

        $this->options->setKeyPattern('');
        $this->assertSame('', $this->options->getKeyPattern());
    }

    public function testSetKeyPatternThrowsInvalidArgumentExceptionOnInvalidPattern()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->options->setKeyPattern('foo bar');
    }

    public function testNamespace()
    {
        $this->assertSame($this->options, $this->options->setNamespace('foobar'));
        $this->assertSame('foobar', $this->options->getNamespace());
    }

    public function testReadable()
    {
        $this->assertSame($this->options, $this->options->setReadable(false));
        $this->assertSame(false, $this->options->getReadable());

        $this->assertSame($this->options, $this->options->setReadable(true));
        $this->assertSame(true, $this->options->getReadable());
    }

    public function testWritable()
    {
        $this->assertSame($this->options, $this->options->setWritable(false));
        $this->assertSame(false, $this->options->getWritable());

        $this->assertSame($this->options, $this->options->setWritable(true));
        $this->assertSame(true, $this->options->getWritable());
    }

    public function testTtl()
    {
        // infinite default value
        $this->assertSame(0, $this->options->getTtl());

        $this->assertSame($this->options, $this->options->setTtl(12345));
        $this->assertSame(12345, $this->options->getTtl());
    }

    public function testSetTtlThrowsInvalidArgumentExceptionOnNegativeValue()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->options->setTtl(-1);
    }

    public function testSetTtlAutoconvertToIntIfPossible()
    {
        $this->options->setTtl(12345.0);
        $this->assertSame(12345, $this->options->getTtl());

        $this->options->setTtl(12345.678);
        $this->assertSame(12345.678, $this->options->getTtl());
    }

    public function testTriggerOptionEvent()
    {
        // setup an adapter implements EventsCapableInterface
        $adapter = $this->getMockForAbstractClass(AbstractAdapter::class);
        $this->options->setAdapter($adapter);

        // setup event listener
        $calledArgs = null;
        $adapter->getEventManager()->attach('option', function () use (& $calledArgs) {
            $calledArgs = func_get_args();
        });

        // trigger by changing an option
        $this->options->setWritable(false);

        // assert (hopefully) called listener and arguments
        $this->assertCount(1, $calledArgs, '"option" event was not triggered or got a wrong number of arguments');
        $this->assertInstanceOf(Event::class, $calledArgs[0]);
        $this->assertEquals(['writable' => false], $calledArgs[0]->getParams()->getArrayCopy());
    }

    public function testSetFromArrayWithoutPrioritizedOptions()
    {
        $this->assertSame($this->options, $this->options->setFromArray([
            'kEy_pattERN' => '/./',
            'nameSPACE'   => 'foobar',
        ]));
        $this->assertSame('/./', $this->options->getKeyPattern());
        $this->assertSame('foobar', $this->options->getNamespace());
    }

    public function testSetFromArrayWithPrioritizedOptions()
    {
        $options = $this->getMockBuilder(AdapterOptions::class)
            ->setMethods(['setKeyPattern', 'setNamespace', 'setWritable'])
            ->getMock();

        // set key_pattern and namespace to be a prioritized options
        $optionsRef = new \ReflectionObject($options);
        $propRef    = $optionsRef->getProperty('__prioritizedProperties__');
        $propRef->setAccessible(true);
        $propRef->setValue($options, ['key_pattern', 'namespace']);

        // expected order of setter be called
        $options->expects($this->at(0))->method('setKeyPattern')->with($this->equalTo('/./'));
        $options->expects($this->at(1))->method('setNamespace')->with($this->equalTo('foobar'));
        $options->expects($this->at(2))->method('setWritable')->with($this->equalTo(false));

        // send unordered options array
        $this->assertSame($options, $options->setFromArray([
            'nAmeSpace'   => 'foobar',
            'WriTAble'    => false,
            'KEY_paTTern' => '/./',
        ]));
    }
}
