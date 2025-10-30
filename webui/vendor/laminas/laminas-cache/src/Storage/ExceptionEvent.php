<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Storage;

use ArrayObject;
use Exception;

class ExceptionEvent extends PostEvent
{
    /**
     * The exception to be thrown
     *
     * @var Exception
     */
    protected $exception;

    /**
     * Throw the exception or use the result
     *
     * @var bool
     */
    protected $throwException = true;

    /**
     * Constructor
     *
     * Accept a target and its parameters.
     *
     * @param  string $name
     * @param  StorageInterface $storage
     * @param  ArrayObject $params
     * @param  mixed $result
     * @param  Exception $exception
     */
    public function __construct($name, StorageInterface $storage, ArrayObject $params, & $result, Exception $exception)
    {
        parent::__construct($name, $storage, $params, $result);
        $this->setException($exception);
    }

    /**
     * Set the exception to be thrown
     *
     * @param  Exception $exception
     * @return ExceptionEvent
     */
    public function setException(Exception $exception)
    {
        $this->exception = $exception;
        return $this;
    }

    /**
     * Get the exception to be thrown
     *
     * @return Exception
     */
    public function getException()
    {
        return $this->exception;
    }

    /**
     * Throw the exception or use the result
     *
     * @param  bool $flag
     * @return ExceptionEvent
     */
    public function setThrowException($flag)
    {
        $this->throwException = (bool) $flag;
        return $this;
    }

    /**
     * Throw the exception or use the result
     *
     * @return bool
     */
    public function getThrowException()
    {
        return $this->throwException;
    }
}
