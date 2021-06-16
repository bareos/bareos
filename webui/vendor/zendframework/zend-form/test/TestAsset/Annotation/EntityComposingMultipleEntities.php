<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\TestAsset\Annotation;

use Zend\Form\Annotation;

/**
 * @Annotation\Name("hierarchical")
 */
class EntityComposingMultipleEntities
{
    /**
     * @Annotation\Name("composed")
     * @Annotation\ComposedObject({"target_object":"ZendTest\Form\TestAsset\Annotation\Entity", "is_collection":"true"})
     */
    public $child;
}
