<?php
/**
 * @link      http://github.com/zendframework/zend-inputfilter for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\InputFilter\TestAsset;

use Interop\Container\ContainerInterface;
use Zend\ServiceManager\AbstractFactoryInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class FooAbstractFactory implements AbstractFactoryInterface
{
    public function __invoke(ContainerInterface $container, $name, array $options = null)
    {
        return new Foo();
    }

    public function canCreate(ContainerInterface $container, $name)
    {
        if ($name == 'foo') {
            return true;
        }
    }

    public function canCreateServiceWithName(ServiceLocatorInterface $container, $name, $requestedName)
    {
        return $this->canCreate($container, $requestedName ?: $name);
    }

    public function createServiceWithName(ServiceLocatorInterface $container, $name, $requestedName)
    {
        return $this($container, $requestedName ?: $name);
    }
}
