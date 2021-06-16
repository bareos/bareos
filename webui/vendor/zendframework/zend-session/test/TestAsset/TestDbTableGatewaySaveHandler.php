<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Session\TestAsset;

use Zend\Session\SaveHandler\DbTableGateway;

class TestDbTableGatewaySaveHandler extends DbTableGateway
{
    protected $numReadCalls = 0;

    protected $numDestroyCalls = 0;

    public function getNumReadCalls()
    {
        return $this->numReadCalls;
    }

    public function getNumDestroyCalls()
    {
        return $this->numDestroyCalls;
    }

    public function read($id, $destroyExpired = true)
    {
        $this->numReadCalls++;
        return parent::read($id, $destroyExpired);
    }

    public function destroy($id)
    {
        $this->numDestroyCalls++;
        return parent::destroy($id);
    }
}
