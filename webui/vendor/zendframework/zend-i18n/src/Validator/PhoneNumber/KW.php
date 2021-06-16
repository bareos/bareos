<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '965',
    'patterns' => [
        'national' => [
            'general' => '/^[12569]\\d{6,7}$/',
            'fixed' => '/^(?:18\\d|2(?:[23]\\d{2}|4(?:[1-35-9]\\d|44)|5(?:0[034]|[2-46]\\d|5[1-3]|7[1-7])))\\d{4}$/',
            'mobile' => '/^(?:5(?:11|[05]\\d)|6(?:0[034679]|5[015-9]|6\\d|7[067]|9[069])|9(?:0[09]|4[049]|6[69]|[79]\\d))\\d{5}$/',
            'shortcode' => '/^1(?:[02-9]\\d|1[013-9])$/',
            'emergency' => '/^112$/',
        ],
        'possible' => [
            'general' => '/^\\d{7,8}$/',
            'fixed' => '/^\\d{7,8}$/',
            'mobile' => '/^\\d{8}$/',
            'shortcode' => '/^\\d{3}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
