<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager\Listener;

use PHPUnit\Framework\TestCase;
use Zend\Loader\ModuleAutoloader;
use ZendTest\ModuleManager\ResetAutoloadFunctionsTrait;

/**
 * Common test methods for all AbstractListener children.
 */
class AbstractListenerTestCase extends TestCase
{
    use ResetAutoloadFunctionsTrait;

    /**
     * @before
     */
    protected function registerTestAssetsOnModuleAutoloader()
    {
        $autoloader = new ModuleAutoloader([
            dirname(__DIR__) . '/TestAsset',
        ]);
        $autoloader->register();
    }
}
