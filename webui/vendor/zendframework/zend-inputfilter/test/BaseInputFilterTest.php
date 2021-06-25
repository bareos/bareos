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
use ArrayObject;
use FilterIterator;
use PHPUnit\Framework\TestCase;
use PHPUnit_Framework_MockObject_MockObject as MockObject;
use stdClass;
use Zend\InputFilter\BaseInputFilter;
use Zend\InputFilter\Exception\InvalidArgumentException;
use Zend\InputFilter\Exception\RuntimeException;
use Zend\InputFilter\Input;
use Zend\InputFilter\InputFilterInterface;
use Zend\InputFilter\InputInterface;
use Zend\InputFilter\UnfilteredDataInterface;

/**
 * @covers \Zend\InputFilter\BaseInputFilter
 */
class BaseInputFilterTest extends TestCase
{
    /**
     * @var BaseInputFilter
     */
    protected $inputFilter;

    protected function setUp()
    {
        $this->inputFilter = new BaseInputFilter();
    }

    public function testInputFilterIsEmptyByDefault()
    {
        $filter = $this->inputFilter;
        $this->assertEquals(0, count($filter));
    }

    public function testAddWithInvalidInputTypeThrowsInvalidArgumentException()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage(
            'expects an instance of Zend\InputFilter\InputInterface or Zend\InputFilter\InputFilterInterface ' .
            'as its first argument; received "stdClass"'
        );
        /** @noinspection PhpParamsInspection */
        $inputFilter->add(new stdClass());
    }

    public function testGetThrowExceptionIfInputDoesNotExists()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('no input found matching "not exists"');
        $inputFilter->get('not exists');
    }

    public function testReplaceWithInvalidInputTypeThrowsInvalidArgumentException()
    {
        $inputFilter = $this->inputFilter;
        $inputFilter->add(new Input('foo'), 'replace_me');

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage(
            'expects an instance of Zend\InputFilter\InputInterface or Zend\InputFilter\InputFilterInterface ' .
            'as its first argument; received "stdClass"'
        );
        /** @noinspection PhpParamsInspection */
        $inputFilter->replace(new stdClass(), 'replace_me');
    }

    public function testReplaceThrowExceptionIfInputToReplaceDoesNotExists()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('no input found matching "not exists"');
        $inputFilter->replace(new Input('foo'), 'not exists');
    }

    public function testGetValueThrowExceptionIfInputDoesNotExists()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('"not exists" was not found in the filter');
        $inputFilter->getValue('not exists');
    }

    public function testGetRawValueThrowExceptionIfInputDoesNotExists()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('"not exists" was not found in the filter');
        $inputFilter->getRawValue('not exists');
    }

    public function testSetDataWithInvalidDataTypeThrowsInvalidArgumentException()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('expects an array or Traversable argument; received stdClass');
        /** @noinspection PhpParamsInspection */
        $inputFilter->setData(new stdClass());
    }

    public function testIsValidThrowExceptionIfDataWasNotSetYet()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(RuntimeException::class);
        $this->expectExceptionMessage('no data present to validate');
        $inputFilter->isValid();
    }

    public function testSetValidationGroupSkipsRecursionWhenInputIsNotAnInputFilter()
    {
        $inputFilter = $this->inputFilter;

        /** @var InputInterface|MockObject $nestedInput */
        $nestedInput = $this->createMock(InputInterface::class);
        $inputFilter->add($nestedInput, 'fooInput');

        $inputFilter->setValidationGroup(['fooInput' => 'foo']);
        $this->assertAttributeEquals(['fooInput'], 'validationGroup', $inputFilter);
    }

    public function testSetValidationGroupAllowsSpecifyingArrayOfInputsToNestedInputFilter()
    {
        $inputFilter = $this->inputFilter;

        $nestedInputFilter = new BaseInputFilter();

        /** @var InputInterface|MockObject $nestedInput1 */
        $nestedInput1 = $this->createMock(InputInterface::class);
        $nestedInputFilter->add($nestedInput1, 'nested-input1');

        /** @var InputInterface|MockObject $nestedInput2 */
        $nestedInput2 = $this->createMock(InputInterface::class);
        $nestedInputFilter->add($nestedInput2, 'nested-input2');

        $inputFilter->add($nestedInputFilter, 'nested');

        $inputFilter->setValidationGroup(['nested' => ['nested-input1', 'nested-input2']]);

        $this->assertAttributeEquals(['nested'], 'validationGroup', $inputFilter);
        $this->assertAttributeEquals(['nested-input1', 'nested-input2'], 'validationGroup', $nestedInputFilter);
    }

    public function testSetValidationGroupThrowExceptionIfInputFilterNotExists()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage(
            'expects a list of valid input names; "anotherNotExistsInputFilter" was not found'
        );
        $inputFilter->setValidationGroup(['notExistInputFilter' => 'anotherNotExistsInputFilter']);
    }

    public function testSetValidationGroupThrowExceptionIfInputFilterInArgumentListNotExists()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage(
            'expects a list of valid input names; "notExistInputFilter" was not found'
        );
        $inputFilter->setValidationGroup('notExistInputFilter');
    }

    public function testHasUnknownThrowExceptionIfDataWasNotSetYet()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(RuntimeException::class);
        $inputFilter->hasUnknown();
    }

    public function testGetUnknownThrowExceptionIfDataWasNotSetYet()
    {
        $inputFilter = $this->inputFilter;

        $this->expectException(RuntimeException::class);
        $inputFilter->getUnknown();
    }

    /**
     * Verify the state of the input filter is the desired after change it using the method `add()`
     *
     * @dataProvider addMethodArgumentsProvider
     */
    public function testAddHasGet($input, $name, $expectedInputName, $expectedInput)
    {
        $inputFilter = $this->inputFilter;
        $this->assertFalse(
            $inputFilter->has($expectedInputName),
            "InputFilter shouldn't have an input with the name $expectedInputName yet"
        );
        $currentNumberOfFilters = count($inputFilter);

        $return = $inputFilter->add($input, $name);
        $this->assertSame($inputFilter, $return, "add() must return it self");

        // **Check input collection state**
        $this->assertTrue($inputFilter->has($expectedInputName), "There is no input with name $expectedInputName");
        $this->assertCount($currentNumberOfFilters + 1, $inputFilter, 'Number of filters must be increased by 1');

        $returnInput = $inputFilter->get($expectedInputName);
        $this->assertEquals($expectedInput, $returnInput, 'get() does not match the expected input');
    }

    /**
     * Verify the state of the input filter is the desired after change it using the method `add()` and `remove()`
     *
     * @dataProvider addMethodArgumentsProvider
     */
    public function testAddRemove($input, $name, $expectedInputName)
    {
        $inputFilter = $this->inputFilter;

        $inputFilter->add($input, $name);
        $currentNumberOfFilters = count($inputFilter);

        $return = $inputFilter->remove($expectedInputName);
        $this->assertSame($inputFilter, $return, 'remove() must return it self');

        $this->assertFalse($inputFilter->has($expectedInputName), "There is no input with name $expectedInputName");
        $this->assertCount($currentNumberOfFilters - 1, $inputFilter, 'Number of filters must be decreased by 1');
    }

    public function testAddingInputWithNameDoesNotInjectNameInInput()
    {
        $inputFilter = $this->inputFilter;

        $foo = new Input('foo');
        $inputFilter->add($foo, 'bas');

        $test = $inputFilter->get('bas');
        $this->assertSame($foo, $test, 'get() does not match the input added');
        $this->assertEquals('foo', $foo->getName(), 'Input name should not change');
    }

    /**
     * @dataProvider inputProvider
     */
    public function testReplace($input, $inputName, $expectedInput)
    {
        $inputFilter = $this->inputFilter;
        $nameToReplace = 'replace_me';
        $inputToReplace = new Input($nameToReplace);

        $inputFilter->add($inputToReplace);
        $currentNumberOfFilters = count($inputFilter);

        $return = $inputFilter->replace($input, $nameToReplace);
        $this->assertSame($inputFilter, $return, 'replace() must return it self');
        $this->assertCount($currentNumberOfFilters, $inputFilter, "Number of filters shouldn't change");

        $returnInput = $inputFilter->get($nameToReplace);
        $this->assertEquals($expectedInput, $returnInput, 'get() does not match the expected input');
    }

    /**
     * @dataProvider setDataArgumentsProvider
     */
    public function testSetDataAndGetRawValueGetValue(
        $inputs,
        $data,
        $expectedRawValues,
        $expectedValues,
        $expectedIsValid,
        $expectedInvalidInputs,
        $expectedValidInputs,
        $expectedMessages
    ) {
        $inputFilter = $this->inputFilter;
        foreach ($inputs as $inputName => $input) {
            $inputFilter->add($input, $inputName);
        }
        $return = $inputFilter->setData($data);
        $this->assertSame($inputFilter, $return, 'setData() must return it self');

        // ** Check filter state **
        $this->assertSame($expectedRawValues, $inputFilter->getRawValues(), 'getRawValues() value not match');
        foreach ($expectedRawValues as $inputName => $expectedRawValue) {
            $this->assertSame(
                $expectedRawValue,
                $inputFilter->getRawValue($inputName),
                'getRawValue() value not match for input ' . $inputName
            );
        }

        $this->assertSame($expectedValues, $inputFilter->getValues(), 'getValues() value not match');
        foreach ($expectedValues as $inputName => $expectedValue) {
            $this->assertSame(
                $expectedValue,
                $inputFilter->getValue($inputName),
                'getValue() value not match for input ' . $inputName
            );
        }

        // ** Check validation state **
        // @codingStandardsIgnoreStart
        $this->assertEquals($expectedIsValid, $inputFilter->isValid(), 'isValid() value not match');
        $this->assertEquals($expectedInvalidInputs, $inputFilter->getInvalidInput(), 'getInvalidInput() value not match');
        $this->assertEquals($expectedValidInputs, $inputFilter->getValidInput(), 'getValidInput() value not match');
        $this->assertEquals($expectedMessages, $inputFilter->getMessages(), 'getMessages() value not match');
        // @codingStandardsIgnoreEnd

        // ** Check unknown fields **
        $this->assertFalse($inputFilter->hasUnknown(), 'hasUnknown() value not match');
        $this->assertEmpty($inputFilter->getUnknown(), 'getUnknown() value not match');
    }

    /**
     * @dataProvider setDataArgumentsProvider
     */
    public function testSetTraversableDataAndGetRawValueGetValue(
        $inputs,
        $data,
        $expectedRawValues,
        $expectedValues,
        $expectedIsValid,
        $expectedInvalidInputs,
        $expectedValidInputs,
        $expectedMessages
    ) {
        $dataTypes = $this->dataTypes();
        $this->testSetDataAndGetRawValueGetValue(
            $inputs,
            $dataTypes['Traversable']($data),
            $expectedRawValues,
            $expectedValues,
            $expectedIsValid,
            $expectedInvalidInputs,
            $expectedValidInputs,
            $expectedMessages
        );
    }

    public function testResetEmptyValidationGroupRecursively()
    {
        $data = [
            'flat' => 'foo',
            'deep' => [
                'deep-input1' => 'deep-foo1',
                'deep-input2' => 'deep-foo2',
            ],
        ];
        $expectedData = array_merge($data, ['notSet' => null]);
        /** @var Input|MockObject $resetInput */
        $flatInput = $this->getMockBuilder(Input::class)
            ->enableProxyingToOriginalMethods()
            ->setConstructorArgs(['flat'])
            ->getMock()
        ;
        $flatInput->expects($this->once())
            ->method('setValue')
            ->with('foo')
        ;
        // Inputs without value must be reset for to have clean states when use different setData arguments
        /** @var Input|MockObject $flatInput */
        $resetInput = $this->getMockBuilder(Input::class)
            ->enableProxyingToOriginalMethods()
            ->setConstructorArgs(['notSet'])
            ->getMock()
        ;
        $resetInput->expects($this->once())
            ->method('resetValue')
        ;

        $filter = $this->inputFilter;
        $filter->add($flatInput);
        $filter->add($resetInput);
        $deepInputFilter = new BaseInputFilter;
        $deepInputFilter->add(new Input, 'deep-input1');
        $deepInputFilter->add(new Input, 'deep-input2');
        $filter->add($deepInputFilter, 'deep');
        $filter->setData($data);
        $filter->setValidationGroup(['deep' => 'deep-input1']);
        // reset validation group
        $filter->setValidationGroup(InputFilterInterface::VALIDATE_ALL);
        $this->assertEquals($expectedData, $filter->getValues());
    }

    /*
     * Idea for this one is that validation may need to rely on context -- e.g., a "password confirmation"
     * field may need to know what the original password entered was in order to compare.
     */

    public function contextProvider()
    {
        $data = ['fooInput' => 'fooValue'];
        $traversableData = new ArrayObject(['fooInput' => 'fooValue']);
        $expectedFromData = ['fooInput' => 'fooValue'];

        return [
            // Description => [$data, $customContext, $expectedContext]
            'by default get context from data (array)' => [$data, null, $expectedFromData],
            'by default get context from data (Traversable)' => [$traversableData, null, $expectedFromData],
            'use custom context' => [[], 'fooContext', 'fooContext'],
        ];
    }

    /**
     * @dataProvider contextProvider
     */
    public function testValidationContext($data, $customContext, $expectedContext)
    {
        $filter = $this->inputFilter;

        $input = $this->createInputInterfaceMock('fooInput', true, true, $expectedContext);
        $filter->add($input, 'fooInput');

        $filter->setData($data);

        $this->assertTrue(
            $filter->isValid($customContext),
            'isValid() value not match. Detail: ' . json_encode($filter->getMessages())
        );
    }

    public function testBuildValidationContextUsingInputGetRawValue()
    {
        $data = [];
        $expectedContext = ['fooInput' => 'fooRawValue'];
        $filter = $this->inputFilter;

        $input = $this->createInputInterfaceMock('fooInput', true, true, $expectedContext, 'fooRawValue');
        $filter->add($input, 'fooInput');

        $filter->setData($data);

        $this->assertTrue(
            $filter->isValid(),
            'isValid() value not match. Detail: ' . json_encode($filter->getMessages())
        );
    }

    public function testContextIsTheSameWhenARequiredInputIsGivenAndOptionalInputIsMissing()
    {
        $data = [
            'inputRequired' => 'inputRequiredValue',
        ];
        $expectedContext = [
            'inputRequired' => 'inputRequiredValue',
            'inputOptional' => null,
        ];
        $inputRequired = $this->createInputInterfaceMock('fooInput', true, true, $expectedContext);
        $inputOptional = $this->createInputInterfaceMock('fooInput', false);

        $filter = $this->inputFilter;
        $filter->add($inputRequired, 'inputRequired');
        $filter->add($inputOptional, 'inputOptional');

        $filter->setData($data);

        $this->assertTrue(
            $filter->isValid(),
            'isValid() value not match. Detail: ' . json_encode($filter->getMessages())
        );
    }

    public function testValidationSkipsFieldsMarkedNotRequiredWhenNoDataPresent()
    {
        $filter = $this->inputFilter;

        $optionalInputName = 'fooOptionalInput';
        /** @var InputInterface|MockObject $optionalInput */
        $optionalInput = $this->createMock(InputInterface::class);
        $optionalInput->method('getName')
            ->willReturn($optionalInputName)
        ;
        $optionalInput->expects($this->never())
            ->method('isValid')
        ;
        $data = [];

        $filter->add($optionalInput);

        $filter->setData($data);

        $this->assertTrue(
            $filter->isValid(),
            'isValid() value not match. Detail . ' . json_encode($filter->getMessages())
        );
        $this->assertArrayNotHasKey(
            $optionalInputName,
            $filter->getValidInput(),
            'Missing optional fields must not appear as valid input neither invalid input'
        );
        $this->assertArrayNotHasKey(
            $optionalInputName,
            $filter->getInvalidInput(),
            'Missing optional fields must not appear as valid input neither invalid input'
        );
    }

    /**
     * @dataProvider unknownScenariosProvider
     */
    public function testUnknown($inputs, $data, $hasUnknown, $getUnknown)
    {
        $inputFilter = $this->inputFilter;
        foreach ($inputs as $name => $input) {
            $inputFilter->add($input, $name);
        }

        $inputFilter->setData($data);

        $this->assertEquals($getUnknown, $inputFilter->getUnknown(), 'getUnknown() value not match');
        $this->assertEquals($hasUnknown, $inputFilter->hasUnknown(), 'hasUnknown() value not match');
    }

    public function testGetInputs()
    {
        $filter = $this->inputFilter;

        $foo = new Input('foo');
        $bar = new Input('bar');

        $filter->add($foo);
        $filter->add($bar);

        $filters = $filter->getInputs();

        $this->assertCount(2, $filters);
        $this->assertEquals('foo', $filters['foo']->getName());
        $this->assertEquals('bar', $filters['bar']->getName());
    }

    /**
     * @group 4996
     */
    public function testAddingExistingInputWillMergeIntoExisting()
    {
        $filter = $this->inputFilter;

        $foo1    = new Input('foo');
        $foo1->setRequired(true);
        $filter->add($foo1);

        $foo2    = new Input('foo');
        $foo2->setRequired(false);
        $filter->add($foo2);

        $this->assertFalse($filter->get('foo')->isRequired());
    }

    /**
     * @group 6431
     */
    public function testMerge()
    {
        $inputFilter       = $this->inputFilter;
        $originInputFilter = new BaseInputFilter();

        $inputFilter->add(new Input(), 'foo');
        $inputFilter->add(new Input(), 'bar');

        $originInputFilter->add(new Input(), 'baz');

        $inputFilter->merge($originInputFilter);

        $this->assertEquals(
            [
                'foo',
                'bar',
                'baz',
            ],
            array_keys($inputFilter->getInputs())
        );
    }

    public function testNestedInputFilterShouldAllowNonArrayValueForData()
    {
        $filter1 = new BaseInputFilter();
        $nestedFilter = new BaseInputFilter();
        $nestedFilter->add(new Input('nestedField1'));
        $filter1->add($nestedFilter, 'nested');

        // non scalar and non null value
        $filter1->setData(['nested' => false]);
        self::assertNull($filter1->getValues()['nested']['nestedField1']);

        $filter1->setData(['nested' => 123]);
        self::assertNull($filter1->getValues()['nested']['nestedField1']);

        $filter1->setData(['nested' => new stdClass()]);
        self::assertNull($filter1->getValues()['nested']['nestedField1']);
    }

    public function testInstanceOfUnfilteredDataInterface()
    {
        $baseInputFilter = new BaseInputFilter();

        self::assertInstanceOf(
            UnfilteredDataInterface::class,
            $baseInputFilter,
            sprintf('%s should implement %s', BaseInputFilter::class, UnfilteredDataInterface::class)
        );
    }

    public function testGetUnfilteredDataReturnsArray()
    {
        $baseInputFilter = new BaseInputFilter();

        self::assertInternalType('array', $baseInputFilter->getUnfilteredData());
    }

    public function testSetUnfilteredDataReturnsBaseInputFilter()
    {
        $baseInputFilter = new BaseInputFilter();

        self::assertInstanceOf(BaseInputFilter::class, $baseInputFilter->setUnfilteredData([]));
    }

    public function testSettingAndReturningDataArrayUnfilteredDataInterface()
    {
        $testArray = [
            'foo' => 'bar',
        ];

        $baseInputFilter = new BaseInputFilter();
        $baseInputFilter->setUnfilteredData($testArray);

        self::assertSame($testArray, $baseInputFilter->getUnfilteredData());
    }

    public function testSettingAndReturnDataArrayUsingSetDataForUnfilteredDataInterface()
    {
        $testArray = [
            'foo' => 'bar',
        ];

        $baseInputFilter = new BaseInputFilter();
        $baseInputFilter->setData($testArray);

        self::assertSame($testArray, $baseInputFilter->getUnfilteredData());
    }

    public function testSetDataUsingSetDataAndApplyFiltersReturningSameAsOriginalForUnfilteredData()
    {
        $filteredArray = [
            'bar' => 'foo',
        ];

        $unfilteredArray = array_merge(
            $filteredArray,
            [
                'foo' => 'bar',
            ]
        );

        /** @var BaseInputFilter $baseInputFilter */
        $baseInputFilter = (new BaseInputFilter())
            ->add(new Input(), 'bar')
            ->setData($unfilteredArray);

        self::assertSame($unfilteredArray, $baseInputFilter->getUnfilteredData());
        self::assertSame($filteredArray, $baseInputFilter->getValues());
        self::assertSame($filteredArray, $baseInputFilter->getRawValues());
    }

    public function addMethodArgumentsProvider()
    {
        $inputTypes = $this->inputProvider();

        $inputName = function ($inputTypeData) {
            return $inputTypeData[1];
        };

        $sameInput = function ($inputTypeData) {
            return $inputTypeData[2];
        };

        // @codingStandardsIgnoreStart
        $dataTemplates=[
            // Description => [[$input argument], $name argument, $expectedName, $expectedInput]
            'null' =>        [$inputTypes, null         , $inputName   , $sameInput],
            'custom_name' => [$inputTypes, 'custom_name', 'custom_name', $sameInput],
        ];
        // @codingStandardsIgnoreEnd

        // Expand data template matrix for each possible input type.
        // Description => [$input argument, $name argument, $expectedName, $expectedInput]
        $dataSets = [];
        foreach ($dataTemplates as $dataTemplateDescription => $dataTemplate) {
            foreach ($dataTemplate[0] as $inputTypeDescription => $inputTypeData) {
                $tmpTemplate = $dataTemplate;
                $tmpTemplate[0] = $inputTypeData[0]; // expand input
                if (is_callable($dataTemplate[2])) {
                    $tmpTemplate[2] = $dataTemplate[2]($inputTypeData);
                }
                $tmpTemplate[3] = $dataTemplate[3]($inputTypeData);

                $dataSets[$inputTypeDescription . ' / ' . $dataTemplateDescription] = $tmpTemplate;
            }
        }

        return $dataSets;
    }

    public function setDataArgumentsProvider()
    {
        $iAName = 'InputA';
        $iBName = 'InputB';
        $vRaw = 'rawValue';
        $vFiltered = 'filteredValue';

        $dARaw = [$iAName => $vRaw];
        $dBRaw = [$iBName => $vRaw];
        $dAfRaw = [$iAName => ['fooInput' => $vRaw]];
        $d2Raw = array_merge($dARaw, $dBRaw);
        $dAfBRaw = array_merge($dAfRaw, $dBRaw);
        $dAFiltered = [$iAName => $vFiltered];
        $dBFiltered = [$iBName => $vFiltered];
        $dAfFiltered = [$iAName => ['fooInput' => $vFiltered]];
        $d2Filtered = array_merge($dAFiltered, $dBFiltered);
        $dAfBFiltered = array_merge($dAfFiltered, $dBFiltered);

        $required = true;
        $valid = true;
        $bOnFail = true;

        $input = function ($iName, $required, $bOnFail, $isValid, $msg = []) use ($vRaw, $vFiltered) {
            // @codingStandardsIgnoreStart
            return function ($context) use ($iName, $required, $bOnFail, $isValid, $vRaw, $vFiltered, $msg) {
                return $this->createInputInterfaceMock($iName, $required, $isValid, $context, $vRaw, $vFiltered, $msg, $bOnFail);
            };
            // @codingStandardsIgnoreEnd
        };

        $inputFilter = function ($isValid, $msg = []) use ($vRaw, $vFiltered) {
            return function ($context) use ($isValid, $vRaw, $vFiltered, $msg) {
                $vRaw = ['fooInput' => $vRaw];
                $vFiltered = ['fooInput' => $vFiltered];
                return $this->createInputFilterInterfaceMock($isValid, $context, $vRaw, $vFiltered, $msg);
            };
        };

        // @codingStandardsIgnoreStart
        $iAri  = [$iAName => $input($iAName, $required, !$bOnFail, !$valid, ['Invalid ' . $iAName])];
        $iAriX = [$iAName => $input($iAName, $required,  $bOnFail, !$valid, ['Invalid ' . $iAName])];
        $iArvX = [$iAName => $input($iAName, $required,  $bOnFail,  $valid, [])];
        $iBri  = [$iBName => $input($iBName, $required, !$bOnFail, !$valid, ['Invalid ' . $iBName])];
        $iBriX = [$iBName => $input($iBName, $required,  $bOnFail, !$valid, ['Invalid ' . $iBName])];
        $iBrvX = [$iBName => $input($iBName, $required,  $bOnFail,  $valid, [])];
        $ifAi  = [$iAName => $inputFilter(!$valid, ['fooInput' => ['Invalid ' . $iAName]])];
        $ifAv  = [$iAName => $inputFilter($valid)];
        $iAriBri   = array_merge($iAri,  $iBri);
        $iArvXBrvX = array_merge($iArvX, $iBrvX);
        $iAriBrvX  = array_merge($iAri,  $iBrvX);
        $iArvXBir  = array_merge($iArvX, $iBri);
        $iAriXBrvX = array_merge($iAriX, $iBrvX);
        $iArvXBriX = array_merge($iArvX, $iBriX);
        $iAriXBriX = array_merge($iAriX, $iBriX);
        $ifAiBri   = array_merge($ifAi, $iBri);
        $ifAiBrvX  = array_merge($ifAi, $iBrvX);
        $ifAvBri   = array_merge($ifAv, $iBri);
        $ifAvBrv   = array_merge($ifAv, $iBrvX);

        $msgAInv = [$iAName => ['Invalid InputA']];
        $msgBInv = [$iBName => ['Invalid InputB']];
        $msgAfInv = [$iAName => ['fooInput' => ['Invalid InputA']]];
        $msg2Inv = array_merge($msgAInv, $msgBInv);
        $msgAfBInv = array_merge($msgAfInv, $msgBInv);

        $dataSets = [
            // Description => [$inputs, $data argument, $expectedRawValues, $expectedValues, $expectedIsValid,
            //                 $expectedInvalidInputs, $expectedValidInputs, $expectedMessages]
            'invalid Break invalid'     => [$iAriXBriX, $d2Raw, $d2Raw, $d2Filtered, false, $iAri    , []        , $msgAInv],
            'invalid Break valid'       => [$iAriXBrvX, $d2Raw, $d2Raw, $d2Filtered, false, $iAri    , []        , $msgAInv],
            'valid   Break invalid'     => [$iArvXBriX, $d2Raw, $d2Raw, $d2Filtered, false, $iBri    , $iAri     , $msgBInv],
            'valid   Break valid'       => [$iArvXBrvX, $d2Raw, $d2Raw, $d2Filtered, true , []       , $iArvXBrvX, []],
            'valid   invalid'           => [$iArvXBir , $d2Raw, $d2Raw, $d2Filtered, false, $iBri    , $iArvX    , $msgBInv],
            'IInvalid IValid'           => [$iAriBrvX , $d2Raw, $d2Raw, $d2Filtered, false, $iAri    , $iBrvX    , $msgAInv],
            'IInvalid IInvalid'         => [$iAriBri  , $d2Raw, $d2Raw, $d2Filtered, false, $iAriBri , []        , $msg2Inv],
            'IInvalid IValid / Partial' => [$iAriBri  , $dARaw, $d2Raw, $d2Filtered, false, $iAriBrvX, []        , $msg2Inv],
            'IFInvalid IValid'          => [$ifAiBrvX , $dAfBRaw, $dAfBRaw, $dAfBFiltered, false, $ifAi   , $iBrvX  , $msgAfInv],
            'IFInvalid IInvalid'        => [$ifAiBri  , $dAfBRaw, $dAfBRaw, $dAfBFiltered, false, $ifAiBri, []      , $msgAfBInv],
            'IFValid IInvalid'          => [$ifAvBri  , $dAfBRaw, $dAfBRaw, $dAfBFiltered, false, $iBri   , $ifAv   , $msgBInv],
            'IFValid IValid'            => [$ifAvBrv  , $dAfBRaw, $dAfBRaw, $dAfBFiltered, true , []      , $ifAvBrv, []],
        ];
        // @codingStandardsIgnoreEnd

        array_walk(
            $dataSets,
            function (&$set) {
                // Create unique mock input instances for each set
                foreach ($set[0] as $name => $createMock) {
                    $input = $createMock($set[2]);

                    $set[0][$name] = $input;
                    if (in_array($name, array_keys($set[5]))) {
                        $set[5][$name] = $input;
                    }
                    if (in_array($name, array_keys($set[6]))) {
                        $set[6][$name] = $input;
                    }
                }
            }
        );

        return $dataSets;
    }

    public function unknownScenariosProvider()
    {
        $inputA = $this->createInputInterfaceMock('inputA', true);
        $dataA = ['inputA' => 'foo'];
        $dataUnknown = ['inputUnknown' => 'unknownValue'];
        $dataAAndUnknown = array_merge($dataA, $dataUnknown);

        // @codingStandardsIgnoreStart
        return [
            // Description => [$inputs, $data, $hasUnknown, $getUnknown]
            'empty data and inputs'  => [[]       , []              , false, []],
            'empty data'             => [[$inputA], []              , false, []],
            'data and fields match'  => [[$inputA], $dataA          , false, []],
            'data known and unknown' => [[$inputA], $dataAAndUnknown, true , $dataUnknown],
            'data unknown'           => [[$inputA], $dataUnknown    , true , $dataUnknown],
            'data unknown, no input' => [[]       , $dataUnknown    , true , $dataUnknown],
        ];
        // @codingStandardsIgnoreEnd
    }

    public function inputProvider()
    {
        $input = $this->createInputInterfaceMock('fooInput', null);
        $inputFilter = $this->createInputFilterInterfaceMock();

        // @codingStandardsIgnoreStart
        return [
            // Description => [input, expected name, $expectedReturnInput]
            'InputInterface' =>       [$input      , 'fooInput', $input],
            'InputFilterInterface' => [$inputFilter, null      , $inputFilter],
        ];
        // @codingStandardsIgnoreEnd
    }

    /**
     * @param null|bool $isValid
     * @param mixed $context
     * @param mixed[] $getRawValues
     * @param mixed[] $getValues
     * @param string[] $getMessages
     *
     * @return MockObject|InputFilterInterface
     */
    protected function createInputFilterInterfaceMock(
        $isValid = null,
        $context = null,
        $getRawValues = [],
        $getValues = [],
        $getMessages = []
    ) {
        /** @var InputFilterInterface|MockObject $inputFilter */
        $inputFilter = $this->createMock(InputFilterInterface::class);
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

    /**
     * @param string $name
     * @param bool $isRequired
     * @param null|bool $isValid
     * @param mixed $context
     * @param mixed $getRawValue
     * @param mixed $getValue
     * @param string[] $getMessages
     * @param bool $breakOnFailure
     *
     * @return MockObject|InputInterface
     */
    protected function createInputInterfaceMock(
        $name,
        $isRequired,
        $isValid = null,
        $context = null,
        $getRawValue = null,
        $getValue = null,
        $getMessages = [],
        $breakOnFailure = false
    ) {
        /** @var InputInterface|MockObject $input */
        $input = $this->createMock(InputInterface::class);
        $input->method('getName')
            ->willReturn($name)
        ;
        $input->method('isRequired')
            ->willReturn($isRequired)
        ;
        $input->method('getRawValue')
            ->willReturn($getRawValue)
        ;
        $input->method('getValue')
            ->willReturn($getValue)
        ;
        $input->method('breakOnFailure')
            ->willReturn($breakOnFailure)
        ;
        if (($isValid === false) || ($isValid === true)) {
            $input->expects($this->any())
                ->method('isValid')
                ->with($context)
                ->willReturn($isValid)
            ;
        } else {
            $input->expects($this->never())
                ->method('isValid')
                ->with($context)
            ;
        }
        $input->method('getMessages')
            ->willReturn($getMessages)
        ;

        return $input;
    }

    /**
     * @return callable[]
     */
    protected function dataTypes()
    {
        return [
            // Description => callable
            'array' => function ($data) {
                return $data;
            },
            'Traversable' => function ($data) {
                return $this->getMockBuilder(FilterIterator::class)
                    ->setConstructorArgs([new ArrayIterator($data)])
                    ->getMock()
                ;
            },
        ];
    }
}
