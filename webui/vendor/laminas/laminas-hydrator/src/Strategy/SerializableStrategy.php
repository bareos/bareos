<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\Strategy;

use Laminas\Hydrator\Exception\InvalidArgumentException;
use Laminas\Serializer\Adapter\AdapterInterface as SerializerAdapter;
use Laminas\Serializer\Serializer as SerializerFactory;

class SerializableStrategy implements StrategyInterface
{
    /**
     * @var string|SerializerAdapter
     */
    protected $serializer;

    /**
     * @var array
     */
    protected $serializerOptions = [];

    /**
     *
     * @param mixed $serializer string or SerializerAdapter
     * @param mixed $serializerOptions
     */
    public function __construct($serializer, $serializerOptions = null)
    {
        $this->setSerializer($serializer);
        if ($serializerOptions) {
            $this->setSerializerOptions($serializerOptions);
        }
    }

    /**
     * Serialize the given value so that it can be extracted by the hydrator.
     *
     * @param mixed $value The original value.
     * @return mixed Returns the value that should be extracted.
     */
    public function extract($value)
    {
        $serializer = $this->getSerializer();
        return $serializer->serialize($value);
    }

    /**
     * Unserialize the given value so that it can be hydrated by the hydrator.
     *
     * @param mixed $value The original value.
     * @return mixed Returns the value that should be hydrated.
     */
    public function hydrate($value)
    {
        $serializer = $this->getSerializer();
        return $serializer->unserialize($value);
    }

    /**
     * Set serializer
     *
     * @param  string|SerializerAdapter $serializer
     * @return SerializableStrategy
     */
    public function setSerializer($serializer)
    {
        if (!is_string($serializer) && !$serializer instanceof SerializerAdapter) {
            throw new InvalidArgumentException(sprintf(
                '%s expects either a string serializer name or Laminas\Serializer\Adapter\AdapterInterface instance; '
                . 'received "%s"',
                __METHOD__,
                (is_object($serializer) ? get_class($serializer) : gettype($serializer))
            ));
        }
        $this->serializer = $serializer;
        return $this;
    }

    /**
     * Get serializer
     *
     * @return SerializerAdapter
     */
    public function getSerializer()
    {
        if (is_string($this->serializer)) {
            $options = $this->getSerializerOptions();
            $this->setSerializer(SerializerFactory::factory($this->serializer, $options));
        } elseif (null === $this->serializer) {
            $this->setSerializer(SerializerFactory::getDefaultAdapter());
        }

        return $this->serializer;
    }

    /**
     * Set configuration options for instantiating a serializer adapter
     *
     * @param  mixed $serializerOptions
     * @return SerializableStrategy
     */
    public function setSerializerOptions($serializerOptions)
    {
        $this->serializerOptions = $serializerOptions;
        return $this;
    }

    /**
     * Get configuration options for instantiating a serializer adapter
     *
     * @return mixed
     */
    public function getSerializerOptions()
    {
        return $this->serializerOptions;
    }
}
