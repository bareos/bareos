<?php
/**
 * @see       https://github.com/zendframework/zend-filter for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-filter/blob/master/LICENSE.md New BSD License
 */

namespace Zend\Filter;

class ToFloat extends AbstractFilter
{
    /**
     * Defined by Zend\Filter\FilterInterface
     *
     * Returns (float) $value
     *
     * If the value provided is non-scalar, the value will remain unfiltered
     *
     * @param  mixed $value
     * @return float|mixed
     */
    public function filter($value)
    {
        if (! is_scalar($value)) {
            return $value;
        }

        return (float) $value;
    }
}
