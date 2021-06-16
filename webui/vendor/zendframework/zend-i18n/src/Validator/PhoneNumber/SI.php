<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '386',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[1-7]\\d{6,7}|[89]\\d{4,7})$/',
            'fixed' => '/^(?:1\\d|[25][2-8]|3[4-8]|4[24-8]|7[3-8])\\d{6}$/',
            'mobile' => '/^(?:[37][01]|4[019]|51|6[48])\\d{6}$/',
            'tollfree' => '/^80\\d{4,6}$/',
            'premium' => '/^(?:90\\d{4,6}|89[1-3]\\d{2,5})$/',
            'voip' => '/^(?:59|8[1-3])\\d{6}$/',
            'emergency' => '/^11[23]$/',
        ],
        'possible' => [
            'general' => '/^\\d{5,8}$/',
            'fixed' => '/^\\d{7,8}$/',
            'mobile' => '/^\\d{8}$/',
            'tollfree' => '/^\\d{6,8}$/',
            'premium' => '/^\\d{5,8}$/',
            'voip' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
