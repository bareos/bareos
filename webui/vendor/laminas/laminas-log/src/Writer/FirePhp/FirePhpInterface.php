<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Writer\FirePhp;

interface FirePhpInterface
{
    /**
     * Determine whether or not FirePHP is enabled
     *
     * @return bool
     */
    public function getEnabled();

    /**
     * Log an error message
     *
     * @param string $line
     */
    public function error($line);

    /**
     * Log a warning
     *
     * @param string $line
     */
    public function warn($line);

    /**
     * Log informational message
     *
     * @param string $line
     */
    public function info($line);

    /**
     * Log a trace
     *
     * @param string $line
     */
    public function trace($line);

    /**
     * Log a message
     *
     * @param string $line
     */
    public function log($line);
}
