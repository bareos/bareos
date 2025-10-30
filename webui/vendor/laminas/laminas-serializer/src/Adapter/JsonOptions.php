<?php

/**
 * @see       https://github.com/laminas/laminas-serializer for the canonical source repository
 * @copyright https://github.com/laminas/laminas-serializer/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-serializer/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Serializer\Adapter;

use Laminas\Json\Json as LaminasJson;
use Laminas\Serializer\Exception;

class JsonOptions extends AdapterOptions
{
    /** @var bool */
    protected $cycleCheck = false;

    /** @var bool */
    protected $enableJsonExprFinder = false;

    /** @var int */
    protected $objectDecodeType = LaminasJson::TYPE_ARRAY;

    /**
     * @param  bool $flag
     * @return JsonOptions
     */
    public function setCycleCheck($flag)
    {
        $this->cycleCheck = (bool) $flag;
        return $this;
    }

    /**
     * @return bool
     */
    public function getCycleCheck()
    {
        return $this->cycleCheck;
    }

    /**
     * @param  bool $flag
     * @return JsonOptions
     */
    public function setEnableJsonExprFinder($flag)
    {
        $this->enableJsonExprFinder = (bool) $flag;
        return $this;
    }

    /**
     * @return bool
     */
    public function getEnableJsonExprFinder()
    {
        return $this->enableJsonExprFinder;
    }

    /**
     * @param  int $type
     * @return JsonOptions
     * @throws Exception\InvalidArgumentException
     */
    public function setObjectDecodeType($type)
    {
        if ($type != LaminasJson::TYPE_ARRAY && $type != LaminasJson::TYPE_OBJECT) {
            throw new Exception\InvalidArgumentException(
                'Unknown decode type: ' . $type
            );
        }

        $this->objectDecodeType = (int) $type;

        return $this;
    }

    /**
     * @return int
     */
    public function getObjectDecodeType()
    {
        return $this->objectDecodeType;
    }
}
