<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log;

use Laminas\ServiceManager\AbstractPluginManager;

/**
 * Plugin manager for log processors.
 */
class ProcessorPluginManager extends AbstractPluginManager
{
    /**
     * Default set of processors
     *
     * @var array
     */
    protected $invokableClasses = [
        'backtrace'      => 'Laminas\Log\Processor\Backtrace',
        'psrplaceholder' => 'Laminas\Log\Processor\PsrPlaceholder',
        'referenceid'    => 'Laminas\Log\Processor\ReferenceId',
        'requestid'      => 'Laminas\Log\Processor\RequestId',
    ];

    /**
     * Allow many processors of the same type
     *
     * @var bool
     */
    protected $shareByDefault = false;

    /**
     * Validate the plugin
     *
     * Checks that the processor loaded is an instance of Processor\ProcessorInterface.
     *
     * @param  mixed $plugin
     * @return void
     * @throws Exception\InvalidArgumentException if invalid
     */
    public function validatePlugin($plugin)
    {
        if ($plugin instanceof Processor\ProcessorInterface) {
            // we're okay
            return;
        }

        throw new Exception\InvalidArgumentException(sprintf(
            'Plugin of type %s is invalid; must implement %s\Processor\ProcessorInterface',
            (is_object($plugin) ? get_class($plugin) : gettype($plugin)),
            __NAMESPACE__
        ));
    }
}
