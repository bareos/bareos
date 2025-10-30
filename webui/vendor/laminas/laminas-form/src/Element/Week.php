<?php

/**
 * @see       https://github.com/laminas/laminas-form for the canonical source repository
 * @copyright https://github.com/laminas/laminas-form/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-form/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Form\Element;

use Laminas\Validator\DateStep as DateStepValidator;
use Laminas\Validator\Regex as RegexValidator;

class Week extends DateTime
{
    /**
     * Seed attributes
     *
     * @var array
     */
    protected $attributes = [
        'type' => 'week',
    ];

    /**
     * Retrieves a Date Validator configured for a Week Input type
     *
     * @return \Laminas\Validator\ValidatorInterface
     */
    protected function getDateValidator()
    {
        return new RegexValidator('/^[0-9]{4}\-W[0-9]{2}$/');
    }

    /**
     * Retrieves a DateStep Validator configured for a Week Input type
     *
     * @return \Laminas\Validator\ValidatorInterface
     */
    protected function getStepValidator()
    {
        $stepValue = (isset($this->attributes['step']))
                     ? $this->attributes['step'] : 1; // Weeks

        $baseValue = (isset($this->attributes['min']))
                     ? $this->attributes['min'] : '1970-W01';

        return new DateStepValidator([
            'format'    => 'Y-\WW',
            'baseValue' => $baseValue,
            'step'      => new \DateInterval("P{$stepValue}W"),
        ]);
    }
}
