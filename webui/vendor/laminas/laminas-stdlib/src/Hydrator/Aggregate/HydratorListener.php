<?php

/**
 * @see       https://github.com/laminas/laminas-stdlib for the canonical source repository
 * @copyright https://github.com/laminas/laminas-stdlib/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-stdlib/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Stdlib\Hydrator\Aggregate;

use Laminas\Hydrator\Aggregate\HydratorListener as BaseHydratorListener;

/**
 * Aggregate listener wrapping around a hydrator. Listens
 * to {@see \Laminas\Stdlib\Hydrator\Aggregate::EVENT_HYDRATE} and
 * {@see \Laminas\Stdlib\Hydrator\Aggregate::EVENT_EXTRACT}
 *
 * @deprecated Use Laminas\Hydrator\Aggregate\HydratorListener from laminas/laminas-hydrator instead.
 */
class HydratorListener extends BaseHydratorListener
{
}
