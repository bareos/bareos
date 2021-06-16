<?php
/**
 * @see       https://github.com/zendframework/zend-loader for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-loader/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Loader\TestAsset;

/**
 * @group      Loader
 */
class TestPluginMap implements \IteratorAggregate
{
    /**
     * Plugin map
     *
     * @var array
     */
    public $map = array(
        'map'    => __CLASS__,
        'test'   => 'ZendTest\Loader\PluginClassLoaderTest',
        'loader' => 'Zend\Loader\PluginClassLoader',
    );

    /**
     * Return iterator
     *
     * @return Traversable
     */
    public function getIterator()
    {
        return new \ArrayIterator($this->map);
    }
}
