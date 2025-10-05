<?php

/**
 * @see       https://github.com/laminas/laminas-log for the canonical source repository
 * @copyright https://github.com/laminas/laminas-log/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-log/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Log\Processor;

/**
 * Processes an event message according to PSR-3 rules.
 *
 * This processor replaces `{foo}` with the value from `$extra['foo']`.
 */
class PsrPlaceholder implements ProcessorInterface
{
    /**
     * @param array $event event data
     * @return array event data
     */
    public function process(array $event)
    {
        if (false === strpos($event['message'], '{')) {
            return $event;
        }

        $replacements = [];
        foreach ($event['extra'] as $key => $val) {
            if (is_null($val)
                || is_scalar($val)
                || (is_object($val) && method_exists($val, "__toString"))
            ) {
                $replacements['{'.$key.'}'] = $val;
                continue;
            }

            if (is_object($val)) {
                $replacements['{'.$key.'}'] = '[object '.get_class($val).']';
                continue;
            }

            $replacements['{'.$key.'}'] = '['.gettype($val).']';
        }

        $event['message'] = strtr($event['message'], $replacements);
        return $event;
    }
}
