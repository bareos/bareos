<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Writer;

use Laminas\Log\Exception;
use Laminas\Log\Formatter;
use Laminas\ServiceManager\AbstractPluginManager;

class FormatterPluginManager extends AbstractPluginManager
{
    /**
     * Default set of formatters
     *
     * @var array
     */
    protected $invokableClasses = [
        'base'             => 'Laminas\Log\Formatter\Base',
        'simple'           => 'Laminas\Log\Formatter\Simple',
        'xml'              => 'Laminas\Log\Formatter\Xml',
        'db'               => 'Laminas\Log\Formatter\Db',
        'errorhandler'     => 'Laminas\Log\Formatter\ErrorHandler',
        'exceptionhandler' => 'Laminas\Log\Formatter\ExceptionHandler',
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
     * Checks that the formatter loaded is an instance of Formatter\FormatterInterface.
     *
     * @param  mixed $plugin
     * @return void
     * @throws Exception\InvalidArgumentException if invalid
     */
    public function validatePlugin($plugin)
    {
        if ($plugin instanceof Formatter\FormatterInterface) {
            // we're okay
            return;
        }

        throw new Exception\InvalidArgumentException(sprintf(
            'Plugin of type %s is invalid; must implement %s\Formatter\FormatterInterface',
            (is_object($plugin) ? get_class($plugin) : gettype($plugin)),
            __NAMESPACE__
        ));
    }
}
