<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\EventManager;

use ArrayAccess;
use ArrayObject;
use Traversable;
use Zend\Stdlib\CallbackHandler;
use Zend\Stdlib\PriorityQueue;

/**
 * Event manager: notification system
 *
 * Use the EventManager when you want to create a per-instance notification
 * system for your objects.
 */
class EventManager implements EventManagerInterface
{
    /**
     * Subscribed events and their listeners
     * @var array Array of PriorityQueue objects
     */
    protected $events = [];

    /**
     * @var string Class representing the event being emitted
     */
    protected $eventClass = 'Zend\EventManager\Event';

    /**
     * Identifiers, used to pull shared signals from SharedEventManagerInterface instance
     * @var array
     */
    protected $identifiers = [];

    /**
     * Shared event manager
     * @var false|null|SharedEventManagerInterface
     */
    protected $sharedManager = null;

    /**
     * Constructor
     *
     * Allows optionally specifying identifier(s) to use to pull signals from a
     * SharedEventManagerInterface.
     *
     * @param  null|string|int|array|Traversable $identifiers
     */
    public function __construct($identifiers = null)
    {
        $this->setIdentifiers($identifiers);
    }

    /**
     * Set the event class to utilize
     *
     * @deprecated This method is deprecated with 2.6.0, and will be removed in 3.0.0.
     *     See {@link https://github.com/zendframework/zend-eventmanager/blob/develop/doc/book/migration/removed.md}
     *     for details.
     * @param  string $class
     * @return EventManager
     */
    public function setEventClass($class)
    {
        $this->eventClass = $class;
        return $this;
    }

    /**
     * Set shared event manager
     *
     * @deprecated This method is deprecated with 2.6.0, and will be removed in 3.0.0.
     *     See {@link https://github.com/zendframework/zend-eventmanager/blob/develop/doc/book/migration/removed.md}
     *     for details.
     * @param SharedEventManagerInterface $sharedEventManager
     * @return EventManager
     */
    public function setSharedManager(SharedEventManagerInterface $sharedEventManager)
    {
        $this->sharedManager = $sharedEventManager;
        StaticEventManager::setInstance($sharedEventManager);
        return $this;
    }

    /**
     * Remove any shared event manager currently attached
     *
     * @deprecated This method is deprecated with 2.6.0, and will be removed in 3.0.0.
     *     See {@link https://github.com/zendframework/zend-eventmanager/blob/develop/doc/book/migration/removed.md}
     *     for details.
     * @return void
     */
    public function unsetSharedManager()
    {
        $this->sharedManager = false;
    }

    /**
     * Get shared event manager
     *
     * If one is not defined, but we have a static instance in
     * StaticEventManager, that one will be used and set in this instance.
     *
     * If none is available in the StaticEventManager, a boolean false is
     * returned.
     *
     * @return false|SharedEventManagerInterface
     */
    public function getSharedManager()
    {
        // "false" means "I do not want a shared manager; don't try and fetch one"
        if (false === $this->sharedManager
            || $this->sharedManager instanceof SharedEventManagerInterface
        ) {
            return $this->sharedManager;
        }

        if (!StaticEventManager::hasInstance()) {
            return false;
        }

        $this->sharedManager = StaticEventManager::getInstance();
        return $this->sharedManager;
    }

    /**
     * Get the identifier(s) for this EventManager
     *
     * @return array
     */
    public function getIdentifiers()
    {
        return $this->identifiers;
    }

    /**
     * Set the identifiers (overrides any currently set identifiers)
     *
     * @param string|int|array|Traversable $identifiers
     * @return EventManager Provides a fluent interface
     */
    public function setIdentifiers($identifiers)
    {
        if (is_array($identifiers) || $identifiers instanceof Traversable) {
            $this->identifiers = array_unique((array) $identifiers);
        } elseif ($identifiers !== null) {
            $this->identifiers = [$identifiers];
        }
        return $this;
    }

    /**
     * Add some identifier(s) (appends to any currently set identifiers)
     *
     * @param string|int|array|Traversable $identifiers
     * @return EventManager Provides a fluent interface
     */
    public function addIdentifiers($identifiers)
    {
        if (is_array($identifiers) || $identifiers instanceof Traversable) {
            $this->identifiers = array_unique(array_merge($this->identifiers, (array) $identifiers));
        } elseif ($identifiers !== null) {
            $this->identifiers = array_unique(array_merge($this->identifiers, [$identifiers]));
        }
        return $this;
    }

    /**
     * Trigger all listeners for a given event
     *
     * @param  string|EventInterface $event
     * @param  string|object     $target   Object calling emit, or symbol describing target (such as static method name)
     * @param  array|ArrayAccess $argv     Array of arguments; typically, should be associative
     * @param  null|callable     $callback Trigger listeners until return value of this callback evaluate to true
     * @return ResponseCollection All listener return values
     * @throws Exception\InvalidCallbackException
     */
    public function trigger($event, $target = null, $argv = [], $callback = null)
    {
        if ($event instanceof EventInterface) {
            $e        = $event;
            $event    = $e->getName();
            $callback = $target;
        } elseif ($target instanceof EventInterface) {
            $e = $target;
            $e->setName($event);
            $callback = $argv;
        } elseif ($argv instanceof EventInterface) {
            $e = $argv;
            $e->setName($event);
            $e->setTarget($target);
        } else {
            $e = new $this->eventClass();
            $e->setName($event);
            $e->setTarget($target);
            $e->setParams($argv);
        }

        if ($callback && !is_callable($callback)) {
            throw new Exception\InvalidCallbackException('Invalid callback provided');
        }

        return $this->triggerListeners($event, $e, $callback);
    }

    /**
     * Trigger listeners until return value of one causes a callback to
     * evaluate to true
     *
     * Triggers listeners until the provided callback evaluates the return
     * value of one as true, or until all listeners have been executed.
     *
     * @deprecated The signature of this method will change in 3.0.0.
     *     See {@link https://github.com/zendframework/zend-eventmanager/blob/develop/doc/book/migration/changed.md}
     *     for details.
     * @param  string|EventInterface $event
     * @param  string|object $target Object calling emit, or symbol describing target (such as static method name)
     * @param  array|ArrayAccess $argv Array of arguments; typically, should be associative
     * @param  callable $callback
     * @return ResponseCollection
     * @throws Exception\InvalidCallbackException if invalid callable provided
     */
    public function triggerUntil($event, $target, $argv = null, $callback = null)
    {
        trigger_error(
            'This method is deprecated and will be removed in the future. Please use trigger() instead.',
            E_USER_DEPRECATED
        );
        return $this->trigger($event, $target, $argv, $callback);
    }

    /**
     * Trigger an event instance.
     *
     * @param EventInterface $event
     * @return ResponseCollection
     */
    public function triggerEvent(EventInterface $event)
    {
        return $this->triggerListeners($event->getName(), $event);
    }

    /**
     * Trigger an event instance, short-circuiting if a listener response evaluates true via the callback.
     *
     * @param callable $callback
     * @param EventInterface $event
     * @return ResponseCollection
     */
    public function triggerEventUntil(callable $callback, EventInterface $event)
    {
        return $this->triggerListeners($event->getName(), $event, $callback);
    }

    /**
     * Attach a listener to an event
     *
     * The first argument is the event, and the next argument describes a
     * callback that will respond to that event. A CallbackHandler instance
     * describing the event listener combination will be returned.
     *
     * The last argument indicates a priority at which the event should be
     * executed. By default, this value is 1; however, you may set it for any
     * integer value. Higher values have higher priority (i.e., execute first).
     *
     * You can specify "*" for the event name. In such cases, the listener will
     * be triggered for every event.
     *
     * @param  string|array|ListenerAggregateInterface $event An event or array of event names. If a ListenerAggregateInterface, proxies to {@link attachAggregate()}.
     * @param  callable|int $callback If string $event provided, expects PHP callback; for a ListenerAggregateInterface $event, this will be the priority
     * @param  int $priority If provided, the priority at which to register the callable
     * @return CallbackHandler|mixed CallbackHandler if attaching callable (to allow later unsubscribe); mixed if attaching aggregate
     * @throws Exception\InvalidArgumentException
     */
    public function attach($event, $callback = null, $priority = 1)
    {
        // Proxy ListenerAggregateInterface arguments to attachAggregate()
        if ($event instanceof ListenerAggregateInterface) {
            return $this->attachAggregate($event, $callback);
        }

        // Null callback is invalid
        if (null === $callback) {
            throw new Exception\InvalidArgumentException(sprintf(
                '%s: expects a callback; none provided',
                __METHOD__
            ));
        }

        // Array of events should be registered individually, and return an array of all listeners
        if (is_array($event)) {
            $listeners = [];
            foreach ($event as $name) {
                $listeners[] = $this->attach($name, $callback, $priority);
            }
            return $listeners;
        }

        // If we don't have a priority queue for the event yet, create one
        if (empty($this->events[$event])) {
            $this->events[$event] = new PriorityQueue();
        }

        // Create a callback handler, setting the event and priority in its metadata
        $listener = new CallbackHandler($callback, ['event' => $event, 'priority' => $priority]);

        // Inject the callback handler into the queue
        $this->events[$event]->insert($listener, $priority);
        return $listener;
    }

    /**
     * Attach a listener aggregate
     *
     * Listener aggregates accept an EventManagerInterface instance, and call attach()
     * one or more times, typically to attach to multiple events using local
     * methods.
     *
     * @deprecated This method is deprecated with 2.6.0, and will be removed in 3.0.0.
     *     See {@link https://github.com/zendframework/zend-eventmanager/blob/develop/doc/book/migration/removed.md}
     *     for details.
     * @param  ListenerAggregateInterface $aggregate
     * @param  int $priority If provided, a suggested priority for the aggregate to use
     * @return mixed return value of {@link ListenerAggregateInterface::attach()}
     */
    public function attachAggregate(ListenerAggregateInterface $aggregate, $priority = 1)
    {
        return $aggregate->attach($this, $priority);
    }

    /**
     * Unsubscribe a listener from an event
     *
     * @param  CallbackHandler|ListenerAggregateInterface $listener
     * @return bool Returns true if event and listener found, and unsubscribed; returns false if either event or listener not found
     * @throws Exception\InvalidArgumentException if invalid listener provided
     */
    public function detach($listener)
    {
        if ($listener instanceof ListenerAggregateInterface) {
            return $this->detachAggregate($listener);
        }

        if (!$listener instanceof CallbackHandler) {
            throw new Exception\InvalidArgumentException(sprintf(
                '%s: expected a ListenerAggregateInterface or CallbackHandler; received "%s"',
                __METHOD__,
                (is_object($listener) ? get_class($listener) : gettype($listener))
            ));
        }

        $event = $listener->getMetadatum('event');
        if (!$event || empty($this->events[$event])) {
            return false;
        }
        $return = $this->events[$event]->remove($listener);
        if (!$return) {
            return false;
        }
        if (!count($this->events[$event])) {
            unset($this->events[$event]);
        }
        return true;
    }

    /**
     * Detach a listener aggregate
     *
     * Listener aggregates accept an EventManagerInterface instance, and call detach()
     * of all previously attached listeners.
     *
     * @deprecated This method is deprecated with 2.6.0, and will be removed in 3.0.0.
     *     See {@link https://github.com/zendframework/zend-eventmanager/blob/develop/doc/book/migration/removed.md}
     *     for details.
     * @param  ListenerAggregateInterface $aggregate
     * @return mixed return value of {@link ListenerAggregateInterface::detach()}
     */
    public function detachAggregate(ListenerAggregateInterface $aggregate)
    {
        return $aggregate->detach($this);
    }

    /**
     * Retrieve all registered events
     *
     * @deprecated This method is deprecated with 2.6.0, and will be removed in 3.0.0.
     *     See {@link https://github.com/zendframework/zend-eventmanager/blob/develop/doc/book/migration/removed.md}
     *     for details.
     * @return array
     */
    public function getEvents()
    {
        return array_keys($this->events);
    }

    /**
     * Retrieve all listeners for a given event
     *
     * @deprecated This method is deprecated with 2.6.0, and will be removed in 3.0.0.
     *     See {@link https://github.com/zendframework/zend-eventmanager/blob/develop/doc/book/migration/removed.md}
     *     for details.
     * @param  string $event
     * @return PriorityQueue
     */
    public function getListeners($event)
    {
        if (!array_key_exists($event, $this->events)) {
            return new PriorityQueue();
        }
        return $this->events[$event];
    }

    /**
     * Clear all listeners for a given event
     *
     * @param  string $event
     * @return void
     */
    public function clearListeners($event)
    {
        if (!empty($this->events[$event])) {
            unset($this->events[$event]);
        }
    }

    /**
     * Prepare arguments
     *
     * Use this method if you want to be able to modify arguments from within a
     * listener. It returns an ArrayObject of the arguments, which may then be
     * passed to trigger().
     *
     * @param  array $args
     * @return ArrayObject
     */
    public function prepareArgs(array $args)
    {
        return new ArrayObject($args);
    }

    /**
     * Trigger listeners
     *
     * Actual functionality for triggering listeners, to which trigger() delegate.
     *
     * @param  string           $event Event name
     * @param  EventInterface $e
     * @param  null|callable    $callback
     * @return ResponseCollection
     */
    protected function triggerListeners($event, EventInterface $e, $callback = null)
    {
        // Initial value of stop propagation flag should be false
        $e->stopPropagation(false);

        $responses = new ResponseCollection;
        $listeners = $this->getListeners($event);

        // Add shared/wildcard listeners to the list of listeners,
        // but don't modify the listeners object
        $sharedListeners         = $this->getSharedListeners($event);
        $sharedWildcardListeners = $this->getSharedListeners('*');
        $wildcardListeners       = $this->getListeners('*');
        if (count($sharedListeners) || count($sharedWildcardListeners) || count($wildcardListeners)) {
            $listeners = clone $listeners;

            // Shared listeners on this specific event
            $this->insertListeners($listeners, $sharedListeners);

            // Shared wildcard listeners
            $this->insertListeners($listeners, $sharedWildcardListeners);

            // Add wildcard listeners
            $this->insertListeners($listeners, $wildcardListeners);
        }

        foreach ($listeners as $listener) {
            $listenerCallback = $listener->getCallback();

            // Trigger the listener's callback, and push its result onto the
            // response collection
            $responses->push(call_user_func($listenerCallback, $e));

            // If the event was asked to stop propagating, do so
            if ($e->propagationIsStopped()) {
                $responses->setStopped(true);
                break;
            }

            // If the result causes our validation callback to return true,
            // stop propagation
            if ($callback && call_user_func($callback, $responses->last())) {
                $responses->setStopped(true);
                break;
            }
        }

        return $responses;
    }

    /**
     * Get list of all listeners attached to the shared event manager for
     * identifiers registered by this instance
     *
     * @param  string $event
     * @return array
     */
    protected function getSharedListeners($event)
    {
        if (!$sharedManager = $this->getSharedManager()) {
            return [];
        }

        $identifiers     = $this->getIdentifiers();
        //Add wildcard id to the search, if not already added
        if (!in_array('*', $identifiers)) {
            $identifiers[] = '*';
        }
        $sharedListeners = [];

        foreach ($identifiers as $id) {
            if (!$listeners = $sharedManager->getListeners($id, $event)) {
                continue;
            }

            if (!is_array($listeners) && !($listeners instanceof Traversable)) {
                continue;
            }

            foreach ($listeners as $listener) {
                if (!$listener instanceof CallbackHandler) {
                    continue;
                }
                $sharedListeners[] = $listener;
            }
        }

        return $sharedListeners;
    }

    /**
     * Add listeners to the master queue of listeners
     *
     * Used to inject shared listeners and wildcard listeners.
     *
     * @param  PriorityQueue $masterListeners
     * @param  array|Traversable $listeners
     * @return void
     */
    protected function insertListeners($masterListeners, $listeners)
    {
        foreach ($listeners as $listener) {
            $priority = $listener->getMetadatum('priority');
            if (null === $priority) {
                $priority = 1;
            } elseif (is_array($priority)) {
                // If we have an array, likely using PriorityQueue. Grab first
                // element of the array, as that's the actual priority.
                $priority = array_shift($priority);
            }
            $masterListeners->insert($listener, $priority);
        }
    }
}
