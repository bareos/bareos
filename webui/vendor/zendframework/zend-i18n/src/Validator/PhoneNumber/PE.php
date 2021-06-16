<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '51',
    'patterns' => [
        'national' => [
            'general' => '/^[14-9]\\d{7,8}$/',
            'fixed' => '/^(?:1\\d|4[1-4]|5[1-46]|6[1-7]|7[2-46]|8[2-4])\\d{6}$/',
            'mobile' => '/^9\\d{8}$/',
            'tollfree' => '/^800\\d{5}$/',
            'premium' => '/^805\\d{5}$/',
            'shared' => '/^801\\d{5}$/',
            'personal' => '/^80[24]\\d{5}$/',
            'emergency' => '/^1(?:05|1[67])$/',
        ],
        'possible' => [
            'general' => '/^\\d{6,9}$/',
            'fixed' => '/^\\d{6,8}$/',
            'mobile' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{8}$/',
            'premium' => '/^\\d{8}$/',
            'shared' => '/^\\d{8}$/',
            'personal' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
