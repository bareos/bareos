<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace BafModule;

use Zend\Config\Config;

class Module
{
    public function getConfig()
    {
        return new Config(include __DIR__ . '/configs/config.php');
    }
}
