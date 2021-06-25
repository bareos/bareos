<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Pattern;

use PHPUnit\Framework\TestCase;
use Zend\ServiceManager\ServiceManager;
use Zend\Cache\PatternPluginManager;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Pattern\PatternOptions<extended>
 */
abstract class CommonPatternTest extends TestCase
{
    // @codingStandardsIgnoreStart
    /**
     * @var \Zend\Cache\Pattern\PatternInterface
     */
    protected $_pattern;
    // @codingStandardsIgnoreEnd

    public function setUp()
    {
        $this->assertInstanceOf(
            'Zend\Cache\Pattern\PatternInterface',
            $this->_pattern,
            'Internal pattern instance is needed for tests'
        );
    }

    public function tearDown()
    {
        unset($this->_pattern);
    }

    /**
     * A data provider for common pattern names
     */
    abstract public function getCommonPatternNamesProvider();

    /**
     * @dataProvider getCommonPatternNamesProvider
     */
    public function testPatternPluginManagerWithCommonNames($commonPatternName)
    {
        $pluginManager = new PatternPluginManager(new ServiceManager);
        $this->assertTrue(
            $pluginManager->has($commonPatternName),
            "Pattern name '{$commonPatternName}' not found in PatternPluginManager"
        );
    }

    public function testOptionNamesValid()
    {
        $options = $this->_pattern->getOptions();
        $this->assertInstanceOf('Zend\Cache\Pattern\PatternOptions', $options);
    }

    public function testOptionsGetAndSetDefault()
    {
        $options = $this->_pattern->getOptions();
        $this->_pattern->setOptions($options);
        $this->assertSame($options, $this->_pattern->getOptions());
    }
}
