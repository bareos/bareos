<?php
/**
 * @see       https://github.com/zendframework/zend-loader for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-loader/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Loader\TestAsset;

use Zend\ServiceManager\ServiceLocatorInterface;

class ServiceLocator implements ServiceLocatorInterface
{
    protected $services = array();

    public function get($name, array $params = array())
    {
        if (!isset($this->services[$name])) {
            return null;
        }

        return $this->services[$name];
    }

    public function has($name)
    {
        return (isset($this->services[$name]));
    }

    public function set($name, $object)
    {
        $this->services[$name] = $object;
    }
}
