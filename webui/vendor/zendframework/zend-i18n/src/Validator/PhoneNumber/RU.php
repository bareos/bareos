<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '7',
    'patterns' => [
        'national' => [
            'general' => '/^[3489]\\d{9}$/',
            'fixed' => '/^(?:3(?:0[12]|4[1-35-79]|5[1-3]|8[1-58]|9[0145])|4(?:01|1[1356]|2[13467]|7[1-5]|8[1-7]|9[1-689])|8(?:1[1-8]|2[01]|3[13-6]|4[0-8]|5[15]|6[1-35-7]|7[1-37-9]))\\d{7}$/',
            'mobile' => '/^9\\d{9}$/',
            'tollfree' => '/^80[04]\\d{7}$/',
            'premium' => '/^80[39]\\d{7}$/',
            'emergency' => '/^(?:0[123]|112)$/',
        ],
        'possible' => [
            'general' => '/^\\d{10}$/',
            'emergency' => '/^\\d{2,3}$/',
        ],
    ],
];
