<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log\Processor;

use PHPUnit\Framework\TestCase;
use stdClass;
use Zend\Log\Processor\PsrPlaceholder;

/**
 * @coversDefaultClass \Zend\Log\Processor\PsrPlaceholder
 */
class PsrPlaceholderTest extends TestCase
{
    /**
     * @dataProvider pairsProvider
     * @covers ::process
     */
    public function testReplacement($val, $expected)
    {
        $psrProcessor = new PsrPlaceholder;
        $event = $psrProcessor->process([
            'message' => '{foo}',
            'extra'   => ['foo' => $val]
        ]);
        $this->assertEquals($expected, $event['message']);
    }

    /**
     * Data provider
     *
     * @return array
     */
    public function pairsProvider()
    {
        return [
            'string'     => ['foo', 'foo'],
            'string-int' => ['3', '3'],
            'int'        => [3, '3'],
            'null'       => [null, ''],
            'true'       => [true, '1'],
            'false'      => [false, ''],
            'stdclass'   => [new stdClass, '[object stdClass]'],
            'array'      => [[], '[array]'],
        ];
    }
}
