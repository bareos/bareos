<?php
/**
 * @see       https://github.com/zendframework/zend-view for the canonical source repository
 * @copyright Copyright (c) 2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-view/blob/master/LICENSE.md New BSD License
 */

if (class_exists(PHPUnit_Framework_Error_Deprecated::class)
    && ! class_exists(PHPUnit\Framework\Error\Deprecated::class)) {
    class_alias(PHPUnit_Framework_Error_Deprecated::class, PHPUnit\Framework\Error\Deprecated::class);
}
