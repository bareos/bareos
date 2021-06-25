<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\TestAsset\Annotation;

use Zend\Form\Annotation;

/**
 * @Annotation\Name("user")
 * @Annotation\Attributes({"legend":"Register"})
 * @Annotation\Hydrator({"type":"Zend\Hydrator\ClassMethodsHydrator", "options": {"underscoreSeparatedKeys": false}})
 */
class EntityWithHydratorArrayHydratorV3
{
    /**
     * @Annotation\Options({"label":"Username:", "label_attributes": {"class": "label"}})
     */
    public $username;
}
