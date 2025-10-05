<?php

/**
 * @see       https://github.com/laminas/laminas-eventmanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-eventmanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-eventmanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\EventManager;

/**
 * Interface for self-registering event listeners.
 *
 * Classes implementing this interface may be registered by name or instance
 * with a SharedEventManager, without an event name. The {@link attach()} method will
 * then be called with the current SharedEventManager instance, allowing the class to
 * wire up one or more listeners.
 *
 * @deprecated This interface is deprecated with 2.6.0, and will be removed in 3.0.0.
 *     See {@link https://github.com/laminas/laminas-eventmanager/blob/develop/doc/book/migration/removed.md}
 *     for details.
 */
interface SharedListenerAggregateInterface
{
    /**
     * Attach one or more listeners
     *
     * Implementors may add an optional $priority argument; the SharedEventManager
     * implementation will pass this to the aggregate.
     *
     * @param SharedEventManagerInterface $events
     */
    public function attachShared(SharedEventManagerInterface $events);

    /**
     * Detach all previously attached listeners
     *
     * @param SharedEventManagerInterface $events
     */
    public function detachShared(SharedEventManagerInterface $events);
}
