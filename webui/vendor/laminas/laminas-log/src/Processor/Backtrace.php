<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Processor;

class Backtrace implements ProcessorInterface
{
    /**
     * Maximum stack level of backtrace (PHP > 5.4.0)
     * @var int
     */
    protected $traceLimit = 10;

    /**
     * Classes within this namespace in the stack are ignored
     * @var string
     */
    protected $ignoredNamespace = 'Laminas\\Log';

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
               && false !== strpos($trace[$i]['class'], $this->ignoredNamespace)
        ) {
            $i++;
        }

        $origin = [
            'file'     => isset($trace[$i-1]['file'])   ? $trace[$i-1]['file']   : null,
            'line'     => isset($trace[$i-1]['line'])   ? $trace[$i-1]['line']   : null,
            'class'    => isset($trace[$i]['class'])    ? $trace[$i]['class']    : null,
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
     * Provide backtrace as slim as possible
     *
     * @return array[]
     */
    protected function getBacktrace()
    {
        return debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, $this->traceLimit);
    }
}
