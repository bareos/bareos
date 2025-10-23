<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Storage;

use Iterator;

interface IteratorInterface extends Iterator
{
    const CURRENT_AS_SELF     = 0;
    const CURRENT_AS_KEY      = 1;
    const CURRENT_AS_VALUE    = 2;
    const CURRENT_AS_METADATA = 3;

    /**
     * Get storage instance
     *
     * @return StorageInterface
     */
    public function getStorage();

    /**
     * Get iterator mode
     *
     * @return int Value of IteratorInterface::CURRENT_AS_*
     */
    public function getMode();

    /**
     * Set iterator mode
     *
     * @param int $mode Value of IteratorInterface::CURRENT_AS_*
     * @return IteratorInterface Fluent interface
     */
    public function setMode($mode);
}
