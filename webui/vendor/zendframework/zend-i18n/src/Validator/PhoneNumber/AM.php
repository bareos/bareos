<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '374',
    'patterns' => [
        'national' => [
            'general' => '/^[1-9]\d{7}$/',
            'fixed' => '/^(?:10\d|2(?:2[2-46]|3[1-8]|4[2-69]|5[2-7]|6[1-9]|8[1-7])|3[12]2|47\d)\d{5}$/',
            'mobile' => '/^(?:55|77|9[1-9])\d{6}$/',
            'tollfree' => '/^800\d{5}$/',
            'premium' => '/^90[016]\d{5}$/',
            'shared' => '/^80[1-4]\d{5}$/',
            'voip' => '/^60[2-6]\d{5}$/',
            'shortcode' => '/^(?:8[1-7]\d{2}|1(?:0[04-9]|[1-9]\d))$/',
            'emergency' => '/^10[123]$/',
        ],
        'possible' => [
            'general' => '/^\d{5,8}$/',
            'mobile' => '/^\d{8}$/',
            'tollfree' => '/^\d{8}$/',
            'premium' => '/^\d{8}$/',
            'shared' => '/^\d{8}$/',
            'voip' => '/^\d{8}$/',
            'shortcode' => '/^\d{3,4}$/',
            'emergency' => '/^\d{3}$/',
        ],
    ],
];
