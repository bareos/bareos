<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log;

use PHPUnit\Framework\TestCase;
use Zend\Log\WriterPluginManager;
use Zend\ServiceManager\ServiceManager;

class WriterPluginManagerTest extends TestCase
{
    /**
     * @var WriterPluginManager
     */
    protected $plugins;

    protected function setUp()
    {
        $this->plugins = new WriterPluginManager(new ServiceManager());
    }

    public function testInvokableClassFirephp()
    {
        $firephp = $this->plugins->get('firephp');
        $this->assertInstanceOf('Zend\Log\Writer\Firephp', $firephp);
    }
}
