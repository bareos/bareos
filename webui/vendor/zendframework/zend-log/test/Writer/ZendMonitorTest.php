<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log\Writer;

use PHPUnit\Framework\TestCase;
use Zend\Log\Writer\ZendMonitor;

class ZendMonitorTest extends TestCase
{
    /**
     * @group ZF-10081
     */
    public function testWrite()
    {
        $writer = new ZendMonitor();
        $writer->write([
            'message' => 'my mess',
            'priority' => 1
        ]);
    }

    public function testIsEnabled()
    {
        $writer = new ZendMonitor();
        $this->assertInternalType('boolean', $writer->isEnabled());
    }
}
