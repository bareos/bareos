<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace Zend\Http\Client\Adapter;

/**
 * This is a stub for PHP's `fwrite` function. It
 * allows us to check that a write operation to a
 * socket producing a returned "0 bytes" written
 * is actually valid.
 */
function fwrite($socket, $request)
{
    return 0;
}
