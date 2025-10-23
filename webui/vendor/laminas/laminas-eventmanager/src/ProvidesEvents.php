<?php

/**
 * @see       https://github.com/laminas/laminas-eventmanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-eventmanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-eventmanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\EventManager;

trigger_error('Laminas\EventManager\ProvidesEvents has been deprecated in favor of Laminas\EventManager\EventManagerAwareTrait; please update your code', E_USER_DEPRECATED);

/**
 * @deprecated Please use EventManagerAwareTrait instead.
 *
 * This trait exists solely for backwards compatibility in the 2.x branch and
 * will likely be removed in 3.x.
 */
trait ProvidesEvents
{
    use EventManagerAwareTrait;
}
