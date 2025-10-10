<?php

/**
 * @see       https://github.com/laminas/laminas-form for the canonical source repository
 * @copyright https://github.com/laminas/laminas-form/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-form/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Form\Element;

use DateInterval;
use DateTime as PhpDateTime;
use Laminas\Form\Element;
use Laminas\InputFilter\InputProviderInterface;
use Laminas\Validator\Date as DateValidator;
use Laminas\Validator\DateStep as DateStepValidator;
use Laminas\Validator\GreaterThan as GreaterThanValidator;
use Laminas\Validator\LessThan as LessThanValidator;

class DateTime extends Element implements InputProviderInterface
{
    const DATETIME_FORMAT = 'Y-m-d\TH:iP';

    /**
     * Seed attributes
     *
     * @var array
     */
    protected $attributes = [
        'type' => 'datetime',
    ];

    /**
     * A valid format string accepted by date()
     *
     * @var string
     */
    protected $format = self::DATETIME_FORMAT;

    /**
     * @var array
     */
    protected $validators;

    /**
     * Accepted options for DateTime:
     * - format: A \DateTime compatible string
     *
     * @param array|\Traversable $options
     * @return DateTime
     */
    public function setOptions($options)
    {
        parent::setOptions($options);

        if (isset($this->options['format'])) {
            $this->setFormat($this->options['format']);
        }

        return $this;
    }

    /**
     * Retrieve the element value
     *
     * If the value is a DateTime object, and $returnFormattedValue is true
     * (the default), we return the string
     * representation using the currently registered format.
     *
     * If $returnFormattedValue is false, the original value will be
     * returned, regardless of type.
     *
     * @param  bool $returnFormattedValue
     * @return mixed
     */
    public function getValue($returnFormattedValue = true)
    {
        $value = parent::getValue();
        if (!$value instanceof PhpDateTime || !$returnFormattedValue) {
            return $value;
        }
        $format = $this->getFormat();
        return $value->format($format);
    }

    /**
     * Set value for format
     *
     * @param  string $format
     * @return DateTime
     */
    public function setFormat($format)
    {
        $this->format = (string) $format;
        return $this;
    }

    /**
     * Retrieve the DateTime format to use for the value
     *
     * @return string
     */
    public function getFormat()
    {
        return $this->format;
    }

    /**
     * Get validators
     *
     * @return array
     */
    protected function getValidators()
    {
        if ($this->validators) {
            return $this->validators;
        }

        $validators = [];
        $validators[] = $this->getDateValidator();

        if (isset($this->attributes['min'])) {
            $validators[] = new GreaterThanValidator([
                'min'       => $this->attributes['min'],
                'inclusive' => true,
            ]);
        }
        if (isset($this->attributes['max'])) {
            $validators[] = new LessThanValidator([
                'max'       => $this->attributes['max'],
                'inclusive' => true,
            ]);
        }
        if (!isset($this->attributes['step'])
            || 'any' !== $this->attributes['step']
        ) {
            $validators[] = $this->getStepValidator();
        }

        $this->validators = $validators;
        return $this->validators;
    }

    /**
     * Retrieves a Date Validator configured for a DateTime Input type
     *
     * @return DateTime
     */
    protected function getDateValidator()
    {
        return new DateValidator(['format' => $this->format]);
    }

    /**
     * Retrieves a DateStep Validator configured for a DateTime Input type
     *
     * @return DateTime
     */
    protected function getStepValidator()
    {
        $format    = $this->getFormat();
        $stepValue = (isset($this->attributes['step']))
                   ? $this->attributes['step'] : 1; // Minutes

        $baseValue = (isset($this->attributes['min']))
                   ? $this->attributes['min'] : date($format, 0);

        return new DateStepValidator([
            'format'    => $format,
            'baseValue' => $baseValue,
            'step'      => new DateInterval("PT{$stepValue}M"),
        ]);
    }

    /**
     * Provide default input rules for this element
     *
     * Attaches default validators for the datetime input.
     *
     * @return array
     */
    public function getInputSpecification()
    {
        return [
            'name' => $this->getName(),
            'required' => true,
            'filters' => [
                ['name' => 'Laminas\Filter\StringTrim'],
            ],
            'validators' => $this->getValidators(),
        ];
    }
}
