<?php

/**
 * @see       https://github.com/laminas/laminas-serializer for the canonical source repository
 * @copyright https://github.com/laminas/laminas-serializer/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-serializer/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Serializer\Adapter;

use Laminas\Json\Json as LaminasJson;
use Laminas\Serializer\Exception;

class Json extends AbstractAdapter
{
    /**
     * @var JsonOptions
     */
    protected $options = null;

    /**
     * Set options
     *
     * @param  array|\Traversable|JsonOptions $options
     * @return Json
     */
    public function setOptions($options)
    {
        if (!$options instanceof JsonOptions) {
            $options = new JsonOptions($options);
        }

        $this->options = $options;
        return $this;
    }

    /**
     * Get options
     *
     * @return JsonOptions
     */
    public function getOptions()
    {
        if ($this->options === null) {
            $this->options = new JsonOptions();
        }

        return $this->options;
    }

    /**
     * Serialize PHP value to JSON
     *
     * @param  mixed $value
     * @return string
     * @throws Exception\InvalidArgumentException
     * @throws Exception\RuntimeException
     */
    public function serialize($value)
    {
        $options    = $this->getOptions();
        $cycleCheck = $options->getCycleCheck();
        $opts = [
            'enableJsonExprFinder' => $options->getEnableJsonExprFinder(),
            'objectDecodeType'     => $options->getObjectDecodeType(),
        ];

        try {
            return LaminasJson::encode($value, $cycleCheck, $opts);
        } catch (\InvalidArgumentException $e) {
            throw new Exception\InvalidArgumentException('Serialization failed: ' . $e->getMessage(), 0, $e);
        } catch (\Exception $e) {
            throw new Exception\RuntimeException('Serialization failed: ' . $e->getMessage(), 0, $e);
        }
    }

    /**
     * Deserialize JSON to PHP value
     *
     * @param  string $json
     * @return mixed
     * @throws Exception\InvalidArgumentException
     * @throws Exception\RuntimeException
     */
    public function unserialize($json)
    {
        try {
            $ret = LaminasJson::decode($json, $this->getOptions()->getObjectDecodeType());
        } catch (\InvalidArgumentException $e) {
            throw new Exception\InvalidArgumentException('Unserialization failed: ' . $e->getMessage(), 0, $e);
        } catch (\Exception $e) {
            throw new Exception\RuntimeException('Unserialization failed: ' . $e->getMessage(), 0, $e);
        }

        return $ret;
    }
}
