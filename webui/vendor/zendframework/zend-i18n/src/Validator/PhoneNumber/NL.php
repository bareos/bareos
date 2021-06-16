<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '31',
    'patterns' => [
        'national' => [
            'general' => '/^(?:1\\d{4,8}|[2-7]\\d{8}|[89]\\d{6,9})$/',
            'fixed' => '/^(?:1[0135-8]|2[02-69]|3[0-68]|4[0135-9]|[57]\\d|8[478])\\d{7}$/',
            'mobile' => '/^6[1-58]\\d{7}$/',
            'pager' => '/^66\\d{7}$/',
            'tollfree' => '/^800\\d{4,7}$/',
            'premium' => '/^90[069]\\d{4,7}$/',
            'voip' => '/^85\\d{7}$/',
            'uan' => '/^140(?:1(?:[035]|[16-8]\\d)|2(?:[0346]|[259]\\d)|3(?:[03568]|[124]\\d)|4(?:[0356]|[17-9]\\d)|5(?:[0358]|[124679]\\d)|7\\d|8[458])$/',
            'shortcode' => '/^18\\d{2}$/',
            'emergency' => '/^(?:112|911)$/',
        ],
        'possible' => [
            'general' => '/^\\d{5,10}$/',
            'fixed' => '/^\\d{9}$/',
            'mobile' => '/^\\d{9}$/',
            'pager' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{7,10}$/',
            'premium' => '/^\\d{7,10}$/',
            'voip' => '/^\\d{9}$/',
            'uan' => '/^\\d{5,6}$/',
            'shortcode' => '/^\\d{4}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
