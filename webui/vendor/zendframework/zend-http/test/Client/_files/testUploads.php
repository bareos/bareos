<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

if (! empty($_FILES)) {
    foreach ($_FILES as $name => $file) {
        if (is_array($file['name'])) {
            foreach ($file['name'] as $k => $v) {
                echo "$name $v {$file['type'][$k]} {$file['size'][$k]}\n";
            }
        } else {
            echo "$name {$file['name']} {$file['type']} {$file['size']}\n";
        }
    }
}
