<?php

/**
 * @see       https://github.com/laminas/laminas-session for the canonical source repository
 * @copyright https://github.com/laminas/laminas-session/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-session/blob/master/LICENSE.md New BSD License
 */
namespace Laminas\Session\Validator;

use Laminas\EventManager\EventManager;
use Laminas\Session\Storage\StorageInterface;

/**
 * Abstract validator chain for validating sessions (for use with laminas-eventmanager v3)
 */
abstract class AbstractValidatorChainEM3 extends EventManager
{
    use ValidatorChainTrait;

    /**
     * Construct the validation chain
     *
     * Retrieves validators from session storage and attaches them.
     *
     * Duplicated in ValidatorChainEM2 to prevent trait collision with parent.
     *
     * @param StorageInterface $storage
     */
    public function __construct(StorageInterface $storage)
    {
        parent::__construct();

        $this->storage = $storage;
        $validators = $storage->getMetadata('_VALID');
        if ($validators) {
            foreach ($validators as $validator => $data) {
                $this->attachValidator('session.validate', [new $validator($data), 'isValid'], 1);
            }
        }
    }

    /**
     * Attach a listener to the session validator chain.
     *
     * @param string $eventName
     * @param callable $callback
     * @param int $priority
     * @return \Laminas\Stdlib\CallbackHandler
     */
    public function attach($eventName, callable $callback, $priority = 1)
    {
        return $this->attachValidator($eventName, $callback, $priority);
    }
}
