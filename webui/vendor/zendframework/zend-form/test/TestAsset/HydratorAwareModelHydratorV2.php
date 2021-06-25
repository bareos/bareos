<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Hydrator\HydratorAwareInterface;
use Zend\Hydrator\HydratorInterface;
use Zend\Hydrator\ClassMethods;

/**
 * This test asset targest zend-hydrator v1 and v2, and will be aliased to
 * HydratorAwareModel when of those versions is installed.
 */
class HydratorAwareModelHydratorV2 implements HydratorAwareInterface
{
    protected $hydrator = null;

    protected $foo = null;
    protected $bar = null;

    public function setHydrator(HydratorInterface $hydrator)
    {
        $this->hydrator = $hydrator;
    }

    public function getHydrator()
    {
        if (! $this->hydrator) {
            $this->hydrator = new ClassMethods();
        }
        return $this->hydrator;
    }

    public function setFoo($value)
    {
        $this->foo = $value;
        return $this;
    }

    public function setBar($value)
    {
        $this->bar = $value;
        return $this;
    }

    public function getFoo()
    {
        return $this->foo;
    }

    public function getBar()
    {
        return $this->bar;
    }
}
