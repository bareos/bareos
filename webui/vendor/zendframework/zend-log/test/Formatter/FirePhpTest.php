<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright  Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license    http://framework.zend.com/license/new-bsd     New BSD License
 */

namespace ZendTest\Log\Formatter;

use PHPUnit\Framework\TestCase;
use Zend\Log\Formatter\FirePhp;

class FirePhpTest extends TestCase
{
    public function testFormatWithExtraData()
    {
        $fields = [ 'message' => 'foo',
                'extra' => new \stdClass() ];

        $f = new FirePhp();
        list($line, $label) = $f->format($fields);

        $this->assertContains($fields['message'], $label);
        $this->assertEquals($fields['extra'], $line);
    }

    public function testFormatWithoutExtra()
    {
        $fields = [ 'message' => 'foo' ];

        $f = new FirePhp();
        list($line, $label) = $f->format($fields);

        $this->assertContains($fields['message'], $line);
        $this->assertNull($label);
    }

    public function testFormatWithEmptyExtra()
    {
        $fields = [ 'message' => 'foo',
                'extra' => [] ];

        $f = new FirePhp();
        list($line, $label) = $f->format($fields);

        $this->assertContains($fields['message'], $line);
        $this->assertNull($label);
    }

    public function testSetDateTimeFormatDoesNothing()
    {
        $formatter = new FirePhp();

        $this->assertEquals('', $formatter->getDateTimeFormat());
        $this->assertSame($formatter, $formatter->setDateTimeFormat('r'));
        $this->assertEquals('', $formatter->getDateTimeFormat());
    }
}
