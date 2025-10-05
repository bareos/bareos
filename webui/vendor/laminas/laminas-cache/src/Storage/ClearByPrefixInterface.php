<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Storage;

interface ClearByPrefixInterface
{
    /**
     * Remove items matching given prefix
     *
     * @param string $prefix
     * @return bool
     */
    public function clearByPrefix($prefix);
}
