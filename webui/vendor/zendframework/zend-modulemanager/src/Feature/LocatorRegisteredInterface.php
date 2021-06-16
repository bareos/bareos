<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace Zend\ModuleManager\Feature;

/**
 * LocatorRegistered
 *
 * By implementing this interface in a Module class, the instance of the Module
 * class will be automatically injected into any DI-configured object which has
 * a constructor or setter parameter which is type hinted with the Module class
 * name. Implementing this interface obviously does not require adding any
 * methods to your class.
 */
interface LocatorRegisteredInterface
{
}
