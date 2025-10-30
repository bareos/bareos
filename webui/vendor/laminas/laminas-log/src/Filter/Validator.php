<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Filter;

use Laminas\Log\Exception;
use Laminas\Validator\ValidatorInterface as LaminasValidator;
use Traversable;

class Validator implements FilterInterface
{
    /**
     * Regex to match
     *
     * @var LaminasValidator
     */
    protected $validator;

    /**
     * Filter out any log messages not matching the validator
     *
     * @param  LaminasValidator|array|Traversable $validator
     * @throws Exception\InvalidArgumentException
     * @return Validator
     */
    public function __construct($validator)
    {
        if ($validator instanceof Traversable) {
            $validator = iterator_to_array($validator);
        }
        if (is_array($validator)) {
            $validator = isset($validator['validator']) ? $validator['validator'] : null;
        }
        if (!$validator instanceof LaminasValidator) {
            throw new Exception\InvalidArgumentException(sprintf(
                'Parameter of type %s is invalid; must implement Laminas\Validator\ValidatorInterface',
                (is_object($validator) ? get_class($validator) : gettype($validator))
            ));
        }
        $this->validator = $validator;
    }

    /**
     * Returns TRUE to accept the message, FALSE to block it.
     *
     * @param array $event event data
     * @return bool
     */
    public function filter(array $event)
    {
        return $this->validator->isValid($event['message']);
    }
}
