<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '262',
    'patterns' => [
        'national' => [
            'general' => '/^[268]\\d{8}$/',
            'fixed' => '/^262\\d{6}$/',
            'mobile' => '/^6(?:9[23]|47)\\d{6}$/',
            'tollfree' => '/^80\\d{7}$/',
            'premium' => '/^89[1-37-9]\\d{6}$/',
            'shared' => '/^8(?:1[019]|2[0156]|84|90)\\d{6}$/',
            'emergency' => '/^1(?:12|[578])$/',
        ],
        'possible' => [
            'general' => '/^\\d{9}$/',
            'mobile' => '/^\\d{9}$/',
            'emergency' => '/^\\d{2,3}$/',
        ],
    ],
];
