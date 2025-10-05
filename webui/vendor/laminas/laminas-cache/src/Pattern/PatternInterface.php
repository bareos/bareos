<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Pattern;

interface PatternInterface
{
    /**
     * Set pattern options
     *
     * @param  PatternOptions $options
     * @return PatternInterface
     */
    public function setOptions(PatternOptions $options);

    /**
     * Get all pattern options
     *
     * @return PatternOptions
     */
    public function getOptions();
}
