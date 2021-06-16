<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Cache\Pattern;

use Zend\Cache\Exception;

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
     * @return AbstractPattern Provides a fluent interface
     * @throws Exception\InvalidArgumentException
     */
    public function setOptions(PatternOptions $options)
    {
        if (! $options instanceof PatternOptions) {
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
