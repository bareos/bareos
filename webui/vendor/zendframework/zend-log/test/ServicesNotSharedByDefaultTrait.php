<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zend-log for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log;

use ReflectionProperty;

trait ServicesNotSharedByDefaultTrait
{
    public function testServicesShouldNotBeSharedByDefault()
    {
        $plugins = $this->getPluginManager();

        $r = method_exists($plugins, 'configure')
            ? new ReflectionProperty($plugins, 'sharedByDefault')
            : new ReflectionProperty($plugins, 'shareByDefault');
        $r->setAccessible(true);
        $this->assertFalse($r->getValue($plugins));
    }
}
