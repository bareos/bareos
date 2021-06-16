<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage;

use PHPUnit\Framework\TestCase;
use Zend\Cache\Exception\ExtensionNotLoadedException;
use Zend\Cache\Exception\RuntimeException;
use Zend\Cache\Storage\AdapterPluginManager;
use Zend\Cache\Storage\StorageInterface;
use Zend\ServiceManager\Exception\ServiceNotCreatedException;
use Zend\ServiceManager\ServiceManager;
use Zend\ServiceManager\Test\CommonPluginManagerTrait;

class AdapterPluginManagerTest extends TestCase
{
    use CommonPluginManagerTrait {
        testPluginAliasesResolve as commonPluginAliasesResolve;
    }

    /**
     * @dataProvider aliasProvider
     */
    public function testPluginAliasesResolve($alias, $expected)
    {
        try {
            $this->commonPluginAliasesResolve($alias, $expected);
        } catch (ServiceNotCreatedException $e) {
            // if we get as far as "extension not loaded" we've hit the constructor: alias has resolved
            if (! $e->getPrevious() instanceof ExtensionNotLoadedException) {
                $this->fail($e->getMessage());
            }
        }
        $this->addToAssertionCount(1);
    }

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
        return StorageInterface::class;
    }
}
