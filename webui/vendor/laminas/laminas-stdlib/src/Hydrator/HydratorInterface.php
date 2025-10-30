<?php

/**
 * @see       https://github.com/laminas/laminas-stdlib for the canonical source repository
 * @copyright https://github.com/laminas/laminas-stdlib/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-stdlib/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Stdlib\Hydrator;

use Laminas\Hydrator\HydratorInterface as BaseHydratorInterface;
use Laminas\Stdlib\Extractor\ExtractionInterface;

/**
 * @deprecated Use Laminas\Hydrator\HydratorInterface from laminas/laminas-hydrator instead.
 */
interface HydratorInterface extends BaseHydratorInterface, HydrationInterface, ExtractionInterface
{
}
