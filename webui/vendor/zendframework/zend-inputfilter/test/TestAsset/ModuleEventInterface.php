<?php
/**
 * @link      https://github.com/zendframework/zend-inputfilter for the canonical source repository
 * @copyright Copyright (c) 2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-inputfilter/blob/master/LICENSE.md
 */

namespace ZendTest\InputFilter\TestAsset;

/**
 * Mock interface to use when testing Module::init
 *
 * Mimics Zend\ModuleManager\ModuleEvent methods called.
 */
interface ModuleEventInterface
{
    public function getParam($name, $default = null);
}
