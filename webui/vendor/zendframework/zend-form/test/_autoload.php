<?php
/**
 * @see       https://github.com/zendframework/zend-hydrator for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form;

use Zend\Hydrator\HydratorPluginManagerInterface;

// @codingStandardsIgnoreStart
if (interface_exists(HydratorPluginManagerInterface::class)) {
    class_alias(TestAsset\Annotation\ComplexEntityHydratorV3::class, TestAsset\Annotation\ComplexEntity::class, true);
    class_alias(TestAsset\Annotation\EntityUsingInstancePropertyHydratorV3::class, TestAsset\Annotation\EntityUsingInstanceProperty::class, true);
    class_alias(TestAsset\Annotation\EntityUsingObjectPropertyHydratorV3::class, TestAsset\Annotation\EntityUsingObjectProperty::class, true);
    class_alias(TestAsset\Annotation\EntityWithHydratorArrayHydratorV3::class, TestAsset\Annotation\EntityWithHydratorArray::class, true);
    class_alias(TestAsset\HydratorAwareModelHydratorV3::class, TestAsset\HydratorAwareModel::class, true);
    class_alias(TestAsset\HydratorStrategyHydratorV3::class, TestAsset\HydratorStrategy::class, true);
} else {
    class_alias(TestAsset\Annotation\ComplexEntityHydratorV2::class, TestAsset\Annotation\ComplexEntity::class, true);
    class_alias(TestAsset\Annotation\EntityUsingInstancePropertyHydratorV2::class, TestAsset\Annotation\EntityUsingInstanceProperty::class, true);
    class_alias(TestAsset\Annotation\EntityUsingObjectPropertyHydratorV2::class, TestAsset\Annotation\EntityUsingObjectProperty::class, true);
    class_alias(TestAsset\Annotation\EntityWithHydratorArrayHydratorV2::class, TestAsset\Annotation\EntityWithHydratorArray::class, true);
    class_alias(TestAsset\HydratorAwareModelHydratorV2::class, TestAsset\HydratorAwareModel::class, true);
    class_alias(TestAsset\HydratorStrategyHydratorV2::class, TestAsset\HydratorStrategy::class, true);
}
// @codingStandardsIgnoreEnd
