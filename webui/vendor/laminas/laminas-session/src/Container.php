<?php

/**
 * @see       https://github.com/laminas/laminas-session for the canonical source repository
 * @copyright https://github.com/laminas/laminas-session/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-session/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Session;

/**
 * Session storage container
 *
 * Allows for interacting with session storage in isolated containers, which
 * may have their own expiries, or even expiries per key in the container.
 * Additionally, expiries may be absolute TTLs or measured in "hops", which
 * are based on how many times the key or container were accessed.
 */
class Container extends AbstractContainer
{
    /**
     * Retrieve a specific key in the container
     *
     * @param  string $key
     * @return mixed
     */
    public function &offsetGet($key)
    {
        $ret = null;
        if (! $this->offsetExists($key)) {
            return $ret;
        }
        $storage = $this->getStorage();
        $name    = $this->getName();
        $ret =& $storage[$name][$key];

        return $ret;
    }
}
