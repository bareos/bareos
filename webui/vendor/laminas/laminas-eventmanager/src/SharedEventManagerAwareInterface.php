<?php

/**
 * @see       https://github.com/laminas/laminas-eventmanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-eventmanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-eventmanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\EventManager;

/**
 * Interface to automate setter injection for a SharedEventManagerInterface instance
 *
 * @deprecated This interface is deprecated with 2.6.0, and will be removed in 3.0.0.
 *     See {@link https://github.com/laminas/laminas-eventmanager/blob/develop/doc/book/migration/removed.md}
 *     for details.
 */
interface SharedEventManagerAwareInterface extends SharedEventsCapableInterface
{
    /**
     * Inject a SharedEventManager instance
     *
     * @param  SharedEventManagerInterface $sharedEventManager
     * @return SharedEventManagerAwareInterface
     */
    public function setSharedManager(SharedEventManagerInterface $sharedEventManager);

    /**
     * Remove any shared collections
     *
     * @return void
     */
    public function unsetSharedManager();
}
