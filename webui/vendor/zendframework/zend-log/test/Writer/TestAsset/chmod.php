<?php
/**
 * @link      http://github.com/zendframework/zend-log for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Log\Writer;

/**
 * chmod() override for emulating warnings in tests.
 *
 * Set `$GLOBALS['chmod_throw_error']` to a truthy value in order to emulate
 * raising an E_WARNING.
 *
 * @param string $filename
 * @param int $mode
 * @return bool
 */
function chmod($filename, $mode)
{
    if (! empty($GLOBALS['chmod_throw_error'])) {
        trigger_error('some_error', E_WARNING);
        return false;
    }

    return \chmod($filename, $mode);
}
