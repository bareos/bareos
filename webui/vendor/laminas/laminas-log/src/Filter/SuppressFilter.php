<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Filter;

use Laminas\Log\Exception;
use Traversable;

class SuppressFilter implements FilterInterface
{
    /**
     * @var bool
     */
    protected $accept = true;

    /**
     * This is a simple boolean filter.
     *
     * @param int|array|Traversable $suppress
     * @throws Exception\InvalidArgumentException
     */
    public function __construct($suppress = false)
    {
        if ($suppress instanceof Traversable) {
            $suppress = iterator_to_array($suppress);
        }
        if (is_array($suppress)) {
            $suppress = isset($suppress['suppress']) ? $suppress['suppress'] : false;
        }
        if (!is_bool($suppress)) {
            throw new Exception\InvalidArgumentException(
                sprintf('Suppress must be a boolean; received "%s"', gettype($suppress))
            );
        }

        $this->suppress($suppress);
    }

    /**
     * This is a simple boolean filter.
     *
     * Call suppress(true) to suppress all log events.
     * Call suppress(false) to accept all log events.
     *
     * @param  bool $suppress Should all log events be suppressed?
     * @return void
     */
    public function suppress($suppress)
    {
        $this->accept = ! (bool) $suppress;
    }

    /**
     * Returns TRUE to accept the message, FALSE to block it.
     *
     * @param array $event event data
     * @return bool accepted?
     */
    public function filter(array $event)
    {
        return $this->accept;
    }
}
