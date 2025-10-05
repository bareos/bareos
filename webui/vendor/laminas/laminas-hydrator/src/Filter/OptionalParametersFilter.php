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
use ReflectionParameter;

/**
 * Filter that includes methods which have no parameters or only optional parameters
 */
class OptionalParametersFilter implements FilterInterface
{
    /**
     * Map of methods already analyzed
     * by {@see OptionalParametersFilter::filter()},
     * cached for performance reasons
     *
     * @var bool[]
     */
    protected static $propertiesCache = [];

    /**
     * {@inheritDoc}
     */
    public function filter($property)
    {
        if (isset(static::$propertiesCache[$property])) {
            return static::$propertiesCache[$property];
        }

        try {
            $reflectionMethod = new ReflectionMethod($property);
        } catch (ReflectionException $exception) {
            throw new InvalidArgumentException(sprintf('Method %s doesn\'t exist', $property));
        }

        $mandatoryParameters = array_filter(
            $reflectionMethod->getParameters(),
            function (ReflectionParameter $parameter) {
                return ! $parameter->isOptional();
            }
        );

        return static::$propertiesCache[$property] = empty($mandatoryParameters);
    }
}
