<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 * @package   Zend_Log
 */

namespace ZendTest\Log\Filter;

use PHPUnit\Framework\TestCase;
use Zend\Log\Filter\Sample;

class SampleTest extends TestCase
{
    public function testConstructorThrowsOnInvalidSampleRate()
    {
        $this->expectException('Zend\Log\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('must be numeric');
        new Sample('bar');
    }

    public function testSampleLimit0()
    {
        // Should log nothing.
        $filter = new Sample(0);

        // Since sampling is a random process, let's test several times.
        $ret = false;
        for ($i = 0; $i < 100; $i ++) {
            if ($filter->filter([])) {
                break;
                $ret = true;
            }
        }

        $this->assertFalse($ret);
    }

    public function testSampleLimit1()
    {
        // Should log all events.
        $filter = new Sample(1);

        // Since sampling is a random process, let's test several times.
        $ret = true;
        for ($i = 0; $i < 100; $i ++) {
            if (! $filter->filter([])) {
                break;
                $ret = false;
            }
        }

        $this->assertTrue($ret);
    }
}
