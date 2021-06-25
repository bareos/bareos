<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zend-log for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log;

use PHPUnit\Framework\TestCase;
use ReflectionProperty;
use Zend\Log\Exception\InvalidArgumentException;
use Zend\Log\Writer;
use Zend\Log\WriterPluginManager;
use Zend\ServiceManager\ServiceManager;
use Zend\ServiceManager\Test\CommonPluginManagerTrait;

class WriterPluginManagerCompatibilityTest extends TestCase
{
    use CommonPluginManagerTrait;
    use ServicesNotSharedByDefaultTrait;

    protected function getPluginManager()
    {
        return new WriterPluginManager(new ServiceManager());
    }

    protected function getV2InvalidPluginException()
    {
        return InvalidArgumentException::class;
    }

    protected function getInstanceOf()
    {
        return Writer\WriterInterface::class;
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
                case Writer\Mail::class:
                    // intentionally fall-through
                case Writer\Db::class:
                    // intentionally fall-through
                case Writer\FingersCrossed::class:
                    // intentionally fall-through
                case Writer\Mongo::class:
                    // intentionally fall-through
                case Writer\MongoDB::class:
                    // intentionally fall-through
                case Writer\Stream::class:
                    // always skip; these implementations have required arguments
                    break;
                default:
                    yield $alias => [$alias, $target];
            }
        }
    }
}
