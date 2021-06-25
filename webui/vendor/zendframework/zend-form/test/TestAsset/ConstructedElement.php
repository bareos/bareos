<?php
/**
 * @see       https://github.com/zendframework/zend-navigation for the canonical source repository
 * @copyright Copyright (c) 2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Form\Element;

class ConstructedElement extends Element
{
    public $constructedKey;

    /**
     * @param null|int|string $name
     * @param array $options
     */
    public function __construct($name = null, $options = [])
    {
        if (isset($options['constructedKey'])) {
            $this->constructedKey = $options['constructedKey'];
        }
        parent::__construct($name, $options);
    }
}
