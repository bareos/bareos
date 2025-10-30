<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Filter;

use Laminas\Log\Exception;

class Sample implements FilterInterface
{
    /**
     * Sample rate [0-1].
     *
     * @var float
     */
    protected $sampleRate;

    /**
     * Filters logging by sample rate.
     *
     * Sample rate must be a float number between 0 and 1 included.
     * If 0.5, only half of the values will be logged.
     * If 0.1 only 1 among 10 values will be logged.
     *
     * @param  float|int $samplerate Sample rate [0-1].
     * @return Priority
     * @throws Exception\InvalidArgumentException
     */
    public function __construct($sampleRate = 1)
    {
        if (! is_numeric($sampleRate)) {
            throw new Exception\InvalidArgumentException(sprintf(
                'Sample rate must be numeric, received "%s"',
                gettype($sampleRate)
            ));
        }

        $this->sampleRate = (float) $sampleRate;
    }

    /**
     * Returns TRUE to accept the message, FALSE to block it.
     *
     * @param  array $event event data
     * @return bool Accepted ?
     */
    public function filter(array $event)
    {
        return (mt_rand() / mt_getrandmax()) <= $this->sampleRate;
    }
}
