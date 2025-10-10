<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log;

/**
 * Logger aware interface
 */
interface LoggerAwareInterface
{
    /**
     * Set logger instance
     *
     * @param LoggerInterface
     * @return void
     */
    public function setLogger(LoggerInterface $logger);

    /**
     * Get logger instance. Currently commented out as this would possibly break
     * existing implementations.
     *
     * @return null|LoggerInterface
     */
    // public function getLogger();
}
