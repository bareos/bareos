<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache;

use PHPUnit\Framework\TestCase;
use Zend\Cache\Exception\RuntimeException;
use Zend\Cache\Pattern\PatternInterface;
use Zend\Cache\PatternPluginManager;
use Zend\Cache\Storage\StorageInterface;
use Zend\ServiceManager\ServiceManager;
use Zend\ServiceManager\Test\CommonPluginManagerTrait;

class PatternPluginManagerTest extends TestCase
{
    use CommonPluginManagerTrait;

    protected function getPluginManager()
    {
        return new PatternPluginManager(new ServiceManager());
    }

    protected function getV2InvalidPluginException()
    {
        return RuntimeException::class;
    }

    protected function getInstanceOf()
    {
        return PatternInterface::class;
    }

    public function testGetWillInjectProvidedOptionsAsPatternOptionsInstance()
    {
        $plugins = $this->getPluginManager();
        $storage = $this->prophesize(StorageInterface::class)->reveal();
        $plugin = $plugins->get('callback', [
            'cache_output' => false,
            'storage' => $storage,
        ]);
        $options = $plugin->getOptions();
        $this->assertFalse($options->getCacheOutput());
        $this->assertSame($storage, $options->getStorage());
    }

    public function testBuildWillInjectProvidedOptionsAsPatternOptionsInstance()
    {
        $plugins = $this->getPluginManager();

        if (! method_exists($plugins, 'configure')) {
            $this->markTestSkipped('Test is only relevant for zend-servicemanager v3');
        }

        $storage = $this->prophesize(StorageInterface::class)->reveal();
        $plugin = $plugins->build('callback', [
            'cache_output' => false,
            'storage' => $storage,
        ]);
        $options = $plugin->getOptions();
        $this->assertFalse($options->getCacheOutput());
        $this->assertSame($storage, $options->getStorage());
    }
}
