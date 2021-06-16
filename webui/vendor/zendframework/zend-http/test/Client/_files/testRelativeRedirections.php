<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

if (! isset($_GET['redirect'])) {
    $_GET['redirect'] = null;
}

switch ($_GET['redirect']) {
    case 'abpath':
        header("Location: /path/to/fake/file.ext?redirect=abpath");
        break;

    case 'relpath':
        header("Location: path/to/fake/file.ext?redirect=relpath");
        break;

    default:
        echo "Redirections done.";
        break;
}
