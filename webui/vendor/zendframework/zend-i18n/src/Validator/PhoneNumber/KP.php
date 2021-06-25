<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '850',
    'patterns' => [
        'national' => [
            'general' => '/^(?:1\\d{9}|[28]\\d{7})$/',
            'fixed' => '/^(?:2\\d{7}|85\\d{6})$/',
            'mobile' => '/^19[123]\\d{7}$/',
        ],
        'possible' => [
            'general' => '/^(?:\\d{6,8}|\\d{10})$/',
            'fixed' => '/^\\d{6,8}$/',
            'mobile' => '/^\\d{10}$/',
        ],
    ],
];
