<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator;

use Interop\Container\ContainerInterface;

class DelegatingHydrator implements HydratorInterface
{
    /**
     * @var ContainerInterface
     */
    protected $hydrators;

    /**
     * Constructor
     *
     * @param ContainerInterface $hydrators
     */
    public function __construct(ContainerInterface $hydrators)
    {
        $this->hydrators = $hydrators;
    }

    /**
     * {@inheritdoc}
     */
    public function hydrate(array $data, $object)
    {
        return $this->getHydrator($object)->hydrate($data, $object);
    }

    /**
     * {@inheritdoc}
     */
    public function extract($object)
    {
        return $this->getHydrator($object)->extract($object);
    }

    /**
     * Gets hydrator of an object
     *
     * @param  object $object
     * @return HydratorInterface
     */
    protected function getHydrator($object)
    {
        return $this->hydrators->get(get_class($object));
    }
}
