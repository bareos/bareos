<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator;

use Laminas\ServiceManager\AbstractPluginManager;
use Laminas\ServiceManager\Exception\InvalidServiceException;
use Laminas\ServiceManager\Factory\InvokableFactory;

/**
 * Plugin manager implementation for hydrators.
 *
 * Enforces that adapters retrieved are instances of HydratorInterface
 */
class HydratorPluginManager extends AbstractPluginManager
{
    /**
     * Default aliases
     *
     * @var array
     */
    protected $aliases = [
        'arrayserializable'  => ArraySerializable::class,
        'arraySerializable'  => ArraySerializable::class,
        'ArraySerializable'  => ArraySerializable::class,
        'classmethods'       => ClassMethods::class,
        'classMethods'       => ClassMethods::class,
        'ClassMethods'       => ClassMethods::class,
        'delegatinghydrator' => DelegatingHydrator::class,
        'delegatingHydrator' => DelegatingHydrator::class,
        'DelegatingHydrator' => DelegatingHydrator::class,
        'objectproperty'     => ObjectProperty::class,
        'objectProperty'     => ObjectProperty::class,
        'ObjectProperty'     => ObjectProperty::class,
        'reflection'         => Reflection::class,
        'Reflection'         => Reflection::class,

        // Legacy Zend Framework aliases
        \Zend\Hydrator\ArraySerializable::class => ArraySerializable::class,
        \Zend\Hydrator\ClassMethods::class => ClassMethods::class,
        \Zend\Hydrator\DelegatingHydrator::class => DelegatingHydrator::class,
        \Zend\Hydrator\ObjectProperty::class => ObjectProperty::class,
        \Zend\Hydrator\Reflection::class => Reflection::class,

        // v2 normalized FQCNs
        'zendhydratorarrayserializable' => ArraySerializable::class,
        'zendhydratorclassmethods' => ClassMethods::class,
        'zendhydratordelegatinghydrator' => DelegatingHydrator::class,
        'zendhydratorobjectproperty' => ObjectProperty::class,
        'zendhydratorreflection' => Reflection::class,
    ];

    /**
     * Default factory-based adapters
     *
     * @var array
     */
    protected $factories = [
        ArraySerializable::class                => InvokableFactory::class,
        ClassMethods::class                     => InvokableFactory::class,
        DelegatingHydrator::class               => DelegatingHydratorFactory::class,
        ObjectProperty::class                   => InvokableFactory::class,
        Reflection::class                       => InvokableFactory::class,

        // v2 normalized FQCNs
        'laminashydratorarrayserializable'         => InvokableFactory::class,
        'laminashydratorclassmethods'              => InvokableFactory::class,
        'laminashydratordelegatinghydrator'        => DelegatingHydratorFactory::class,
        'laminashydratorobjectproperty'            => InvokableFactory::class,
        'laminashydratorreflection'                => InvokableFactory::class,
    ];

    /**
     * Whether or not to share by default (v3)
     *
     * @var bool
     */
    protected $sharedByDefault = false;

    /**
     * Whether or not to share by default (v2)
     *
     * @var bool
     */
    protected $shareByDefault = false;

    /**
     * {inheritDoc}
     */
    protected $instanceOf = HydratorInterface::class;

    /**
     * Validate the plugin is of the expected type (v3).
     *
     * Checks that the filter loaded is either a valid callback or an instance
     * of FilterInterface.
     *
     * @param mixed $instance
     * @throws InvalidServiceException
     */
    public function validate($instance)
    {
        if ($instance instanceof $this->instanceOf) {
            // we're okay
            return;
        }

        throw new InvalidServiceException(sprintf(
            'Plugin of type %s is invalid; must implement Laminas\Hydrator\HydratorInterface',
            (is_object($instance) ? get_class($instance) : gettype($instance))
        ));
    }

    /**
     * {@inheritDoc} (v2)
     */
    public function validatePlugin($plugin)
    {
        try {
            $this->validate($plugin);
        } catch (InvalidServiceException $e) {
            throw new Exception\RuntimeException($e->getMessage(), $e->getCode(), $e);
        }
    }
}
