<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\NamingStrategy;

class ArrayMapNamingStrategy implements NamingStrategyInterface
{
    /**
     * @var string[]
     */
    private $extractionMap = [];

    /**
     * @var string[]
     */
    private $hydrationMap = [];

    /**
     * Constructor
     *
     * @param array $extractionMap A map of string keys and values for symmetric translation of hydrated
     *                             and extracted field names
     */
    public function __construct(array $extractionMap)
    {
        $this->extractionMap = $extractionMap;
        $this->hydrationMap  = array_flip($extractionMap);
    }

    /**
     * {@inheritDoc}
     */
    public function hydrate($name)
    {
        return isset($this->hydrationMap[$name]) ? $this->hydrationMap[$name] : $name;
    }

    /**
     * {@inheritDoc}
     */
    public function extract($name)
    {
        return isset($this->extractionMap[$name]) ? $this->extractionMap[$name] : $name;
    }
}
