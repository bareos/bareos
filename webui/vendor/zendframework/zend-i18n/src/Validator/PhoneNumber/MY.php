<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '60',
    'patterns' => [
        'national' => [
            'general' => '/^[13-9]\\d{7,9}$/',
            'fixed' => '/^(?:3[2-9]\\d|[4-9][2-9])\\d{6}$/',
            'mobile' => '/^1(?:1[1-3]\\d{2}|[02-4679][2-9]\\d|8(?:1[23]|[2-9]\\d))\\d{5}$/',
            'tollfree' => '/^1[38]00\\d{6}$/',
            'premium' => '/^1600\\d{6}$/',
            'personal' => '/^1700\\d{6}$/',
            'voip' => '/^154\\d{7}$/',
            'emergency' => '/^(?:112|999)$/',
        ],
        'possible' => [
            'general' => '/^\\d{6,10}$/',
            'fixed' => '/^\\d{6,9}$/',
            'mobile' => '/^\\d{9,10}$/',
            'tollfree' => '/^\\d{10}$/',
            'premium' => '/^\\d{10}$/',
            'personal' => '/^\\d{10}$/',
            'voip' => '/^\\d{10}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
