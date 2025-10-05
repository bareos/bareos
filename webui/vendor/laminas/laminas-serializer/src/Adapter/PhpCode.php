<?php

/**
 * @see       https://github.com/laminas/laminas-serializer for the canonical source repository
 * @copyright https://github.com/laminas/laminas-serializer/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-serializer/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Serializer\Adapter;

use Laminas\Serializer\Exception;
use Laminas\Stdlib\ErrorHandler;

class PhpCode extends AbstractAdapter
{
    /**
     * Serialize PHP using var_export
     *
     * @param  mixed $value
     * @return string
     */
    public function serialize($value)
    {
        return var_export($value, true);
    }

    /**
     * Deserialize PHP string
     *
     * Warning: this uses eval(), and should likely be avoided.
     *
     * @param  string $code
     * @return mixed
     * @throws Exception\RuntimeException on eval error
     */
    public function unserialize($code)
    {
        ErrorHandler::start(E_ALL);
        $ret  = null;
        // This suppression is due to the fact that the ErrorHandler cannot
        // catch syntax errors, and is intentionally left in place.
        $eval = @eval('$ret=' . $code . ';');
        $err  = ErrorHandler::stop();

        if ($eval === false || $err) {
            $msg = 'eval failed';

            // Error handler doesn't catch syntax errors
            if ($eval === false) {
                $lastErr = error_get_last();
                $msg    .= ': ' . $lastErr['message'];
            }

            throw new Exception\RuntimeException($msg, 0, $err);
        }

        return $ret;
    }
}
