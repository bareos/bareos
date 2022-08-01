<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\InputFilter;

use Interop\Container\ContainerInterface;
use Zend\Filter\FilterPluginManager;
use Zend\ServiceManager\AbstractFactoryInterface;
use Zend\ServiceManager\ServiceLocatorInterface;
use Zend\Validator\ValidatorPluginManager;

class InputFilterAbstractServiceFactory implements AbstractFactoryInterface
{
    /**
     * @var Factory
     */
    protected $factory;

    /**
     * @param ContainerInterface      $services
     * @param string                  $rName
     * @param array                   $options
     * @return InputFilterInterface
     */
    public function __invoke(ContainerInterface $services, $rName, array  $options = null)
    {
        $allConfig = $services->get('config');
        $config    = $allConfig['input_filter_specs'][$rName];
        $factory   = $this->getInputFilterFactory($services);

        return $factory->createInputFilter($config);
    }

    /**
     *
     * @param ContainerInterface $services
     * @param string                  $cName
     * @param string                  $rName
     * @return bool
     */
    public function canCreate(ContainerInterface $services, $rName)
    {
        if (! $services->has('config')) {
            return false;
        }

        $config = $services->get('config');
        if (!isset($config['input_filter_specs'][$rName])
            || !is_array($config['input_filter_specs'][$rName])
        ) {
            return false;
        }

        return true;
    }

    /**
     * Determine if we can create a service with name (v2)
     *
     * @param ServiceLocatorInterface $serviceLocator
     * @param $name
     * @param $requestedName
     * @return bool
     */
    public function canCreateServiceWithName(ServiceLocatorInterface $serviceLocator, $name, $requestedName)
    {
        // v2 => need to get parent service locator
        $services = $serviceLocator->getServiceLocator();

        // No parent locator => cannot create service.
        if (! $services) {
            return false;
        }

        return $this->canCreate($services, $requestedName);
    }

    /**
     * @param ServiceLocatorInterface $inputFilters
     * @param string                  $cName
     * @param string                  $rName
     * @return InputFilterInterface
     */
    public function createServiceWithName(ServiceLocatorInterface $inputFilters, $cName, $rName)
    {
        // v2 => need to get parent service locator
        $services = $inputFilters->getServiceLocator();

        // No parent locator => cannot create service.
        if (! $services) {
            return false;
        }

        return $this($services, $rName);
    }

    /**
     * @param ServiceLocatorInterface $services
     * @return Factory
     */
    protected function getInputFilterFactory(ServiceLocatorInterface $services)
    {
        if ($this->factory instanceof Factory) {
            return $this->factory;
        }

        $this->factory = new Factory();
        $this->factory
            ->getDefaultFilterChain()
            ->setPluginManager($this->getFilterPluginManager($services));
        $this->factory
            ->getDefaultValidatorChain()
            ->setPluginManager($this->getValidatorPluginManager($services));

        return $this->factory;
    }

    /**
     * @param ServiceLocatorInterface $services
     * @return FilterPluginManager
     */
    protected function getFilterPluginManager(ServiceLocatorInterface $services)
    {
        if ($services->has('FilterManager')) {
            return $services->get('FilterManager');
        }

        return new FilterPluginManager($services);
    }

    /**
     * @param ServiceLocatorInterface $services
     * @return ValidatorPluginManager
     */
    protected function getValidatorPluginManager(ServiceLocatorInterface $services)
    {
        if ($services->has('ValidatorManager')) {
            return $services->get('ValidatorManager');
        }

        return new ValidatorPluginManager($services);
    }
}
