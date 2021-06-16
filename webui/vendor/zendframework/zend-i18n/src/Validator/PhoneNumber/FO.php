<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '298',
    'patterns' => [
        'national' => [
            'general' => '/^[2-9]\\d{5}$/',
            'fixed' => '/^(?:20|[3-4]\\d|8[19])\\d{4}$/',
            'mobile' => '/^(?:2[1-9]|5\\d|7[1-79])\\d{4}$/',
            'tollfree' => '/^80[257-9]\\d{3}$/',
            'premium' => '/^90(?:[1345][15-7]|2[125-7]|99)\\d{2}$/',
            'voip' => '/^(?:6[0-36]|88)\\d{4}$/',
            'shortcode' => '/^1(?:1[48]|4[124]\\d|71\\d|8[7-9]\\d)$/',
            'emergency' => '/^112$/',
        ],
        'possible' => [
            'general' => '/^\\d{6}$/',
            'shortcode' => '/^\\d{3,4}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
