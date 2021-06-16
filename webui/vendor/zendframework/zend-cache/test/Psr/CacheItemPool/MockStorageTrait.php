<?php
/**
 * @see       https://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-cache/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Cache\Psr\CacheItemPool;

use DateInterval;
use DateTime;
use DateTimeZone;
use Prophecy\Argument;
use Zend\Cache\Storage\Adapter\AbstractAdapter;
use Zend\Cache\Storage\Adapter\AdapterOptions;
use Zend\Cache\Storage\Capabilities;
use Zend\Cache\Storage\ClearByNamespaceInterface;
use Zend\Cache\Storage\FlushableInterface;
use Zend\Cache\Storage\StorageInterface;
use Zend\EventManager\EventManager;

trait MockStorageTrait
{
    protected $defaultCapabilities = [
        'staticTtl' => true,
        'minTtl' => 1,
        'supportedDatatypes' => [
            'NULL'     => true,
            'boolean'  => true,
            'integer'  => true,
            'double'   => true,
            'string'   => true,
            'array'    => true,
            'object'   => 'object',
            'resource' => false,
        ],
    ];

    protected function getStorageProphesy($capabilities = false, $options = false, $class = StorageInterface::class)
    {
        if ($capabilities === false) {
            $capabilities = $this->defaultCapabilities;
        }
        if ($options === false) {
            $options = [];
        }

        // cached items
        $items = [];

        $storage = $this->prophesize($class);
        $storage->willImplement(FlushableInterface::class);
        if (array_key_exists('namespace', $options)) {
            $storage->willImplement(ClearByNamespaceInterface::class);
        }
        if ($class == AbstractAdapter::class) {
            $eventManager = new EventManager();
            $storage->getEventManager()->willReturn($eventManager);
        }
        $storage->getCapabilities()
            ->will(function () use ($capabilities) {
                return new Capabilities(
                    $this->reveal(),
                    new \stdClass(),
                    $capabilities
                );
            });
        $storage->getOptions()
            ->will(function () use ($options) {
                $adapterOptions = new AdapterOptions($options);
                $this->getOptions()->willReturn($adapterOptions);
                return $adapterOptions;
            });

        $storage->hasItem(Argument::type('string'))
            ->will(function ($args) use (&$items) {
                $key = $args[0];
                if (! isset($items[$key])) {
                    return false;
                }
                return $items[$key]['ttd'] > new DateTime('now', new DateTimeZone('UTC'));
            });
        $storage->getItem(Argument::type('string'), Argument::any())
            ->will(function ($args) use (&$items) {
                $key = $args[0];
                if (isset($items[$key]) && $items[$key]['ttd'] > new DateTime('now', new DateTimeZone('UTC'))) {
                    return $items[$key]['value'];
                }
                return;
            });
        $storage->getItems(Argument::type('array'))
            ->will(function ($args) use (&$items) {
                $found = [];
                foreach (array_intersect_key($items, array_flip($args[0])) as $key => $item) {
                    if ($item['ttd'] > new DateTime('now', new DateTimeZone('UTC'))) {
                        $found[$key] = $item['value'];
                    }
                }
                return $found;
            });
        $storage->setItem(Argument::type('string'), Argument::any())
            ->will(function ($args) use (&$items) {
                $key = $args[0];
                $ttl = $this->reveal()->getOptions()->getTtl();
                if (! $ttl) {
                    $ttl = 3600;
                }
                $ttd = (new DateTime('now', new DateTimeZone('UTC')))->add(new DateInterval('PT' . $ttl . 'S'));
                $items[$key] = ['ttd' => $ttd, 'value' => $args[1]];
                return true;
            });
        $storage->flush()
            ->will(function () use (&$items) {
                $items = [];
                return true;
            });
        $storage->removeItems(Argument::type('array'))
            ->will(function ($args) use (&$items) {
                $items = array_diff_key($items, array_flip($args[0]));
                return true;
            });

        return $storage;
    }
}
