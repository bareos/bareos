<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '48',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[1-58]\\d{6,8}|9\\d{8}|[67]\\d{5,8})$/',
            'fixed' => '/^(?:(?:1[2-8]|2[2-59]|3[2-4]|4[1-468]|5[24-689]|6[1-3578]|7[14-6]|8[1-7])\\d{5,7}|77\\d{4,7}|(?:89|9[145])\\d{7})$/',
            'mobile' => '/^(?:5[013]|6[069]|7[2389]|88)\\d{7}$/',
            'pager' => '/^642\\d{3,6}$/',
            'tollfree' => '/^800\\d{6}$/',
            'premium' => '/^70\\d{7}$/',
            'shared' => '/^801\\d{6}$/',
            'voip' => '/^39\\d{7}$/',
            'emergency' => '/^(?:112|99[789])$/',
        ],
        'possible' => [
            'general' => '/^\\d{6,9}$/',
            'mobile' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{9}$/',
            'premium' => '/^\\d{9}$/',
            'shared' => '/^\\d{9}$/',
            'voip' => '/^\\d{9}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
