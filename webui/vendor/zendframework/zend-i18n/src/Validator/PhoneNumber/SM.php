<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '378',
    'patterns' => [
        'national' => [
            'general' => '/^[05-7]\\d{7,9}$/',
            'fixed' => '/^0549(?:8[0157-9]|9\\d)\\d{4}$/',
            'mobile' => '/^6[16]\\d{6}$/',
            'premium' => '/^7[178]\\d{6}$/',
            'voip' => '/^5[158]\\d{6}$/',
            'emergency' => '/^11[358]$/',
        ],
        'possible' => [
            'general' => '/^\\d{6,10}$/',
            'mobile' => '/^\\d{8}$/',
            'premium' => '/^\\d{8}$/',
            'voip' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
