<?php

/**
 * @see       https://github.com/laminas/laminas-serializer for the canonical source repository
 * @copyright https://github.com/laminas/laminas-serializer/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-serializer/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Serializer\Adapter;

use Laminas\Serializer\Exception;
use Laminas\Stdlib\ErrorHandler;

class MsgPack extends AbstractAdapter
{
    /**
     * @var string Serialized 0 value
     */
    private static $serialized0 = null;

    /**
     * Constructor
     *
     * @throws Exception\ExtensionNotLoadedException If msgpack extension is not present
     */
    public function __construct($options = null)
    {
        if (!extension_loaded('msgpack')) {
            throw new Exception\ExtensionNotLoadedException(
                'PHP extension "msgpack" is required for this adapter'
            );
        }

        if (static::$serialized0 === null) {
            static::$serialized0 = msgpack_serialize(0);
        }

        parent::__construct($options);
    }

    /**
     * Serialize PHP value to msgpack
     *
     * @param  mixed $value
     * @return string
     * @throws Exception\RuntimeException on msgpack error
     */
    public function serialize($value)
    {
        ErrorHandler::start();
        $ret = msgpack_serialize($value);
        $err = ErrorHandler::stop();

        if ($ret === false) {
            throw new Exception\RuntimeException('Serialization failed', 0, $err);
        }

        return $ret;
    }

    /**
     * Deserialize msgpack string to PHP value
     *
     * @param  string $serialized
     * @return mixed
     * @throws Exception\RuntimeException on msgpack error
     */
    public function unserialize($serialized)
    {
        if ($serialized === static::$serialized0) {
            return 0;
        }

        ErrorHandler::start();
        $ret = msgpack_unserialize($serialized);
        $err = ErrorHandler::stop();

        if ($ret === 0) {
            throw new Exception\RuntimeException('Unserialization failed', 0, $err);
        }

        return $ret;
    }
}
