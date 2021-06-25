<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '216',
    'patterns' => [
        'national' => [
            'general' => '/^[2-57-9]\\d{7}$/',
            'fixed' => '/^(?:3[012]|7\\d)\\d{6}$/',
            'mobile' => '/^(?:[259]\\d|4[0-2])\\d{6}$/',
            'premium' => '/^8[0128]\\d{6}$/',
            'emergency' => '/^19[078]$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
