<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Processor;

use Laminas\Console\Console;

class RequestId implements ProcessorInterface
{
    /**
     * Request identifier
     *
     * @var string
     */
    protected $identifier;

    /**
     * Adds an identifier for the request to the log, unless one has already been set.
     *
     * This enables to filter the log for messages belonging to a specific request
     *
     * @param array $event event data
     * @return array event data
     */
    public function process(array $event)
    {
        if (isset($event['extra']['requestId'])) {
            return $event;
        }

        if (!isset($event['extra'])) {
            $event['extra'] = [];
        }

        $event['extra']['requestId'] = $this->getIdentifier();
        return $event;
    }

    /**
     * Provide unique identifier for a request
     *
     * @return string
     */
    protected function getIdentifier()
    {
        if ($this->identifier) {
            return $this->identifier;
        }

        $requestTime = $_SERVER['REQUEST_TIME_FLOAT'];

        if (Console::isConsole()) {
            $this->identifier = md5($requestTime);
            return $this->identifier;
        }

        if (isset($_SERVER['HTTP_X_FORWARDED_FOR'])) {
            $this->identifier = md5($requestTime . $_SERVER['HTTP_X_FORWARDED_FOR']);
            return $this->identifier;
        }

        $this->identifier = md5($requestTime . $_SERVER['REMOTE_ADDR']);
        return $this->identifier;
    }
}
