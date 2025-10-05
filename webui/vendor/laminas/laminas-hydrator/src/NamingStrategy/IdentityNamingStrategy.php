<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\NamingStrategy;

class IdentityNamingStrategy implements NamingStrategyInterface
{
    /**
     * {@inheritDoc}
     */
    public function hydrate($name)
    {
        return $name;
    }

    /**
     * {@inheritDoc}
     */
    public function extract($name)
    {
        return $name;
    }
}
