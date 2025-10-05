<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\Strategy;

class ClosureStrategy implements StrategyInterface
{
    /**
     * Function, used in extract method, default:
     *
     * <code>
     * function ($value) {
     *     return $value;
     * };
     * </code>
     *
     * @var callable
     */
    protected $extractFunc = null;

    /**
     * Function, used in hydrate method, default:
     *
     * <code>
     * function ($value) {
     *     return $value;
     * };
     * </code>
     *
     * @var callable
     */
    protected $hydrateFunc = null;

    /**
     * You can describe how your values will extract and hydrate, like this:
     *
     * <code>
     * $hydrator->addStrategy('category', new ClosureStrategy(
     *     function (Category $value) {
     *         return (int) $value->id;
     *     },
     *     function ($value) {
     *         return new Category((int) $value);
     *     }
     * ));
     * </code>
     *
     * @param callable $extractFunc - anonymous function, that extract values
     *     from object
     * @param callable $hydrateFunc - anonymous function, that hydrate values
     *     into object
     */
    public function __construct($extractFunc = null, $hydrateFunc = null)
    {
        if (isset($extractFunc)) {
            if (!is_callable($extractFunc)) {
                throw new \Exception('$extractFunc must be callable');
            }

            $this->extractFunc = $extractFunc;
        } else {
            $this->extractFunc = function ($value) {
                return $value;
            };
        }

        if (isset($hydrateFunc)) {
            if (!is_callable($hydrateFunc)) {
                throw new \Exception('$hydrateFunc must be callable');
            }

            $this->hydrateFunc = $hydrateFunc;
        } else {
            $this->hydrateFunc = function ($value) {
                return $value;
            };
        }
    }

    /**
     * Converts the given value so that it can be extracted by the hydrator.
     *
     * @param  mixed $value  The original value.
     * @param  array $object The object is optionally provided as context.
     * @return mixed Returns the value that should be extracted.
     */
    public function extract($value, $object = null)
    {
        $func = $this->extractFunc;

        return $func($value, $object);
    }

    /**
     * Converts the given value so that it can be hydrated by the hydrator.
     *
     * @param  mixed $value The original value.
     * @param  array $data  The whole data is optionally provided as context.
     * @return mixed Returns the value that should be hydrated.
     */
    public function hydrate($value, $data = null)
    {
        $func = $this->hydrateFunc;

        return $func($value, $data);
    }
}
