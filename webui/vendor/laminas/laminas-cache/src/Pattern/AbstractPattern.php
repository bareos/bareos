<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Pattern;

use Laminas\Cache\Exception;

abstract class AbstractPattern implements PatternInterface
{
    /**
     * @var PatternOptions
     */
    protected $options;

    /**
     * Set pattern options
     *
     * @param  PatternOptions $options
     * @return AbstractPattern
     * @throws Exception\InvalidArgumentException
     */
    public function setOptions(PatternOptions $options)
    {
        if (!$options instanceof PatternOptions) {
            $options = new PatternOptions($options);
        }

        $this->options = $options;
        return $this;
    }

    /**
     * Get all pattern options
     *
     * @return PatternOptions
     */
    public function getOptions()
    {
        if (null === $this->options) {
            $this->setOptions(new PatternOptions());
        }
        return $this->options;
    }
}
