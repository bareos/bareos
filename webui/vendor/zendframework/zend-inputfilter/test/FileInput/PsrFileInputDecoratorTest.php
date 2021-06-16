<?php
/**
 * @see       https://github.com/zendframework/zend-inputfilter for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-inputfilter/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\InputFilter\FileInput;

use Prophecy\Argument;
use Psr\Http\Message\UploadedFileInterface;
use Zend\InputFilter\FileInput;
use Zend\InputFilter\FileInput\PsrFileInputDecorator;
use Zend\Validator;
use ZendTest\InputFilter\InputTest;

/**
 * @covers \Zend\InputFilter\FileInput\PsrFileInputDecorator
 * @covers \Zend\InputFilter\FileInput
 */
class PsrFileInputDecoratorTest extends InputTest
{
    /** @var PsrFileInputDecorator */
    protected $input;

    protected function setUp()
    {
        $this->input = new FileInput('foo');
        // Upload validator does not work in CLI test environment, disable
        $this->input->setAutoPrependUploadValidator(false);
    }

    public function testRetrievingValueFiltersTheValue()
    {
        $this->markTestSkipped('Test is not enabled in PsrFileInputTest');
    }

    public function testRetrievingValueFiltersTheValueOnlyAfterValidating()
    {
        $upload = $this->prophesize(UploadedFileInterface::class);
        $upload->getError()->willReturn(UPLOAD_ERR_OK);

        $this->input->setValue($upload->reveal());

        $filteredUpload = $this->prophesize(UploadedFileInterface::class);

        $this->input->setFilterChain($this->createFilterChainMock([[
            $upload->reveal(),
            $filteredUpload->reveal(),
        ]]));

        $this->assertEquals($upload->reveal(), $this->input->getValue());
        $this->assertTrue(
            $this->input->isValid(),
            'isValid() value not match. Detail . ' . json_encode($this->input->getMessages())
        );
        $this->assertEquals($filteredUpload->reveal(), $this->input->getValue());
    }

    public function testCanFilterArrayOfMultiFileData()
    {
        $values = [];
        for ($i = 0; $i < 3; $i += 1) {
            $upload = $this->prophesize(UploadedFileInterface::class);
            $upload->getError()->willReturn(UPLOAD_ERR_OK);
            $values[] = $upload->reveal();
        }

        $this->input->setValue($values);

        $filteredValues = [];
        for ($i = 0; $i < 3; $i += 1) {
            $upload = $this->prophesize(UploadedFileInterface::class);
            $filteredValues[] = $upload->reveal();
        }

        $this->input->setFilterChain($this->createFilterChainMock([
            [$values[0], $filteredValues[0]],
            [$values[1], $filteredValues[1]],
            [$values[2], $filteredValues[2]],
        ]));

        $this->assertEquals($values, $this->input->getValue());
        $this->assertTrue(
            $this->input->isValid(),
            'isValid() value not match. Detail . ' . json_encode($this->input->getMessages())
        );
        $this->assertEquals(
            $filteredValues,
            $this->input->getValue()
        );
    }

    public function testCanRetrieveRawValue()
    {
        $value = $this->prophesize(UploadedFileInterface::class);
        $value->getError()->shouldNotBeCalled();

        $this->input->setValue($value->reveal());

        $filteredValue = $this->prophesize(UploadedFileInterface::class)->reveal();
        $this->input->setFilterChain($this->createFilterChainMock([[$value->reveal(), $filteredValue]]));

        $this->assertEquals($value->reveal(), $this->input->getRawValue());
    }

    public function testValidationOperatesOnFilteredValue()
    {
        $this->markTestSkipped('Test is not enabled in PsrFileInputTest');
    }

    public function testValidationOperatesBeforeFiltering()
    {
        $badValue = $this->prophesize(UploadedFileInterface::class);
        $badValue->getError()->willReturn(UPLOAD_ERR_NO_FILE);
        $filteredValue = $this->prophesize(UploadedFileInterface::class)->reveal();

        $this->input->setValue($badValue->reveal());

        $this->input->setFilterChain($this->createFilterChainMock([[$badValue->reveal(), $filteredValue]]));
        $this->input->setValidatorChain($this->createValidatorChainMock([[$badValue->reveal(), null, false]]));

        $this->assertFalse($this->input->isValid());
        $this->assertEquals($badValue->reveal(), $this->input->getValue());
    }

    public function testAutoPrependUploadValidatorIsOnByDefault()
    {
        $input = new FileInput('foo');
        $this->assertTrue($input->getAutoPrependUploadValidator());
    }

    public function testUploadValidatorIsAddedDuringIsValidWhenAutoPrependUploadValidatorIsEnabled()
    {
        $this->input->setAutoPrependUploadValidator(true);
        $this->assertTrue($this->input->getAutoPrependUploadValidator());
        $this->assertTrue($this->input->isRequired());

        $uploadedFile = $this->prophesize(UploadedFileInterface::class);
        $uploadedFile->getError()->willReturn(UPLOAD_ERR_CANT_WRITE);

        $this->input->setValue($uploadedFile->reveal());

        $validatorChain = $this->input->getValidatorChain();
        $this->assertCount(0, $validatorChain->getValidators());

        $this->assertFalse($this->input->isValid());
        $validators = $validatorChain->getValidators();
        $this->assertCount(1, $validators);
        $this->assertInstanceOf(Validator\File\UploadFile::class, $validators[0]['instance']);
    }

    public function testUploadValidatorIsNotAddedByDefaultDuringIsValidWhenAutoPrependUploadValidatorIsDisabled()
    {
        $this->assertFalse($this->input->getAutoPrependUploadValidator());
        $this->assertTrue($this->input->isRequired());

        $uploadedFile = $this->prophesize(UploadedFileInterface::class);
        $uploadedFile->getError()->willReturn(UPLOAD_ERR_OK);

        $this->input->setValue($uploadedFile->reveal());
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

        $upload = $this->prophesize(UploadedFileInterface::class);
        $upload->getError()->willReturn(UPLOAD_ERR_OK);

        $this->input->setValue($upload->reveal());

        $validator = $this->prophesize(Validator\File\UploadFile::class);
        $validator
            ->isValid(Argument::that([$upload, 'reveal']), null)
            ->willReturn(true)
            ->shouldBeCalledTimes(1);

        $validatorChain = $this->input->getValidatorChain();
        $validatorChain->prependValidator($validator->reveal());
        $this->assertTrue(
            $this->input->isValid(),
            'isValid() value not match. Detail . ' . json_encode($this->input->getMessages())
        );

        $validators = $validatorChain->getValidators();
        $this->assertEquals(1, count($validators));
        $this->assertEquals($validator->reveal(), $validators[0]['instance']);
    }

    public function testNotEmptyValidatorAddedWhenIsValidIsCalled($value = null)
    {
        $this->markTestSkipped('Test is not enabled in PsrFileInputTest');
    }

    public function testRequiredNotEmptyValidatorNotAddedWhenOneExists($value = null)
    {
        $this->markTestSkipped('Test is not enabled in PsrFileInputTest');
    }

    public function testFallbackValueVsIsValidRules(
        $required = null,
        $fallbackValue = null,
        $originalValue = null,
        $isValid = null,
        $expectedValue = null
    ) {
        $this->markTestSkipped('Input::setFallbackValue is not implemented on PsrFileInput');
    }


    public function testFallbackValueVsIsValidRulesWhenValueNotSet(
        $required = null,
        $fallbackValue = null
    ) {
        $this->markTestSkipped('Input::setFallbackValue is not implemented on PsrFileInput');
    }

    public function testIsEmptyFileUploadNoFile()
    {
        $upload = $this->prophesize(UploadedFileInterface::class);
        $upload->getError()->willReturn(UPLOAD_ERR_NO_FILE);
        $this->assertTrue($this->input->isEmptyFile($upload->reveal()));
    }

    public function testIsEmptyFileOk()
    {
        $upload = $this->prophesize(UploadedFileInterface::class);
        $upload->getError()->willReturn(UPLOAD_ERR_OK);
        $this->assertFalse($this->input->isEmptyFile($upload->reveal()));
    }

    public function testIsEmptyMultiFileUploadNoFile()
    {
        $upload = $this->prophesize(UploadedFileInterface::class);
        $upload->getError()->willReturn(UPLOAD_ERR_NO_FILE);

        $rawValue = [$upload->reveal()];

        $this->assertTrue($this->input->isEmptyFile($rawValue));
    }

    public function testIsEmptyFileMultiFileOk()
    {
        $rawValue = [];
        for ($i = 0; $i < 2; $i += 1) {
            $upload = $this->prophesize(UploadedFileInterface::class);
            $upload->getError()->willReturn(UPLOAD_ERR_OK);
            $rawValue[] = $upload->reveal();
        }

        $this->assertFalse($this->input->isEmptyFile($rawValue));
    }

    /**
     * Specific PsrFileInput::merge extras
     */
    public function testPsrFileInputMerge()
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
        $generator = parent::isRequiredVsAllowEmptyVsContinueIfEmptyVsIsValidProvider();
        if (! is_array($generator)) {
            $generator = clone $generator;
            $generator->rewind();
        }

        $toSkip = [
            'Required: T; AEmpty: F; CIEmpty: F; Validator: X, Value: Empty / tmp_name',
            'Required: T; AEmpty: F; CIEmpty: F; Validator: X, Value: Empty / single',
            'Required: T; AEmpty: F; CIEmpty: F; Validator: X, Value: Empty / multi',
        ];

        foreach ($generator as $name => $data) {
            if (in_array($name, $toSkip, true)) {
                continue;
            }
            yield $name => $data;
        }
    }

    public function emptyValueProvider()
    {
        foreach (['single', 'multi'] as $type) {
            $raw = $this->prophesize(UploadedFileInterface::class);
            $raw->getError()->willReturn(UPLOAD_ERR_NO_FILE);

            $filtered = $this->prophesize(UploadedFileInterface::class);
            $filtered->getError()->willReturn(UPLOAD_ERR_NO_FILE);

            yield $type => [
                'raw'      => $type === 'multi'
                    ? [$raw->reveal()]
                    : $raw->reveal(),
                'filtered' => $raw->reveal(),
            ];
        }
    }

    public function mixedValueProvider()
    {
        $fooUploadErrOk = $this->prophesize(UploadedFileInterface::class);
        $fooUploadErrOk->getError()->willReturn(UPLOAD_ERR_OK);

        return [
            'single' => [
                'raw' => $fooUploadErrOk->reveal(),
                'filtered' => $fooUploadErrOk->reveal(),
            ],
            'multi' => [
                'raw' => [
                    $fooUploadErrOk->reveal(),
                ],
                'filtered' => $fooUploadErrOk->reveal(),
            ],
        ];
    }

    protected function getDummyValue($raw = true)
    {
        $upload = $this->prophesize(UploadedFileInterface::class);
        $upload->getError()->willReturn(UPLOAD_ERR_OK);
        return $upload->reveal();
    }
}
