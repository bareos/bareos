<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\Strategy;

use Laminas\Stdlib\ArrayUtils;
use Traversable;

class StrategyChain implements StrategyInterface
{
    /**
     * Strategy chain for extraction
     *
     * @var StrategyInterface[]
     */
    private $extractionStrategies;

    /**
     * Strategy chain for hydration
     *
     * @var StrategyInterface[]
     */
    private $hydrationStrategies;

    /**
     * Constructor
     *
     * @param array|Traversable $extractionStrategies
     */
    public function __construct($extractionStrategies)
    {
        $extractionStrategies = ArrayUtils::iteratorToArray($extractionStrategies);
        $this->extractionStrategies = array_map(
            function (StrategyInterface $strategy) {
                // this callback is here only to ensure type-safety
                return $strategy;
            },
            $extractionStrategies
        );

        $this->hydrationStrategies = array_reverse($extractionStrategies);
    }

    /**
     * {@inheritDoc}
     */
    public function extract($value)
    {
        foreach ($this->extractionStrategies as $strategy) {
            $value = $strategy->extract($value);
        }

        return $value;
    }

    /**
     * {@inheritDoc}
     */
    public function hydrate($value)
    {
        foreach ($this->hydrationStrategies as $strategy) {
            $value = $strategy->hydrate($value);
        }

        return $value;
    }
}
