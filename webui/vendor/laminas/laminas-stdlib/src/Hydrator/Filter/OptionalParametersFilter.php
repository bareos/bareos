<?php

/**
 * @see       https://github.com/laminas/laminas-stdlib for the canonical source repository
 * @copyright https://github.com/laminas/laminas-stdlib/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-stdlib/blob/master/LICENSE.md New BSD License
 */
namespace Laminas\Stdlib\Hydrator\Filter;

use Laminas\Hydrator\Filter\OptionalParametersFilter as BaseOptionalParametersFilter;

/**
 * Filter that includes methods which have no parameters or only optional parameters
 *
 * @deprecated Use Laminas\Hydrator\Filter\OptionalParametersFilter from laminas/laminas-hydrator instead.
 */
class OptionalParametersFilter extends BaseOptionalParametersFilter implements FilterInterface
{
}
