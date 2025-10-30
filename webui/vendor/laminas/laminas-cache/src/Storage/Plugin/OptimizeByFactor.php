<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Storage\Plugin;

use Laminas\Cache\Storage\OptimizableInterface;
use Laminas\Cache\Storage\PostEvent;
use Laminas\EventManager\EventManagerInterface;

class OptimizeByFactor extends AbstractPlugin
{
    /**
     * {@inheritDoc}
     */
    public function attach(EventManagerInterface $events, $priority = 1)
    {
        $callback          = [$this, 'optimizeByFactor'];
        $this->listeners[] = $events->attach('removeItem.post', $callback, $priority);
        $this->listeners[] = $events->attach('removeItems.post', $callback, $priority);
    }

    /**
     * Optimize by factor on a success _RESULT_
     *
     * @param  PostEvent $event
     * @return void
     */
    public function optimizeByFactor(PostEvent $event)
    {
        $storage = $event->getStorage();
        if (!($storage instanceof OptimizableInterface)) {
            return;
        }

        $factor = $this->getOptions()->getOptimizingFactor();
        if ($factor && mt_rand(1, $factor) == 1) {
            $storage->optimize();
        }
    }
}
