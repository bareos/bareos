<?php
/**
 * @see       https://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-cache/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Cache\Psr\SimpleCache\TestAsset;

use Zend\Cache\Storage\Adapter;
use Zend\Cache\Storage\Capabilities;

class TtlStorage extends Adapter\AbstractAdapter
{
    /** @var array */
    private $data = [];

    /** @var array */
    public $ttl = [];

    protected function internalGetItem(& $normalizedKey, & $success = null, & $casToken = null)
    {
        $success = isset($this->data[$normalizedKey]);

        return $success ? $this->data[$normalizedKey] : null;
    }

    protected function internalSetItem(& $normalizedKey, & $value)
    {
        $this->ttl[$normalizedKey] = $this->getOptions()->getTtl();

        $this->data[$normalizedKey] = $value;
        return true;
    }

    protected function internalRemoveItem(& $normalizedKey)
    {
        unset($this->data[$normalizedKey]);
        return true;
    }

    public function setCapabilities(Capabilities $capabilities)
    {
        $this->capabilities = $capabilities;
    }
}
