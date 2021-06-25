<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Hydrator\Strategy\DefaultStrategy;

/**
 * This class targets zend-hydrator v3, and will be aliased to
 * HydratorStrategy when that versions is detected.
 */
class HydratorStrategyHydratorV3 extends DefaultStrategy
{
    /**
     * A simulated storage device which is just an array with Car objects.
     *
     * @var array
     */
    private $simulatedStorageDevice;

    public function __construct()
    {
        $this->simulatedStorageDevice = [];
        $this->simulatedStorageDevice[] = new HydratorStrategyEntityB(111, 'AAA');
        $this->simulatedStorageDevice[] = new HydratorStrategyEntityB(222, 'BBB');
        $this->simulatedStorageDevice[] = new HydratorStrategyEntityB(333, 'CCC');
    }

    public function extract($value, ?object $object = null) : array
    {
        $result = [];
        foreach ($value as $instance) {
            $result[] = $instance->getField1();
        }
        return $result;
    }

    public function hydrate($value, ?array $data = null) : object
    {
        $result = $value;
        if (is_array($value)) {
            $result = [];
            foreach ($value as $field1) {
                $result[] = $this->findEntity($field1);
            }
        }
        return (object) $result;
    }

    private function findEntity($field1)
    {
        $result = null;
        foreach ($this->simulatedStorageDevice as $entity) {
            if ($entity->getField1() == $field1) {
                $result = $entity;
                break;
            }
        }
        return $result;
    }
}
