<?php

/**
 * @see       https://github.com/laminas/laminas-json for the canonical source repository
 * @copyright https://github.com/laminas/laminas-json/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-json/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Json\Server;

use Laminas\Server\Cache as ServerCache;
use Laminas\Stdlib\ErrorHandler;

/**
 * Laminas\Json\Server\Cache: cache Laminas\Json\Server\Server server definition and SMD
 */
class Cache extends ServerCache
{
    /**
     * Cache a service map description (SMD) to a file
     *
     * Returns true on success, false on failure
     *
     * @param  string $filename
     * @param  \Laminas\Json\Server\Server $server
     * @return bool
     */
    public static function saveSmd($filename, Server $server)
    {
        if (!is_string($filename) || (!file_exists($filename) && !is_writable(dirname($filename)))) {
            return false;
        }

        ErrorHandler::start();
        $test = file_put_contents($filename, $server->getServiceMap()->toJson());
        ErrorHandler::stop();

        if (0 === $test) {
            return false;
        }

        return true;
    }

    /**
     * Retrieve a cached SMD
     *
     * On success, returns the cached SMD (a JSON string); a failure, returns
     * boolean false.
     *
     * @param  string $filename
     * @return string|false
     */
    public static function getSmd($filename)
    {
        if (!is_string($filename) || !file_exists($filename) || !is_readable($filename)) {
            return false;
        }

        ErrorHandler::start();
        $smd = file_get_contents($filename);
        ErrorHandler::stop();

        if (false === $smd) {
            return false;
        }

        return $smd;
    }

    /**
     * Delete a file containing a cached SMD
     *
     * @param  string $filename
     * @return bool
     */
    public static function deleteSmd($filename)
    {
        if (is_string($filename) && file_exists($filename)) {
            unlink($filename);
            return true;
        }

        return false;
    }
}
