<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\Filter;

class MethodMatchFilter implements FilterInterface
{
    /**
     * The method to exclude
     * @var string
     */
    protected $method = null;

    /**
     * Either an exclude or an include
     * @var bool
     */
    protected $exclude = null;

    /**
     * @param string $method The method to exclude or include
     * @param bool $exclude If the method should be excluded
     */
    public function __construct($method, $exclude = true)
    {
        $this->method = $method;
        $this->exclude = $exclude;
    }

    public function filter($property)
    {
        $pos = strpos($property, '::');
        if ($pos !== false) {
            $pos += 2;
        } else {
            $pos = 0;
        }
        if (substr($property, $pos) === $this->method) {
            return !$this->exclude;
        }
        return $this->exclude;
    }
}
