<?php
/**
 * @link      http://github.com/zendframework/zend-navigation for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Navigation\Service\TestAsset;

use Zend\Navigation\Service\AbstractNavigationFactory;

class TestNavigationFactory extends AbstractNavigationFactory
{
    /**
     * @var string
     */
    private $factoryName;

    /**
     * @param string $factoryName
     */
    public function __construct($factoryName = 'test')
    {
        $this->factoryName = $factoryName;
    }

    /**
     * @return string
     */
    protected function getName()
    {
        return $this->factoryName;
    }
}
