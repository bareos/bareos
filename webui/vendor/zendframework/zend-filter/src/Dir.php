<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Filter;

class Dir extends AbstractFilter
{
    /**
     * Defined by Zend\Filter\FilterInterface
     *
     * Returns dirname($value)
     *
     * @param  string $value
     * @return string
     */
    public function filter($value)
    {
        if (! is_scalar($value)) {
            return $value;
        }
        $value = (string) $value;

        return dirname($value);
    }
}
