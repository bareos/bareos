<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '63',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[2-9]\\d{7,9}|1800\\d{7,9})$/',
            'fixed' => '/^(?:2|3[2-68]|4[2-9]|5[2-6]|6[2-58]|7[24578]|8[2-8])\\d{7}$/',
            'mobile' => '/^9(?:0[5-9]|1[025-9]|2[0-36-9]|3[02-9]|4[236-9]|7[349]|89|9[49])\\d{7}$/',
            'tollfree' => '/^1800\\d{7,9}$/',
            'emergency' => '/^(?:11[27]|911)$/',
        ],
        'possible' => [
            'general' => '/^\\d{7,13}$/',
            'fixed' => '/^\\d{7,9}$/',
            'mobile' => '/^\\d{10}$/',
            'tollfree' => '/^\\d{11,13}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
