<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager\Listener\TestAsset;

use Interop\Container\ContainerInterface;
use Zend\ServiceManager\FactoryInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class CustomPluginManagerFactory implements FactoryInterface
{
    /**
     * @var null|array
     */
    protected $creationOptions;

    /**
     * Create and return an instance of the CustomPluginManager (v3)
     *
     * @param ContainerInterface $container
     * @param string $name
     * @param null|array $options
     * @return CustomPluginManager
     */
    public function __invoke(ContainerInterface $container, $name, array $options = null)
    {
        $options = $options ?: [];
        return new CustomPluginManager($container, $options);
    }

    /**
     * Create and return an instance of the CustomPluginManager (v2)
     *
     * @param ServiceLocatorInterface $container
     * @return CustomPluginManager
     */
    public function createService(ServiceLocatorInterface $container)
    {
        return $this($container, CustomPluginManager::class, $this->creationOptions);
    }

    /**
     * Provide options to use during instantiation (v2).
     *
     * @param array $options
     */
    public function setCreationOptions(array $options)
    {
        $this->creationOptions = $options;
    }
}
