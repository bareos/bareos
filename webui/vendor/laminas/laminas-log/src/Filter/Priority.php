<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Filter;

use Laminas\Log\Exception;
use Traversable;

class Priority implements FilterInterface
{
    /**
     * @var int
     */
    protected $priority;

    /**
     * @var string
     */
    protected $operator;

    /**
     * Filter logging by $priority. By default, it will accept any log
     * event whose priority value is less than or equal to $priority.
     *
     * @param  int|array|Traversable $priority Priority
     * @param  string $operator Comparison operator
     * @return Priority
     * @throws Exception\InvalidArgumentException
     */
    public function __construct($priority, $operator = null)
    {
        if ($priority instanceof Traversable) {
            $priority = iterator_to_array($priority);
        }
        if (is_array($priority)) {
            $operator = isset($priority['operator']) ? $priority['operator'] : null;
            $priority = isset($priority['priority']) ? $priority['priority'] : null;
        }
        if (!is_int($priority) && !ctype_digit($priority)) {
            throw new Exception\InvalidArgumentException(sprintf(
                'Priority must be a number, received "%s"',
                gettype($priority)
            ));
        }

        $this->priority = (int) $priority;
        $this->operator = $operator === null ? '<=' : $operator;
    }

    /**
     * Returns TRUE to accept the message, FALSE to block it.
     *
     * @param array $event event data
     * @return bool accepted?
     */
    public function filter(array $event)
    {
        return version_compare($event['priority'], $this->priority, $this->operator);
    }
}
