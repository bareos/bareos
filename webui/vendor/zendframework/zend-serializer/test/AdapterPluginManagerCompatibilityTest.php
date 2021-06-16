<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zend-serializer for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Serializer;

use PHPUnit\Framework\TestCase;
use ReflectionProperty;
use Zend\Serializer\AdapterPluginManager;
use Zend\Serializer\Adapter;
use Zend\Serializer\Exception\RuntimeException;
use Zend\ServiceManager\ServiceManager;
use Zend\ServiceManager\Test\CommonPluginManagerTrait;

class AdapterPluginManagerCompatibilityTest extends TestCase
{
    use CommonPluginManagerTrait;

    protected function getPluginManager()
    {
        return new AdapterPluginManager(new ServiceManager());
    }

    protected function getV2InvalidPluginException()
    {
        return RuntimeException::class;
    }

    protected function getInstanceOf()
    {
        return Adapter\AdapterInterface::class;
    }

    /**
     * Overrides CommonPluginManagerTrait::aliasProvider
     *
     * Iterates through aliases, and for adapters that require extensions,
     * tests if the extension is loaded, skipping that alias if not.
     *
     * @return \Traversable
     */
    public function aliasProvider()
    {
        $pluginManager = $this->getPluginManager();
        $r = new ReflectionProperty($pluginManager, 'aliases');
        $r->setAccessible(true);
        $aliases = $r->getValue($pluginManager);

        foreach ($aliases as $alias => $target) {
            switch ($target) {
                case Adapter\IgBinary::class:
                    if (extension_loaded('igbinary')) {
                        yield $alias => [$alias, $target];
                    }
                    break;
                case Adapter\MsgPack::class:
                    if (extension_loaded('msgpack')) {
                        yield $alias => [$alias, $target];
                    }
                    break;
                case Adapter\Wddx::class:
                    if (extension_loaded('wddx')) {
                        yield $alias => [$alias, $target];
                    }
                    break;
                default:
                    yield $alias => [$alias, $target];
            }
        }
    }
}
