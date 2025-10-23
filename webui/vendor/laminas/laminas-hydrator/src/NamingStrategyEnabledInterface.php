<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator;

interface NamingStrategyEnabledInterface
{
    /**
     * Adds the given naming strategy
     *
     * @param NamingStrategy\NamingStrategyInterface $strategy The naming to register.
     * @return self
     */
    public function setNamingStrategy(NamingStrategy\NamingStrategyInterface $strategy);

    /**
     * Gets the naming strategy.
     *
     * @return NamingStrategy\NamingStrategyInterface
     */
    public function getNamingStrategy();

    /**
     * Checks if a naming strategy exists.
     *
     * @return bool
     */
    public function hasNamingStrategy();

    /**
     * Removes the naming with the given name.
     *
     * @return self
     */
    public function removeNamingStrategy();
}
