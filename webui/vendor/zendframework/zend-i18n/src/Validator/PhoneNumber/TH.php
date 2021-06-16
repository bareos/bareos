<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '66',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[2-9]\\d{7,8}|1\\d{3}(?:\\d{6})?)$/',
            'fixed' => '/^(?:2[1-9]|3[2-9]|4[2-5]|5[2-6]|7[3-7])\\d{6}$/',
            'mobile' => '/^[89]\\d{8}$/',
            'tollfree' => '/^1800\\d{6}$/',
            'premium' => '/^1900\\d{6}$/',
            'voip' => '/^60\\d{7}$/',
            'uan' => '/^1\\d{3}$/',
            'emergency' => '/^1(?:669|9[19])$/',
        ],
        'possible' => [
            'general' => '/^(?:\\d{4}|\\d{8,10})$/',
            'fixed' => '/^\\d{8}$/',
            'mobile' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{10}$/',
            'premium' => '/^\\d{10}$/',
            'voip' => '/^\\d{9}$/',
            'uan' => '/^\\d{4}$/',
            'emergency' => '/^\\d{3,4}$/',
        ],
    ],
];
