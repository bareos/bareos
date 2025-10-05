<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\Filter;

use Laminas\Hydrator\Exception\InvalidArgumentException;
use ReflectionException;
use ReflectionMethod;

class NumberOfParameterFilter implements FilterInterface
{
    /**
     * The number of parameters beeing accepted
     * @var int
     */
    protected $numberOfParameters = null;

    /**
     * @param int $numberOfParameters Number of accepted parameters
     */
    public function __construct($numberOfParameters = 0)
    {
        $this->numberOfParameters = (int) $numberOfParameters;
    }

    /**
     * @param string $property the name of the property
     * @return bool
     * @throws InvalidArgumentException
     */
    public function filter($property)
    {
        try {
            $reflectionMethod = new ReflectionMethod($property);
        } catch (ReflectionException $exception) {
            throw new InvalidArgumentException(
                "Method $property doesn't exist"
            );
        }

        return $reflectionMethod->getNumberOfParameters() === $this->numberOfParameters;
    }
}
