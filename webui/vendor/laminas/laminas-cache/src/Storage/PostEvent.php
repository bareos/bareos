<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Storage;

use ArrayObject;

class PostEvent extends Event
{
    /**
     * The result/return value
     *
     * @var mixed
     */
    protected $result;

    /**
     * Constructor
     *
     * Accept a target and its parameters.
     *
     * @param  string           $name
     * @param  StorageInterface $storage
     * @param  ArrayObject      $params
     * @param  mixed            $result
     */
    public function __construct($name, StorageInterface $storage, ArrayObject $params, & $result)
    {
        parent::__construct($name, $storage, $params);
        $this->setResult($result);
    }

    /**
     * Set the result/return value
     *
     * @param  mixed $value
     * @return PostEvent
     */
    public function setResult(& $value)
    {
        $this->result = & $value;
        return $this;
    }

    /**
     * Get the result/return value
     *
     * @return mixed
     */
    public function & getResult()
    {
        return $this->result;
    }
}
