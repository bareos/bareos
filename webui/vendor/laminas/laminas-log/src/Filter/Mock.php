<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Filter;

class Mock implements FilterInterface
{
    /**
     * array of log events
     *
     * @var array
     */
    public $events = [];

    /**
     * Returns TRUE to accept the message
     *
     * @param array $event event data
     * @return bool
     */
    public function filter(array $event)
    {
        $this->events[] = $event;
        return true;
    }
}
