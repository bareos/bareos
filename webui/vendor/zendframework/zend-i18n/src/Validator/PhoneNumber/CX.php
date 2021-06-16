<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '61',
    'patterns' => [
        'national' => [
            'general' => '/^[1458]\\d{5,9}$/',
            'fixed' => '/^89164\\d{4}$/',
            'mobile' => '/^4(?:[0-2]\\d|3[0-57-9]|4[47-9]|5[0-37-9]|6[6-9]|7[07-9]|8[7-9])\\d{6}$/',
            'tollfree' => '/^1(?:80(?:0\\d{2})?|3(?:00\\d{2})?)\\d{4}$/',
            'premium' => '/^190[0126]\\d{6}$/',
            'personal' => '/^500\\d{6}$/',
            'voip' => '/^550\\d{6}$/',
            'emergency' => '/^(?:000|112)$/',
        ],
        'possible' => [
            'general' => '/^\\d{6,10}$/',
            'fixed' => '/^\\d{8,9}$/',
            'mobile' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{6,10}$/',
            'premium' => '/^\\d{10}$/',
            'personal' => '/^\\d{9}$/',
            'voip' => '/^\\d{9}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
