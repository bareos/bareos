<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\Iterator;

use ArrayIterator;
use Laminas\Hydrator\HydratorInterface;

class HydratingArrayIterator extends HydratingIteratorIterator
{
    /**
     * @var HydratorInterface
     */
    protected $hydrator;

    /**
     * @var object
     */
    protected $prototype;

    /**
     * @param HydratorInterface $hydrator
     * @param array $data
     * @param string|object $prototype Object, or class name to use for prototype.
     */
    public function __construct(HydratorInterface $hydrator, array $data, $prototype)
    {
        parent::__construct($hydrator, new ArrayIterator($data), $prototype);
    }
}
