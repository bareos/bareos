<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace Zend\ModuleManager\Listener;

use Zend\ModuleManager\Exception;
use Zend\ModuleManager\Feature\DependencyIndicatorInterface;
use Zend\ModuleManager\ModuleEvent;

/**
 * Module resolver listener
 */
class ModuleDependencyCheckerListener
{
    /**
     * @var array of already loaded modules, indexed by module name
     */
    protected $loaded = [];

    /**
     * @param \Zend\ModuleManager\ModuleEvent $e
     *
     * @throws Exception\MissingDependencyModuleException
     */
    public function __invoke(ModuleEvent $e)
    {
        $module = $e->getModule();

        if ($module instanceof DependencyIndicatorInterface || method_exists($module, 'getModuleDependencies')) {
            $dependencies = $module->getModuleDependencies();

            foreach ($dependencies as $dependencyModule) {
                if (! isset($this->loaded[$dependencyModule])) {
                    throw new Exception\MissingDependencyModuleException(
                        sprintf(
                            'Module "%s" depends on module "%s", which was not initialized before it',
                            $e->getModuleName(),
                            $dependencyModule
                        )
                    );
                }
            }
        }

        $this->loaded[$e->getModuleName()] = true;
    }
}
