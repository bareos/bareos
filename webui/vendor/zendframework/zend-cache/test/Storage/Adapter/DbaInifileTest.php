<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use PHPUnit\Framework\TestCase;
use Zend\Cache\Storage\Adapter\Dba;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\Dba<extended>
 */
class DbaInifileTest extends TestCase
{
    public function testSpecifyingInifileHandlerRaisesException()
    {
        if (! extension_loaded('dba')) {
            $this->markTestSkipped("Missing ext/dba");
        }

        $this->expectException('Zend\Cache\Exception\ExtensionNotLoadedException');
        $this->expectExceptionMessage('inifile');
        $cache = new Dba([
            'pathname' => sys_get_temp_dir() . DIRECTORY_SEPARATOR . uniqid('zfcache_dba_') . '.inifile',
            'handler'  => 'inifile',
        ]);
    }
}
