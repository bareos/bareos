<?php
/**
 * @see       https://github.com/zendframework/zend-inputfilter for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-inputfilter/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\InputFilter\FileInput;

use Zend\InputFilter\FileInput;
use Zend\InputFilter\FileInput\HttpServerFileInputDecorator;
use Zend\Validator;
use ZendTest\InputFilter\InputTest;

/**
 * @covers \Zend\InputFilter\FileInput\HttpServerFileInputDecorator
 * @covers \Zend\InputFilter\FileInput
 */
class HttpServerFileInputDecoratorTest extends InputTest
{
    /** @var HttpServerFileInputDecorator */
    protected $input;

    protected function setUp()
    {
        $this->input = new FileInput('foo');
        // Upload validator does not work in CLI test environment, disable
        $this->input->setAutoPrependUploadValidator(false);
    }

    public function testRetrievingValueFiltersTheValue()
    {
        $this->markTestSkipped('Test are not enabled in FileInputTest');
    }

    public function testRetrievingValueFiltersTheValueOnlyAfterValidating()
    {
        $value = ['tmp_name' => 'bar'];
        $this->input->setValue($value);

        $newValue = ['tmp_name' => 'foo'];
        $this->input->setFilterChain($this->createFilterChainMock([[$value, $newValue]]));

        $this->assertEquals($value, $this->input->getValue());
        $this->assertTrue(
            $this->input->isValid(),
            'isValid() value not match. Detail . ' . json_encode($this->input->getMessages())
        );
        $this->assertEquals($newValue, $this->input->getValue());
    }

    public function testCanFilterArrayOfMultiFileData()
    {
        $values = [
            ['tmp_name' => 'foo'],
            ['tmp_name' => 'bar'],
            ['tmp_name' => 'baz'],
        ];
        $this->input->setValue($values);

        $newValue = ['tmp_name' => 'new'];
        $filteredValue = [$newValue, $newValue, $newValue];
        $this->input->setFilterChain($this->createFilterChainMock([
            [$values[0], $newValue],
            [$values[1], $newValue],
            [$values[2], $newValue],
        ]));

        $this->assertEquals($values, $this->input->getValue());
        $this->assertTrue(
            $this->input->isValid(),
            'isValid() value not match. Detail . ' . json_encode($this->input->getMessages())
        );
        $this->assertEquals(
            $filteredValue,
            $this->input->getValue()
        );
    }

    public function testCanRetrieveRawValue()
    {
        $value = ['tmp_name' => 'bar'];
        $this->input->setValue($value);

        $newValue = ['tmp_name' => 'new'];
        $this->input->setFilterChain($this->createFilterChainMock([[$value, $newValue]]));

        $this->assertEquals($value, $this->input->getRawValue());
    }

    public function testValidationOperatesOnFilteredValue()
    {
        $this->markTestSkipped('Test is not enabled in FileInputTest');
    }

    public function testValidationOperatesBeforeFiltering()
    {
        $badValue = [
            'tmp_name' => ' ' . __FILE__ . ' ',
            'name'     => 'foo',
            'size'     => 1,
            'error'    => 0,
        ];
        $this->input->setValue($badValue);

        $filteredValue = ['tmp_name' => 'new'];
        $this->input->setFilterChain($this->createFilterChainMock([[$badValue, $filteredValue]]));
        $this->input->setValidatorChain($this->createValidatorChainMock([[$badValue, null, false]]));

        $this->assertFalse($this->input->isValid());
        $this->assertEquals($badValue, $this->input->getValue());
    }

    public function testAutoPrependUploadValidatorIsOnByDefault()
    {
        $input = new FileInput('foo');
        $this->assertTrue($input->getAutoPrependUploadValidator());
    }

    public function testUploadValidatorIsAddedWhenIsValidIsCalled()
    {
        $this->input->setAutoPrependUploadValidator(true);
        $this->assertTrue($this->input->getAutoPrependUploadValidator());
        $this->assertTrue($this->input->isRequired());
        $this->input->setValue([
            'tmp_name' => __FILE__,
            'name'     => 'foo',
            'size'     => 1,
            'error'    => 0,
        ]);
        $validatorChain = $this->input->getValidatorChain();
        $this->assertEquals(0, count($validatorChain->getValidators()));

        $this->assertFalse($this->input->isValid());
        $validators = $validatorChain->getValidators();
        $this->assertEquals(1, count($validators));
        $this->assertInstanceOf(Validator\File\UploadFile::class, $validators[0]['instance']);
    }

    public function testUploadValidatorIsNotAddedWhenIsValidIsCalled()
    {
        $this->assertFalse($this->input->getAutoPrependUploadValidator());
        $this->assertTrue($this->input->isRequired());
        $this->input->setValue(['tmp_name' => 'bar']);
        $validatorChain = $this->input->getValidatorChain();
        $this->assertEquals(0, count($validatorChain->getValidators()));

        $this->assertTrue(
            $this->input->isValid(),
            'isValid() value not match. Detail . ' . json_encode($this->input->getMessages())
        );
        $this->assertEquals(0, count($validatorChain->getValidators()));
    }

    public function testRequiredUploadValidatorValidatorNotAddedWhenOneExists()
    {
        $this->input->setAutoPrependUploadValidator(true);
        $this->assertTrue($this->input->getAutoPrependUploadValidator());
        $this->assertTrue($this->input->isRequired());
        $this->input->setValue(['tmp_name' => 'bar']);

        $uploadMock = $this->getMockBuilder(Validator\File\UploadFile::class)
            ->setMethods(['isValid'])
            ->getMock();
        $uploadMock->expects($this->exactly(1))
                     ->method('isValid')
                     ->will($this->returnValue(true));

        $validatorChain = $this->input->getValidatorChain();
        $validatorChain->prependValidator($uploadMock);
        $this->assertTrue(
            $this->input->isValid(),
            'isValid() value not match. Detail . ' . json_encode($this->input->getMessages())
        );

        $validators = $validatorChain->getValidators();
        $this->assertEquals(1, count($validators));
        $this->assertEquals($uploadMock, $validators[0]['instance']);
    }

    public function testValidationsRunWithoutFileArrayDueToAjaxPost()
    {
        $this->input->setAutoPrependUploadValidator(true);
        $this->assertTrue($this->input->getAutoPrependUploadValidator());
        $this->assertTrue($this->input->isRequired());
        $this->input->setValue('');

        $expectedNormalizedValue = [
            'tmp_name' => '',
            'name' => '',
            'size' => 0,
            'type' => '',
            'error' => UPLOAD_ERR_NO_FILE,
        ];
        $this->input->setValidatorChain($this->createValidatorChainMock([[$expectedNormalizedValue, null, false]]));
        $this->assertFalse($this->input->isValid());
    }

    public function testValidationsRunWithoutFileArrayIsSend()
    {
        $this->input->setAutoPrependUploadValidator(true);
        $this->assertTrue($this->input->getAutoPrependUploadValidator());
        $this->assertTrue($this->input->isRequired());
        $this->input->setValue([]);
        $expectedNormalizedValue = [
            'tmp_name' => '',
            'name'     => '',
            'size'     => 0,
            'type'     => '',
            'error'    => UPLOAD_ERR_NO_FILE,
        ];
        $this->input->setValidatorChain($this->createValidatorChainMock([[$expectedNormalizedValue, null, false]]));
        $this->assertFalse($this->input->isValid());
    }

    public function testNotEmptyValidatorAddedWhenIsValidIsCalled($value = null)
    {
        $this->markTestSkipped('Test is not enabled in FileInputTest');
    }

    public function testRequiredNotEmptyValidatorNotAddedWhenOneExists($value = null)
    {
        $this->markTestSkipped('Test is not enabled in FileInputTest');
    }

    public function testFallbackValueVsIsValidRules(
        $required = null,
        $fallbackValue = null,
        $originalValue = null,
        $isValid = null,
        $expectedValue = null
    ) {
        $this->markTestSkipped('Input::setFallbackValue is not implemented on FileInput');
    }


    public function testFallbackValueVsIsValidRulesWhenValueNotSet(
        $required = null,
        $fallbackValue = null
    ) {
        $this->markTestSkipped('Input::setFallbackValue is not implemented on FileInput');
    }

    public function testIsEmptyFileNotArray()
    {
        $rawValue = 'file';
        $this->assertTrue($this->input->isEmptyFile($rawValue));
    }

    public function testIsEmptyFileUploadNoFile()
    {
        $rawValue = [
            'tmp_name' => '',
            'error' => \UPLOAD_ERR_NO_FILE,
        ];
        $this->assertTrue($this->input->isEmptyFile($rawValue));
    }

    public function testIsEmptyFileOk()
    {
        $rawValue = [
            'tmp_name' => 'name',
            'error' => \UPLOAD_ERR_OK,
        ];
        $this->assertFalse($this->input->isEmptyFile($rawValue));
    }

    public function testIsEmptyMultiFileUploadNoFile()
    {
        $rawValue = [[
            'tmp_name' => 'foo',
            'error'    => \UPLOAD_ERR_NO_FILE,
        ]];
        $this->assertTrue($this->input->isEmptyFile($rawValue));
    }

    public function testIsEmptyFileMultiFileOk()
    {
        $rawValue = [
            [
                'tmp_name' => 'foo',
                'error'    => \UPLOAD_ERR_OK,
            ],
            [
                'tmp_name' => 'bar',
                'error'    => \UPLOAD_ERR_OK,
            ],
        ];
        $this->assertFalse($this->input->isEmptyFile($rawValue));
    }

    public function testDefaultInjectedUploadValidatorRespectsRelease2Convention()
    {
        $input = new FileInput('foo');
        $validatorChain = $input->getValidatorChain();
        $pluginManager = $validatorChain->getPluginManager();
        $pluginManager->setInvokableClass('fileuploadfile', TestAsset\FileUploadMock::class);
        $input->setValue('');

        $this->assertTrue($input->isValid());
    }

    /**
     * Specific FileInput::merge extras
     */
    public function testFileInputMerge()
    {
        $source = new FileInput();
        $source->setAutoPrependUploadValidator(true);

        $target = $this->input;
        $target->setAutoPrependUploadValidator(false);

        $return = $target->merge($source);
        $this->assertSame($target, $return, 'merge() must return it self');

        $this->assertEquals(
            true,
            $target->getAutoPrependUploadValidator(),
            'getAutoPrependUploadValidator() value not match'
        );
    }

    public function isRequiredVsAllowEmptyVsContinueIfEmptyVsIsValidProvider()
    {
        $dataSets = parent::isRequiredVsAllowEmptyVsContinueIfEmptyVsIsValidProvider();

        // FileInput do not use NotEmpty validator so the only validator present in the chain is the custom one.
        unset($dataSets['Required: T; AEmpty: F; CIEmpty: F; Validator: X, Value: Empty / tmp_name']);
        unset($dataSets['Required: T; AEmpty: F; CIEmpty: F; Validator: X, Value: Empty / single']);
        unset($dataSets['Required: T; AEmpty: F; CIEmpty: F; Validator: X, Value: Empty / multi']);

        return $dataSets;
    }

    public function emptyValueProvider()
    {
        return [
            'tmp_name' => [
                'raw' => 'file',
                'filtered' => [
                    'tmp_name' => 'file',
                    'name' => 'file',
                    'size' => 0,
                    'type' => '',
                    'error' => UPLOAD_ERR_NO_FILE,
                ],
            ],
            'single' => [
                'raw' => [
                    'tmp_name' => '',
                    'error' => UPLOAD_ERR_NO_FILE,
                ],
                'filtered' => [
                    'tmp_name' => '',
                    'error' => UPLOAD_ERR_NO_FILE,
                ],
            ],
            'multi' => [
                'raw' => [
                    [
                        'tmp_name' => 'foo',
                        'error' => UPLOAD_ERR_NO_FILE,
                    ],
                ],
                'filtered' => [
                    'tmp_name' => 'foo',
                    'error' => UPLOAD_ERR_NO_FILE,
                ],
            ],
        ];
    }

    public function mixedValueProvider()
    {
        $fooUploadErrOk = [
            'tmp_name' => 'foo',
            'error' => UPLOAD_ERR_OK,
        ];

        return [
            'single' => [
                'raw' => $fooUploadErrOk,
                'filtered' => $fooUploadErrOk,
            ],
            'multi' => [
                'raw' => [
                    $fooUploadErrOk,
                ],
                'filtered' => $fooUploadErrOk,
            ],
        ];
    }

    protected function getDummyValue($raw = true)
    {
        return ['tmp_name' => 'bar'];
    }
}
