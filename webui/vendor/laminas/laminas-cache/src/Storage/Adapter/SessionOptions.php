<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Storage\Adapter;

use Laminas\Session\Container as SessionContainer;

/**
 * These are options specific to the APC adapter
 */
class SessionOptions extends AdapterOptions
{
    /**
     * The session container
     *
     * @var null|SessionContainer
     */
    protected $sessionContainer = null;

    /**
     * Set the session container
     *
     * @param  null|SessionContainer $sessionContainer
     * @return SessionOptions
     */
    public function setSessionContainer(SessionContainer $sessionContainer = null)
    {
        if ($this->sessionContainer != $sessionContainer) {
            $this->triggerOptionEvent('session_container', $sessionContainer);
            $this->sessionContainer = $sessionContainer;
        }

        return $this;
    }

    /**
     * Get the session container
     *
     * @return null|SessionContainer
     */
    public function getSessionContainer()
    {
        return $this->sessionContainer;
    }
}
