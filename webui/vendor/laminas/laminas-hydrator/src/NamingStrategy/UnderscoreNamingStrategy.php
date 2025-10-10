<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\NamingStrategy;

use Laminas\Filter\FilterChain;

class UnderscoreNamingStrategy implements NamingStrategyInterface
{
    /**
     * @var FilterChain|null
     */
    protected static $camelCaseToUnderscoreFilter;

    /**
     * @var FilterChain|null
     */
    protected static $underscoreToStudlyCaseFilter;

    /**
     * Remove underscores and capitalize letters
     *
     * @param  string $name
     * @return string
     */
    public function hydrate($name)
    {
        return $this->getUnderscoreToStudlyCaseFilter()->filter($name);
    }

    /**
     * Remove capitalized letters and prepend underscores.
     *
     * @param  string $name
     * @return string
     */
    public function extract($name)
    {
        return $this->getCamelCaseToUnderscoreFilter()->filter($name);
    }

    /**
     * @return FilterChain
     */
    protected function getUnderscoreToStudlyCaseFilter()
    {
        if (static::$underscoreToStudlyCaseFilter instanceof FilterChain) {
            return static::$underscoreToStudlyCaseFilter;
        }

        $filter = new FilterChain();

        $filter->attachByName('WordUnderscoreToStudlyCase');

        return static::$underscoreToStudlyCaseFilter = $filter;
    }

    /**
     * @return FilterChain
     */
    protected function getCamelCaseToUnderscoreFilter()
    {
        if (static::$camelCaseToUnderscoreFilter instanceof FilterChain) {
            return static::$camelCaseToUnderscoreFilter;
        }

        $filter = new FilterChain();

        $filter->attachByName('WordCamelCaseToUnderscore');
        $filter->attachByName('StringToLower');

        return static::$camelCaseToUnderscoreFilter = $filter;
    }
}
