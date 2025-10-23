<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator;

use Traversable;

interface HydratorOptionsInterface
{
    /**
     * @param  array|Traversable $options
     * @return self
     */
    public function setOptions($options);
}
