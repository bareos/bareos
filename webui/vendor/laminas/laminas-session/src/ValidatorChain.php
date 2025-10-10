<?php

/**
 * @see       https://github.com/laminas/laminas-session for the canonical source repository
 * @copyright https://github.com/laminas/laminas-session/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-session/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Session;

use Laminas\EventManager\GlobalEventManager;

/**
 * Polyfill for AbstractValidatorChain.
 *
 * The definitions for EventManagerInterface::attach differ between versions 2
 * and 3 of laminas-eventmanager, which makes it impossible to override the method
 * in a way that is compatible with both.
 *
 * To get around that, we define 2 abstract classes, one targeting each major
 * version of laminas-eventmanager, and each defining attach() per the EM version
 * they target.
 *
 * This conditional below then aliases the appropriate one to `AbstractValidatorChain`,
 * based on which version of the EM is present. Since the `GlobalEventManager`
 * is only present in v2, we can use that as our test.
 */
if (class_exists(GlobalEventManager::class)) {
    class_alias(Validator\AbstractValidatorChainEM2::class, AbstractValidatorChain::class);
} else {
    class_alias(Validator\AbstractValidatorChainEM3::class, AbstractValidatorChain::class);
}

/**
 * Validator chain implementation.
 *
 * Extends the laminas-eventmanager-version-specific base class implementation
 * as polyfilled above.
 */
class ValidatorChain extends AbstractValidatorChain
{
}
