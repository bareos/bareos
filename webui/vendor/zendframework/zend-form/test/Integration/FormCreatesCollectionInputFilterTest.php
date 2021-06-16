<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\Integration;

use PHPUnit\Framework\TestCase;
use Zend\Form\Form;
use Zend\Form\InputFilterProviderFieldset;
use Zend\Validator;

class FormCreatesCollectionInputFilterTest extends TestCase
{
    public static function assertValidatorFound($class, array $validators, $message = null)
    {
        $message = $message ?: sprintf('Failed to find validator of type %s in validator list', $class);
        foreach ($validators as $instance) {
            $validator = $instance['instance'];
            if ($validator instanceof $class) {
                return true;
            }
        }
        var_export($validators);
        self::fail($message);
    }

    /**
     * @see https://github.com/zendframework/zend-form/issues/78
     */
    public function testCollectionInputFilterContainsExpectedValidators()
    {
        $form = new Form;
        $form->add([
            'name' => 'collection',
            'type' => 'collection',
            'options' => [
                'target_element' => [
                    'type' => InputFilterProviderFieldset::class,
                    'elements' => [
                        [
                            'spec' => [
                                'name' => 'date',
                                'type' => 'date'
                            ],
                        ],
                    ],
                    'options' => ['input_filter_spec' => [
                        'date' => [
                            'required' => false,
                            'validators' => [
                                ['name' => 'StringLength'],
                            ],
                        ],
                    ]],
                ],
            ],
        ]);
        $inputFilter = $form->getInputFilter();
        $filter = $inputFilter->get('collection')->getInputFilter()->get('date');

        $validators = $filter->getValidatorChain()->getValidators();
        $this->assertCount(3, $validators);
        $this->assertValidatorFound(Validator\StringLength::class, $validators);
        $this->assertValidatorFound(Validator\Date::class, $validators);
        $this->assertValidatorFound(Validator\DateStep::class, $validators);

        return $form;
    }

    /**
     * @depends testCollectionInputFilterContainsExpectedValidators
     */
    public function testCollectionElementDoesNotCreateDiscreteElementInInputFilter(Form $form)
    {
        $inputFilter = $form->getInputFilter();
        $this->assertFalse($inputFilter->has('date'));
    }
}
