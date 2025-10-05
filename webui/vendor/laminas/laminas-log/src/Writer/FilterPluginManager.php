<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Writer;

use Laminas\Log\Exception;
use Laminas\Log\Filter;
use Laminas\ServiceManager\AbstractPluginManager;

class FilterPluginManager extends AbstractPluginManager
{
    /**
     * Default set of filters
     *
     * @var array
     */
    protected $invokableClasses = [
        'mock'           => 'Laminas\Log\Filter\Mock',
        'priority'       => 'Laminas\Log\Filter\Priority',
        'regex'          => 'Laminas\Log\Filter\Regex',
        'suppress'       => 'Laminas\Log\Filter\SuppressFilter',
        'suppressfilter' => 'Laminas\Log\Filter\SuppressFilter',
        'validator'      => 'Laminas\Log\Filter\Validator',
    ];

    /**
     * Allow many filters of the same type
     *
     * @var bool
     */
    protected $shareByDefault = false;

    /**
     * Validate the plugin
     *
     * Checks that the filter loaded is an instance of Filter\FilterInterface.
     *
     * @param  mixed $plugin
     * @return void
     * @throws Exception\InvalidArgumentException if invalid
     */
    public function validatePlugin($plugin)
    {
        if ($plugin instanceof Filter\FilterInterface) {
            // we're okay
            return;
        }

        throw new Exception\InvalidArgumentException(sprintf(
            'Plugin of type %s is invalid; must implement %s\Filter\FilterInterface',
            (is_object($plugin) ? get_class($plugin) : gettype($plugin)),
            __NAMESPACE__
        ));
    }
}
