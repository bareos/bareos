<?php

/**
 * @see       https://github.com/laminas/laminas-json for the canonical source repository
 * @copyright https://github.com/laminas/laminas-json/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-json/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Json\Server\Exception;

use Laminas\Json\Exception;

/**
 * Thrown by Laminas\Json\Server\Client when a JSON-RPC fault response is returned.
 */
class ErrorException extends Exception\BadMethodCallException implements
    ExceptionInterface
{
}
