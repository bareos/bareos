<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log;

use Laminas\ServiceManager\AbstractPluginManager;

/**
 * Plugin manager for log writers.
 */
class WriterPluginManager extends AbstractPluginManager
{
    protected $aliases = [
        'null'                 => 'noop',
        'Laminas\Log\Writer\Null' => 'noop',

        // Legacy Zend Framework aliases
        'Zend\Log\Writer\Null' => 'noop',
    ];

    /**
     * Default set of writers
     *
     * @var array
     */
    protected $invokableClasses = [
        'chromephp'      => 'Laminas\Log\Writer\ChromePhp',
        'db'             => 'Laminas\Log\Writer\Db',
        'fingerscrossed' => 'Laminas\Log\Writer\FingersCrossed',
        'firephp'        => 'Laminas\Log\Writer\FirePhp',
        'mail'           => 'Laminas\Log\Writer\Mail',
        'mock'           => 'Laminas\Log\Writer\Mock',
        'noop'           => 'Laminas\Log\Writer\Noop',
        'psr'            => 'Laminas\Log\Writer\Psr',
        'stream'         => 'Laminas\Log\Writer\Stream',
        'syslog'         => 'Laminas\Log\Writer\Syslog',
        'zendmonitor'    => 'Laminas\Log\Writer\ZendMonitor',
    ];

    /**
     * Allow many writers of the same type
     *
     * @var bool
     */
    protected $shareByDefault = false;

    /**
     * Validate the plugin
     *
     * Checks that the writer loaded is an instance of Writer\WriterInterface.
     *
     * @param  mixed $plugin
     * @return void
     * @throws Exception\InvalidArgumentException if invalid
     */
    public function validatePlugin($plugin)
    {
        if ($plugin instanceof Writer\WriterInterface) {
            // we're okay
            return;
        }

        throw new Exception\InvalidArgumentException(sprintf(
            'Plugin of type %s is invalid; must implement %s\Writer\WriterInterface',
            (is_object($plugin) ? get_class($plugin) : gettype($plugin)),
            __NAMESPACE__
        ));
    }
}
