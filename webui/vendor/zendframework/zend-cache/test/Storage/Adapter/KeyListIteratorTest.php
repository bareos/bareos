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
use Zend\Cache\Storage\Adapter\KeyListIterator;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\KeyListIterator
 */
class KeyListIteratorTest extends TestCase
{
    public function testCount()
    {
        $keys = ['key1', 'key2', 'key3'];
        $storage = $this->createMock('Zend\Cache\Storage\StorageInterface');
        $iterator = new KeyListIterator($storage, $keys);
        $this->assertEquals(3, $iterator->count());
    }
}
