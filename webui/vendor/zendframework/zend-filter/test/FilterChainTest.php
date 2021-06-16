<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Filter;

use ArrayIterator;
use PHPUnit\Framework\TestCase;
use Zend\Filter\FilterChain;
use Zend\Filter\PregReplace;
use Zend\Filter\StringToLower;
use Zend\Filter\StringTrim;
use Zend\Filter\StripTags;

class FilterChainTest extends TestCase
{
    public function testEmptyFilterChainReturnsOriginalValue()
    {
        $chain = new FilterChain();
        $value = 'something';
        $this->assertEquals($value, $chain->filter($value));
    }

    public function testFiltersAreExecutedInFifoOrder()
    {
        $chain = new FilterChain();
        $chain->attach(new TestAsset\LowerCase())
            ->attach(new TestAsset\StripUpperCase());
        $value         = 'AbC';
        $valueExpected = 'abc';
        $this->assertEquals($valueExpected, $chain->filter($value));
    }

    public function testFiltersAreExecutedAccordingToPriority()
    {
        $chain = new FilterChain();
        $chain->attach(new TestAsset\StripUpperCase())
            ->attach(new TestAsset\LowerCase, 100);
        $value         = 'AbC';
        $valueExpected = 'b';
        $this->assertEquals($valueExpected, $chain->filter($value));
    }

    public function testAllowsConnectingArbitraryCallbacks()
    {
        $chain = new FilterChain();
        $chain->attach(function ($value) {
            return strtolower($value);
        });
        $value = 'AbC';
        $this->assertEquals('abc', $chain->filter($value));
    }

    public function testAllowsConnectingViaClassShortName()
    {
        if (! function_exists('mb_strtolower')) {
            $this->markTestSkipped('mbstring required');
        }

        $chain = new FilterChain();
        $chain->attachByName(StringTrim::class, null, 100)
            ->attachByName(StripTags::class)
            ->attachByName(StringToLower::class, ['encoding' => 'utf-8'], 900);

        $value         = '<a name="foo"> ABC </a>';
        $valueExpected = 'abc';
        $this->assertEquals($valueExpected, $chain->filter($value));
    }

    public function testAllowsConfiguringFilters()
    {
        $config = $this->getChainConfig();
        $chain  = new FilterChain();
        $chain->setOptions($config);
        $value         = '<a name="foo"> abc </a><img id="bar" />';
        $valueExpected = 'ABC <IMG ID="BAR" />';
        $this->assertEquals($valueExpected, $chain->filter($value));
    }

    public function testAllowsConfiguringFiltersViaConstructor()
    {
        $config        = $this->getChainConfig();
        $chain         = new FilterChain($config);
        $value         = '<a name="foo"> abc </a>';
        $valueExpected = 'ABC';
        $this->assertEquals($valueExpected, $chain->filter($value));
    }

    public function testConfigurationAllowsTraversableObjects()
    {
        $config        = $this->getChainConfig();
        $config        = new ArrayIterator($config);
        $chain         = new FilterChain($config);
        $value         = '<a name="foo"> abc </a>';
        $valueExpected = 'ABC';
        $this->assertEquals($valueExpected, $chain->filter($value));
    }

    public function testCanRetrieveFilterWithUndefinedConstructor()
    {
        $chain    = new FilterChain([
            'filters' => [
                ['name' => 'int'],
            ],
        ]);
        $filtered = $chain->filter('127.1');
        $this->assertEquals(127, $filtered);
    }

    protected function getChainConfig()
    {
        return [
            'callbacks' => [
                ['callback' => __CLASS__ . '::staticUcaseFilter'],
                [
                    'priority' => 10000,
                    'callback' => function ($value) {
                        return trim($value);
                    }
                ],
            ],
            'filters'   => [
                [
                    'name'     => StripTags::class,
                    'options'  => ['allowTags' => 'img', 'allowAttribs' => 'id'],
                    'priority' => 10100
                ],
            ],
        ];
    }

    public static function staticUcaseFilter($value)
    {
        return strtoupper($value);
    }

    /**
     * @group ZF-412
     */
    public function testCanAttachMultipleFiltersOfTheSameTypeAsDiscreteInstances()
    {
        $chain = new FilterChain();
        $chain->attachByName(PregReplace::class, [
            'pattern'     => '/Foo/',
            'replacement' => 'Bar',
        ]);
        $chain->attachByName(PregReplace::class, [
            'pattern'     => '/Bar/',
            'replacement' => 'PARTY',
        ]);

        $this->assertEquals(2, count($chain));
        $filters = $chain->getFilters();
        $compare = null;
        foreach ($filters as $filter) {
            $this->assertNotSame($compare, $filter);
            $compare = $filter;
        }

        $this->assertEquals('Tu et PARTY', $chain->filter('Tu et Foo'));
    }

    public function testClone()
    {
        $chain = new FilterChain();
        $clone = clone $chain;

        $chain->attachByName(StripTags::class);

        $this->assertCount(0, $clone);
    }

    public function testCanSerializeFilterChain()
    {
        $chain = new FilterChain();
        $chain->attach(new TestAsset\LowerCase())
            ->attach(new TestAsset\StripUpperCase());
        $serialized = serialize($chain);

        $unserialized = unserialize($serialized);
        $this->assertInstanceOf('Zend\Filter\FilterChain', $unserialized);
        $this->assertEquals(2, count($unserialized));
        $value         = 'AbC';
        $valueExpected = 'abc';
        $this->assertEquals($valueExpected, $unserialized->filter($value));
    }

    public function testMergingTwoFilterChainsKeepFiltersPriority()
    {
        $value         = 'AbC';
        $valueExpected = 'abc';

        $chain = new FilterChain();
        $chain->attach(new TestAsset\StripUpperCase())
            ->attach(new TestAsset\LowerCase(), 1001);
        $this->assertEquals($valueExpected, $chain->filter($value));

        $chain = new FilterChain();
        $chain->attach(new TestAsset\LowerCase(), 1001)
            ->attach(new TestAsset\StripUpperCase());
        $this->assertEquals($valueExpected, $chain->filter($value));

        $chain = new FilterChain();
        $chain->attach(new TestAsset\LowerCase(), 1001);
        $chainToMerge = new FilterChain();
        $chainToMerge->attach(new TestAsset\StripUpperCase());
        $chain->merge($chainToMerge);
        $this->assertEquals(2, $chain->count());
        $this->assertEquals($valueExpected, $chain->filter($value));

        $chain = new FilterChain();
        $chain->attach(new TestAsset\StripUpperCase());
        $chainToMerge = new FilterChain();
        $chainToMerge->attach(new TestAsset\LowerCase(), 1001);
        $chain->merge($chainToMerge);
        $this->assertEquals(2, $chain->count());
        $this->assertEquals($valueExpected, $chain->filter($value));
    }
}
