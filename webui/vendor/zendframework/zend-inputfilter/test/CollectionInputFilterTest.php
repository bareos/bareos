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
use PHPUnit\Framework\TestCase;
use stdClass;
use Zend\InputFilter\BaseInputFilter;
use Zend\InputFilter\CollectionInputFilter;
use Zend\InputFilter\Exception\InvalidArgumentException;
use Zend\InputFilter\Exception\RuntimeException;
use Zend\InputFilter\Input;
use Zend\InputFilter\InputFilter;
use Zend\Validator\Digits;
use Zend\Validator\NotEmpty;

/**
 * @covers \Zend\InputFilter\CollectionInputFilter
 */
class CollectionInputFilterTest extends TestCase
{
    /**
     * @var CollectionInputFilter
     */
    protected $inputFilter;

    protected function setUp()
    {
        $this->inputFilter = new CollectionInputFilter();
    }

    public function testSetInputFilterWithInvalidTypeThrowsInvalidArgumentException()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(RuntimeException::class);
        $this->expectExceptionMessage('expects an instance of Zend\InputFilter\BaseInputFilter; received "stdClass"');
        /** @noinspection PhpParamsInspection */
        $inputFilter->setInputFilter(new stdClass());
    }

    /**
     * @dataProvider inputFilterProvider
     */
    public function testSetInputFilter($inputFilter, $expectedType)
    {
        $this->inputFilter->setInputFilter($inputFilter);

        $this->assertInstanceOf($expectedType, $this->inputFilter->getInputFilter(), 'getInputFilter() type not match');
    }

    public function testGetDefaultInputFilter()
    {
        $this->assertInstanceOf(BaseInputFilter::class, $this->inputFilter->getInputFilter());
    }

    /**
     * @dataProvider isRequiredProvider
     */
    public function testSetRequired($value)
    {
        $this->inputFilter->setIsRequired($value);
        $this->assertEquals($value, $this->inputFilter->getIsRequired());
    }

    /**
     * @dataProvider countVsDataProvider
     */
    public function testSetCount($count, $data, $expectedCount)
    {
        if ($count !== null) {
            $this->inputFilter->setCount($count);
        }
        if ($data !== null) {
            $this->inputFilter->setData($data);
        }

        $this->assertEquals($expectedCount, $this->inputFilter->getCount(), 'getCount() value not match');
    }

    /**
     * @group 6160
     */
    public function testGetCountReturnsRightCountOnConsecutiveCallsWithDifferentData()
    {
        $collectionData1 = [
            ['foo' => 'bar'],
            ['foo' => 'baz'],
        ];

        $collectionData2 = [
            ['foo' => 'bar'],
        ];

        $this->inputFilter->setData($collectionData1);
        $this->assertEquals(2, $this->inputFilter->getCount());
        $this->inputFilter->setData($collectionData2);
        $this->assertEquals(1, $this->inputFilter->getCount());
    }

    /**
     * @dataProvider dataVsValidProvider
     */
    public function testDataVsValid(
        $required,
        $count,
        $data,
        $inputFilter,
        $expectedRaw,
        $expectedValues,
        $expectedValid,
        $expectedMessages
    ) {
        $this->inputFilter->setInputFilter($inputFilter);
        $this->inputFilter->setData($data);
        if ($count !== null) {
            $this->inputFilter->setCount($count);
        }
        $this->inputFilter->setIsRequired($required);

        $this->assertEquals(
            $expectedValid,
            $this->inputFilter->isValid(),
            'isValid() value not match. Detail . ' . json_encode($this->inputFilter->getMessages())
        );
        $this->assertEquals($expectedRaw, $this->inputFilter->getRawValues(), 'getRawValues() value not match');
        $this->assertEquals($expectedValues, $this->inputFilter->getValues(), 'getValues() value not match');
        $this->assertEquals($expectedMessages, $this->inputFilter->getMessages(), 'getMessages() value not match');
    }

    public function dataVsValidProvider()
    {
        $dataRaw = [
            'fooInput' => 'fooRaw',
        ];
        $dataFiltered = [
            'fooInput' => 'fooFiltered',
        ];
        $colRaw = [$dataRaw];
        $colFiltered = [$dataFiltered];
        $errorMessage = [
            'fooInput' => 'fooError',
        ];
        $colMessages = [$errorMessage];

        $invalidIF = function () use ($dataRaw, $dataFiltered, $errorMessage) {
            return $this->createBaseInputFilterMock(false, $dataRaw, $dataFiltered, $errorMessage);
        };
        $validIF = function () use ($dataRaw, $dataFiltered) {
            return $this->createBaseInputFilterMock(true, $dataRaw, $dataFiltered);
        };
        $noValidIF = function () use ($dataRaw, $dataFiltered) {
            return $this->createBaseInputFilterMock(null, $dataRaw, $dataFiltered);
        };
        $isRequired = true;

        // @codingStandardsIgnoreStart
        $dataSets = [
            // Description => [$required, $count, $data, $inputFilter, $expectedRaw, $expectedValues, $expectedValid, $expectedMessages]
            'Required: T, Count: N, Valid: T'  => [ $isRequired, null, $colRaw, $validIF  , $colRaw, $colFiltered, true , []],
            'Required: T, Count: N, Valid: F'  => [ $isRequired, null, $colRaw, $invalidIF, $colRaw, $colFiltered, false, $colMessages],
            'Required: T, Count: +1, Valid: F' => [ $isRequired,    2, $colRaw, $invalidIF, $colRaw, $colFiltered, false, $colMessages],
            'Required: F, Count: N, Valid: T'  => [!$isRequired, null, $colRaw, $validIF  , $colRaw, $colFiltered, true , []],
            'Required: F, Count: N, Valid: F'  => [!$isRequired, null, $colRaw, $invalidIF, $colRaw, $colFiltered, false, $colMessages],
            'Required: F, Count: +1, Valid: F' => [!$isRequired,    2, $colRaw, $invalidIF, $colRaw, $colFiltered, false, $colMessages],
            'Required: T, Data: [], Valid: X'  => [ $isRequired, null, []     , $noValidIF, []     , []          , false, [['isEmpty' => 'Value is required and can\'t be empty']]],
            'Required: F, Data: [], Valid: X'  => [!$isRequired, null, []     , $noValidIF, []     , []          , true , []],
        ];
        // @codingStandardsIgnoreEnd

        array_walk(
            $dataSets,
            function (&$set) {
                // Create unique mock input instances for each set
                $inputFilter = $set[3]();

                $set[3] = $inputFilter;
            }
        );

        return $dataSets;
    }

    public function testSetValidationGroupUsingFormStyle()
    {
        $validationGroup = [
            'fooGroup',
        ];
        $colValidationGroup = [$validationGroup];

        $dataRaw = [
            'fooInput' => 'fooRaw',
        ];
        $dataFiltered = [
            'fooInput' => 'fooFiltered',
        ];
        $colRaw = [$dataRaw];
        $colFiltered = [$dataFiltered];
        $baseInputFilter = $this->createBaseInputFilterMock(true, $dataRaw, $dataFiltered);
        $baseInputFilter->expects($this->once())
            ->method('setValidationGroup')
            ->with($validationGroup)
        ;

        $this->inputFilter->setInputFilter($baseInputFilter);
        $this->inputFilter->setData($colRaw);
        $this->inputFilter->setValidationGroup($colValidationGroup);

        $this->assertTrue(
            $this->inputFilter->isValid(),
            'isValid() value not match. Detail . ' . json_encode($this->inputFilter->getMessages())
        );
        $this->assertEquals($colRaw, $this->inputFilter->getRawValues(), 'getRawValues() value not match');
        $this->assertEquals($colFiltered, $this->inputFilter->getValues(), 'getValues() value not match');
        $this->assertEquals([], $this->inputFilter->getMessages(), 'getMessages() value not match');
    }

    public function dataNestingCollection()
    {
        return [
            'count not specified' => [
                'count' => null,
                'isValid' => true,
            ],
            'count=0' => [
                'count' => 0,
                'isValid' => true,
            ],
            'count = 1' => [
                'count' => 1,
                'isValid' => true,
            ],
            'count = 2' => [
                'count' => 2,
                'isValid' => false,
            ],
            'count = 3' => [
                'count' => 3,
                'isValid' => false,
            ],
        ];
    }

    /**
     * @dataProvider dataNestingCollection
     */
    public function testNestingCollectionCountCached($count, $expectedIsValid)
    {
        $firstInputFilter = new InputFilter();

        $firstCollection = new CollectionInputFilter();
        $firstCollection->setInputFilter($firstInputFilter);

        $someInput = new Input('input');
        $secondInputFilter = new InputFilter();
        $secondInputFilter->add($someInput, 'input');

        $secondCollection = new CollectionInputFilter();
        $secondCollection->setInputFilter($secondInputFilter);
        if (! is_null($count)) {
            $secondCollection->setCount($count);
        }

        $firstInputFilter->add($secondCollection, 'second_collection');

        $mainInputFilter = new InputFilter();
        $mainInputFilter->add($firstCollection, 'first_collection');

        $data = [
            'first_collection' => [
                [
                    'second_collection' => [
                        [
                            'input' => 'some value',
                        ],
                        [
                            'input' => 'some value',
                        ],
                    ],
                ],
                [
                    'second_collection' => [
                        [
                            'input' => 'some value',
                        ],
                    ],
                ],
            ],
        ];

        $mainInputFilter->setData($data);
        $this->assertSame($expectedIsValid, $mainInputFilter->isValid());
    }

    public function inputFilterProvider()
    {
        $baseInputFilter = new BaseInputFilter();

        $inputFilterSpecificationAsArray = [];
        $inputSpecificationAsTraversable = new ArrayIterator($inputFilterSpecificationAsArray);

        $inputFilterSpecificationResult = new InputFilter();
        $inputFilterSpecificationResult->getFactory()->getInputFilterManager();

        $dataSets = [
            // Description => [inputFilter, $expectedType]
            'BaseInputFilter' => [$baseInputFilter, BaseInputFilter::class],
            'array' => [$inputFilterSpecificationAsArray, InputFilter::class],
            'Traversable' => [$inputSpecificationAsTraversable, InputFilter::class],
        ];

        return $dataSets;
    }

    public function countVsDataProvider()
    {
        $data0 = [];
        $data1 = [['A' => 'a']];
        $data2 = [['A' => 'a'], ['B' => 'b']];

        // @codingStandardsIgnoreStart
        return [
            // Description => [$count, $data, $expectedCount]
            'C:   -1, D: null' => [  -1, null  ,  0],
            'C:    0, D: null' => [   0, null  ,  0],
            'C:    1, D: null' => [   1, null  ,  1],
            'C: null, D:    0' => [null, $data0,  0],
            'C: null, D:    1' => [null, $data1,  1],
            'C: null, D:    2' => [null, $data2,  2],
            'C:   -1, D:    0' => [  -1, $data0,  0],
            'C:    0, D:    0' => [   0, $data0,  0],
            'C:    1, D:    0' => [   1, $data0,  1],
            'C:   -1, D:    1' => [  -1, $data1,  0],
            'C:    0, D:    1' => [   0, $data1,  0],
            'C:    1, D:    1' => [   1, $data1,  1],
        ];
        // @codingStandardsIgnoreEnd
    }

    public function isRequiredProvider()
    {
        return [
            'enabled' => [true],
            'disabled' => [false],
        ];
    }

    /**
     * @param null|bool $isValid
     * @param mixed[] $getRawValues
     * @param mixed[] $getValues
     * @param string[] $getMessages
     *
     * @return MockObject|BaseInputFilter
     */
    protected function createBaseInputFilterMock(
        $isValid = null,
        $getRawValues = [],
        $getValues = [],
        $getMessages = []
    ) {
        /** @var BaseInputFilter|MockObject $inputFilter */
        $inputFilter = $this->createMock(BaseInputFilter::class);
        $inputFilter->method('getRawValues')
            ->willReturn($getRawValues)
        ;
        $inputFilter->method('getValues')
            ->willReturn($getValues)
        ;
        if (($isValid === false) || ($isValid === true)) {
            $inputFilter->expects($this->once())
                ->method('isValid')
                ->willReturn($isValid)
            ;
        } else {
            $inputFilter->expects($this->never())
                ->method('isValid')
            ;
        }
        $inputFilter->method('getMessages')
            ->willReturn($getMessages)
        ;

        return $inputFilter;
    }

    public function testGetUnknownWhenDataAreNotProvidedThrowsRuntimeException()
    {
        $this->expectException(RuntimeException::class);

        $this->inputFilter->getUnknown();
    }

    public function testGetUnknownWhenAllFieldsAreKnownReturnsAnEmptyArray()
    {
        $inputFilter = new InputFilter();
        $inputFilter->add([
            'name' => 'foo',
        ]);

        $collectionInputFilter = $this->inputFilter;
        $collectionInputFilter->setInputFilter($inputFilter);

        $collectionInputFilter->setData([
            ['foo' => 'bar'],
            ['foo' => 'baz'],
        ]);

        $unknown = $collectionInputFilter->getUnknown();

        $this->assertFalse($collectionInputFilter->hasUnknown());
        $this->assertCount(0, $unknown);
    }

    public function testGetUnknownFieldIsUnknown()
    {
        $inputFilter = new InputFilter();
        $inputFilter->add([
            'name' => 'foo',
        ]);

        $collectionInputFilter = $this->inputFilter;
        $collectionInputFilter->setInputFilter($inputFilter);

        $collectionInputFilter->setData([
            ['foo' => 'bar', 'baz' => 'hey'],
            ['foo' => 'car', 'tor' => 'ver'],
        ]);

        $unknown = $collectionInputFilter->getUnknown();

        $this->assertTrue($collectionInputFilter->hasUnknown());
        $this->assertEquals([['baz' => 'hey'], ['tor' => 'ver']], $unknown);
    }

    public function invalidCollections()
    {
        return [
            'null'       => [[['this' => 'is valid'], null]],
            'false'      => [[['this' => 'is valid'], false]],
            'true'       => [[['this' => 'is valid'], true]],
            'zero'       => [[['this' => 'is valid'], 0]],
            'int'        => [[['this' => 'is valid'], 1]],
            'zero-float' => [[['this' => 'is valid'], 0.0]],
            'float'      => [[['this' => 'is valid'], 1.1]],
            'string'     => [[['this' => 'is valid'], 'this is not']],
            'object'     => [[['this' => 'is valid'], (object) ['this' => 'is invalid']]],
        ];
    }

    /**
     * @dataProvider invalidCollections
     */
    public function testSettingDataAsArrayWithInvalidCollectionsRaisesException($data)
    {
        $collectionInputFilter = $this->inputFilter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('invalid item in collection');
        $collectionInputFilter->setData($data);
    }

    /**
     * @dataProvider invalidCollections
     */
    public function testSettingDataAsTraversableWithInvalidCollectionsRaisesException($data)
    {
        $collectionInputFilter = $this->inputFilter;
        $data = new ArrayIterator($data);

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('invalid item in collection');
        $collectionInputFilter->setData($data);
    }

    public function invalidDataType()
    {
        return [
            'null'       => [null],
            'false'      => [false],
            'true'       => [true],
            'zero'       => [0],
            'int'        => [1],
            'zero-float' => [0.0],
            'float'      => [1.1],
            'string'     => ['this is not'],
            'object'     => [(object) ['this' => 'is invalid']],
        ];
    }

    /**
     * @dataProvider invalidDataType
     */
    public function testSettingDataWithNonArrayNonTraversableRaisesException($data)
    {
        $collectionInputFilter = $this->inputFilter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('invalid collection');
        $collectionInputFilter->setData($data);
    }

    public function testCollectionValidationDoesNotReuseMessagesBetweenInputs()
    {
        $inputFilter = new InputFilter();
        $inputFilter->add([
            'name' => 'phone',
            'required' => true,
            'validators' => [
                ['name' => Digits::class],
                ['name' => NotEmpty::class],
            ],
        ]);
        $inputFilter->add([
            'name' => 'name',
            'required' => true,
            'validators' => [
                ['name' => NotEmpty::class],
            ],
        ]);

        $collectionInputFilter = $this->inputFilter;
        $collectionInputFilter->setInputFilter($inputFilter);

        $collectionInputFilter->setData([
            [
                'name' => 'Tom',
            ],
            [
                'phone' => 'tom@tom',
                'name' => 'Tom',
            ],
        ]);

        $isValid = $collectionInputFilter->isValid();
        $messages = $collectionInputFilter->getMessages();

        // @codingStandardsIgnoreStart
        $this->assertFalse($isValid);
        $this->assertCount(2, $messages);

        $this->assertArrayHasKey('phone', $messages[0]);
        $this->assertCount(1, $messages[0]['phone']);
        $this->assertContains('Value is required and can\'t be empty', $messages[0]['phone']);

        $this->assertArrayHasKey('phone', $messages[1]);
        $this->assertCount(1, $messages[1]['phone']);
        $this->assertNotContains('Value is required and can\'t be empty', $messages[1]['phone']);
        $this->assertContains('The input must contain only digits', $messages[1]['phone']);
        // @codingStandardsIgnoreEnd
    }

    public function testCollectionValidationUsesCustomInputErrorMessages()
    {
        $inputFilter = new InputFilter();
        $inputFilter->add([
            'name' => 'phone',
            'required' => true,
            'validators' => [
                ['name' => Digits::class],
                ['name' => NotEmpty::class],
            ],
            'error_message' => 'CUSTOM ERROR MESSAGE',
        ]);
        $inputFilter->add([
            'name' => 'name',
            'required' => true,
            'validators' => [
                ['name' => NotEmpty::class],
            ],
        ]);

        $collectionInputFilter = $this->inputFilter;
        $collectionInputFilter->setInputFilter($inputFilter);

        $collectionInputFilter->setData([
            [
                'name' => 'Tom',
            ],
            [
                'phone' => 'tom@tom',
                'name' => 'Tom',
            ],
        ]);

        $isValid = $collectionInputFilter->isValid();
        $messages = $collectionInputFilter->getMessages();


        $this->assertFalse($isValid);
        $this->assertCount(2, $messages);

        $this->assertArrayHasKey('phone', $messages[0]);
        $this->assertCount(1, $messages[0]['phone']);
        $this->assertContains('CUSTOM ERROR MESSAGE', $messages[0]['phone']);
        $this->assertNotContains('Value is required and can\'t be empty', $messages[0]['phone']);

        $this->assertArrayHasKey('phone', $messages[1]);
        $this->assertCount(1, $messages[1]['phone']);
        $this->assertContains('CUSTOM ERROR MESSAGE', $messages[1]['phone']);
    }

    public function testDuplicatedErrorMessages()
    {
        $factory = new \Zend\InputFilter\Factory();
        $inputFilter = $factory->createInputFilter(
            [
                'element' => [
                    'type' => InputFilter::class,
                    'type1' => [
                        'type' => CollectionInputFilter::class,
                        'input_filter' => [
                            'test_field' => [
                                'type' => CollectionInputFilter::class,
                                'input_filter' => [
                                    'test_field1' => [
                                        'required' => false,
                                        'validators' => [
                                            [
                                                'name' => \Zend\Validator\Between::class,
                                                'options' => [
                                                    'min' => 50,
                                                    'max' => 100,
                                                    'message' => '%value% is incorrect',
                                                ],
                                            ],
                                        ],
                                    ],
                                    'price' => [
                                        'required' => false,
                                        'validators' => [
                                            [
                                                'name' => \Zend\Validator\Between::class,
                                                'options' => [
                                                    'min' => 50,
                                                    'max' => 100,
                                                    'message' => '%value% is incorrect',
                                                ],
                                            ],
                                        ],
                                    ],
                                ],
                            ],
                        ],
                    ],
                ],
            ]
        );

        $inputFilter->setData(
            [
                'element' => [
                    'type1' => [
                        [
                            'test_field' => [
                                [
                                    'test_field1' => -20,
                                    'price' => 20,
                                ],
                                [
                                    'test_field1' => -15,
                                    'price' => 15,
                                ],
                                [
                                    'test_field1' => -10,
                                    'price' => 10,
                                ],
                            ],
                        ],
                        [
                            'test_field' => [
                                [
                                    'test_field1' => -5,
                                    'price' => 5,
                                ],
                            ],
                        ],
                    ],
                ],
            ]
        );
        $this->assertFalse($inputFilter->isValid());
        $this->assertEquals([
            'element' => [
                'type1' => [
                    [
                        'test_field' => [
                            [
                                'test_field1' => [
                                    'notBetween' => '-20 is incorrect',
                                ],
                                'price' => [
                                    'notBetween' => '20 is incorrect',
                                ],
                            ],
                            [
                                'test_field1' => [
                                    'notBetween' => '-15 is incorrect',
                                ],
                                'price' => [
                                    'notBetween' => '15 is incorrect',
                                ],
                            ],
                            [
                                'test_field1' => [
                                    'notBetween' => '-10 is incorrect',
                                ],
                                'price' => [
                                    'notBetween' => '10 is incorrect',
                                ],
                            ],
                        ],
                    ],
                    [
                        'test_field' => [
                            [
                                'test_field1' => [
                                    'notBetween' => '-5 is incorrect',
                                ],
                                'price' => [
                                    'notBetween' => '5 is incorrect',
                                ],
                            ],
                        ],
                    ],
                ],
            ],
        ], $inputFilter->getMessages());
    }

    public function testLazyLoadsANotEmptyValidatorWhenNoneProvided()
    {
        $this->assertInstanceOf(NotEmpty::class, $this->inputFilter->getNotEmptyValidator());
    }

    public function testAllowsComposingANotEmptyValidator()
    {
        $notEmptyValidator = new NotEmpty();
        $this->inputFilter->setNotEmptyValidator($notEmptyValidator);
        $this->assertSame($notEmptyValidator, $this->inputFilter->getNotEmptyValidator());
    }

    public function testUsesMessageFromComposedNotEmptyValidatorWhenRequiredButCollectionIsEmpty()
    {
        $message = 'this is the validation message';
        $notEmptyValidator = new NotEmpty();
        $notEmptyValidator->setMessage($message);

        $this->inputFilter->setIsRequired(true);
        $this->inputFilter->setNotEmptyValidator($notEmptyValidator);

        $this->inputFilter->setData([]);

        $this->assertFalse($this->inputFilter->isValid());

        $this->assertEquals([
            [NotEmpty::IS_EMPTY => $message],
        ], $this->inputFilter->getMessages());
    }

    public function testSetDataUsingSetDataAndRunningIsValidReturningSameAsOriginalForUnfilteredData()
    {
        $filteredArray = [
            [
                'bar' => 'foo',
                'foo' => 'bar',
            ],
        ];

        $unfilteredArray = array_merge(
            $filteredArray,
            [
                [
                    'foo' => 'bar',
                ],
            ]
        );

        /** @var BaseInputFilter $baseInputFilter */
        $baseInputFilter = (new BaseInputFilter())
            ->add(new Input(), 'bar');

        /** @var CollectionInputFilter $collectionInputFilter */
        $collectionInputFilter = (new CollectionInputFilter())->setInputFilter($baseInputFilter);
        $collectionInputFilter->setData($unfilteredArray);

        $collectionInputFilter->isValid();

        self::assertSame($unfilteredArray, $collectionInputFilter->getUnfilteredData());
    }
}
