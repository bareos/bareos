<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '269',
    'patterns' => [
        'national' => [
            'general' => '/^[379]\\d{6}$/',
            'fixed' => '/^7(?:6[0-37-9]|7[0-57-9])\\d{4}$/',
            'mobile' => '/^3[234]\\d{5}$/',
            'premium' => '/^(?:39[01]|9[01]0)\\d{4}$/',
            'emergency' => '/^1[78]$/',
        ],
        'possible' => [
            'general' => '/^\\d{7}$/',
            'emergency' => '/^\\d{2}$/',
        ],
    ],
];
