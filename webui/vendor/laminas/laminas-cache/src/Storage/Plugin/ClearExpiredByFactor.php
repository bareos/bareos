<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Storage\Plugin;

use Laminas\Cache\Storage\ClearExpiredInterface;
use Laminas\Cache\Storage\PostEvent;
use Laminas\EventManager\EventManagerInterface;

class ClearExpiredByFactor extends AbstractPlugin
{
    /**
     * {@inheritDoc}
     */
    public function attach(EventManagerInterface $events, $priority = 1)
    {
        $callback = [$this, 'clearExpiredByFactor'];

        $this->listeners[] = $events->attach('setItem.post', $callback, $priority);
        $this->listeners[] = $events->attach('setItems.post', $callback, $priority);
        $this->listeners[] = $events->attach('addItem.post', $callback, $priority);
        $this->listeners[] = $events->attach('addItems.post', $callback, $priority);
    }

    /**
     * Clear expired items by factor after writing new item(s)
     *
     * @param  PostEvent $event
     * @return void
     */
    public function clearExpiredByFactor(PostEvent $event)
    {
        $storage = $event->getStorage();
        if (!($storage instanceof ClearExpiredInterface)) {
            return;
        }

        $factor = $this->getOptions()->getClearingFactor();
        if ($factor && mt_rand(1, $factor) == 1) {
            $storage->clearExpired();
        }
    }
}
