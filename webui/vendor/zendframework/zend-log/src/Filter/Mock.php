<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zend-log for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Log\Filter;

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
