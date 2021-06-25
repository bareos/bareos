<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zend-log for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Log\Processor;

class Backtrace implements ProcessorInterface
{
    /**
     * Maximum stack level of backtrace (PHP > 5.4.0)
     * @var int
     */
    protected $traceLimit = 10;

    /**
     * Classes within these namespaces in the stack are ignored
     * @var array
     */
    protected $ignoredNamespaces = ['Zend\\Log'];

    /**
     * Set options for a backtrace processor. Accepted options are:
     * - ignoredNamespaces: array of namespaces to be excluded from the logged backtrace
     *
     * @param array|null $options
     */
    public function __construct(array $options = null)
    {
        if (! empty($options['ignoredNamespaces'])) {
            $this->ignoredNamespaces = array_merge($this->ignoredNamespaces, (array) $options['ignoredNamespaces']);
        }
    }

    /**
     * Adds the origin of the log() call to the event extras
     *
     * @param array $event event data
     * @return array event data
    */
    public function process(array $event)
    {
        $trace = $this->getBacktrace();

        array_shift($trace); // ignore $this->getBacktrace();
        array_shift($trace); // ignore $this->process()

        $i = 0;
        while (isset($trace[$i]['class'])
               && $this->shouldIgnoreFrame($trace[$i]['class'])
        ) {
            $i++;
        }

        $origin = [
            'file'     => isset($trace[$i - 1]['file']) ? $trace[$i - 1]['file'] : null,
            'line'     => isset($trace[$i - 1]['line']) ? $trace[$i - 1]['line'] : null,
            'class'    => isset($trace[$i]['class']) ? $trace[$i]['class'] : null,
            'function' => isset($trace[$i]['function']) ? $trace[$i]['function'] : null,
        ];

        $extra = $origin;
        if (isset($event['extra'])) {
            $extra = array_merge($origin, $event['extra']);
        }
        $event['extra'] = $extra;

        return $event;
    }

    /**
     * Get all ignored namespaces
     *
     * @return array
     */
    public function getIgnoredNamespaces()
    {
        return $this->ignoredNamespaces;
    }

    /**
     * Provide backtrace as slim as possible
     *
     * @return array[]
     */
    protected function getBacktrace()
    {
        return debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, $this->traceLimit);
    }

    /**
     * Determine whether the current frame in the backtrace should be ignored based on the class name
     *
     * @param string $class
     * @return bool
     */
    protected function shouldIgnoreFrame($class)
    {
        foreach ($this->ignoredNamespaces as $ignoredNamespace) {
            if (false !== strpos($class, $ignoredNamespace)) {
                return true;
            }
        }

        return false;
    }
}
