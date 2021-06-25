<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\InputFilter;

use ArrayIterator;
use PHPUnit_Framework_MockObject_MockObject as MockObject;
use stdClass;
use Zend\InputFilter\Factory;
use Zend\InputFilter\Input;
use Zend\InputFilter\InputFilter;

/**
 * @covers \Zend\InputFilter\InputFilter
 */
class InputFilterTest extends BaseInputFilterTest
{
    /**
     * @var InputFilter
     */
    protected $inputFilter;

    protected function setUp()
    {
        $this->inputFilter = new InputFilter();
    }

    public function testLazilyComposesAFactoryByDefault()
    {
        $factory = $this->inputFilter->getFactory();
        $this->assertInstanceOf(Factory::class, $factory);
    }

    public function testCanComposeAFactory()
    {
        $factory = $this->createFactoryMock();
        $this->inputFilter->setFactory($factory);
        $this->assertSame($factory, $this->inputFilter->getFactory());
    }

    public function inputProvider()
    {
        $dataSets = parent::inputProvider();

        $inputSpecificationAsArray = [
            'name' => 'inputFoo',
        ];
        $inputSpecificationAsTraversable = new ArrayIterator($inputSpecificationAsArray);

        $inputSpecificationResult = new Input('inputFoo');
        $inputSpecificationResult->getFilterChain(); // Fill input with a default chain just for make the test pass
        $inputSpecificationResult->getValidatorChain(); // Fill input with a default chain just for make the test pass

        // @codingStandardsIgnoreStart
        $inputFilterDataSets = [
            // Description => [input, expected name, $expectedReturnInput]
            'array' =>       [$inputSpecificationAsArray      , 'inputFoo', $inputSpecificationResult],
            'Traversable' => [$inputSpecificationAsTraversable, 'inputFoo', $inputSpecificationResult],
        ];
        // @codingStandardsIgnoreEnd
        $dataSets = array_merge($dataSets, $inputFilterDataSets);

        return $dataSets;
    }

    /**
     * @return Factory|MockObject
     */
    protected function createFactoryMock()
    {
        /** @var Factory|MockObject $factory */
        $factory = $this->createMock(Factory::class);

        return $factory;
    }

    /**
     * Particularly in APIs, a null value may be passed for a set of data
     * rather than an object or array. This ensures that doing so will
     * work consistently with passing an empty array.
     *
     * @see https://github.com/zendframework/zend-inputfilter/issues/159
     */
    public function testNestedInputFilterShouldAllowNullValueForData()
    {
        $filter1 = new InputFilter();
        $filter1->add([
            'type' => InputFilter::class,
            'nestedField1' => [
                'required' => false
            ]
        ], 'nested');

        // Empty set of data
        $filter1->setData([]);
        self::assertNull($filter1->getValues()['nested']['nestedField1']);

        // null provided for nested filter
        $filter1->setData(['nested' => null]);
        self::assertNull($filter1->getValues()['nested']['nestedField1']);
    }
}
