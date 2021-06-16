<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '377',
    'patterns' => [
        'national' => [
            'general' => '/^[4689]\\d{7,8}$/',
            'fixed' => '/^9[2-47-9]\\d{6}$/',
            'mobile' => '/^(?:6\\d{8}|4\\d{7})$/',
            'tollfree' => '/^(?:8\\d|90)\\d{6}$/',
            'emergency' => '/^1(?:12|[578])$/',
        ],
        'possible' => [
            'general' => '/^\\d{8,9}$/',
            'fixed' => '/^\\d{8}$/',
            'tollfree' => '/^\\d{8}$/',
            'emergency' => '/^\\d{2,3}$/',
        ],
    ],
];
