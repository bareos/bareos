<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager\Listener;

use ListenerTestModule;
use ModuleAsClass;
use Zend\ModuleManager\Listener\ModuleResolverListener;
use Zend\ModuleManager\ModuleEvent;

/**
 * @covers \Zend\ModuleManager\Listener\AbstractListener
 * @covers \Zend\ModuleManager\Listener\ModuleResolverListener
 */
class ModuleResolverListenerTest extends AbstractListenerTestCase
{
    /**
     * @dataProvider validModuleNameProvider
     */
    public function testModuleResolverListenerCanResolveModuleClasses($moduleName, $expectedInstanceOf)
    {
        $moduleResolver = new ModuleResolverListener;
        $e = new ModuleEvent;

        $e->setModuleName($moduleName);
        $this->assertInstanceOf($expectedInstanceOf, $moduleResolver($e));
    }

    public function validModuleNameProvider()
    {
        return [
            // Description => [module name, expectedInstanceOf]
            'Append Module'  => ['ListenerTestModule', ListenerTestModule\Module::class],
            'FQCN Module'    => [ListenerTestModule\Module::class, ListenerTestModule\Module::class],
            'FQCN Arbitrary' => [ListenerTestModule\FooModule::class, ListenerTestModule\FooModule::class],
        ];
    }

    public function testModuleResolverListenerReturnFalseIfCannotResolveModuleClasses()
    {
        $moduleResolver = new ModuleResolverListener;
        $e = new ModuleEvent;

        $e->setModuleName('DoesNotExist');
        $this->assertFalse($moduleResolver($e));
    }

    public function testModuleResolverListenerPrefersModuleClassesInModuleNamespaceOverNamedClasses()
    {
        $moduleResolver = new ModuleResolverListener;
        $e = new ModuleEvent;

        $e->setModuleName('ModuleAsClass');
        $this->assertInstanceOf(ModuleAsClass\Module::class, $moduleResolver($e));
    }

    public function testModuleResolverListenerWillNotAttemptToResolveModuleAsClassNameGenerator()
    {
        $moduleResolver = new ModuleResolverListener;
        $e = new ModuleEvent;

        $e->setModuleName('Generator');
        $this->assertFalse($moduleResolver($e));
    }
}
