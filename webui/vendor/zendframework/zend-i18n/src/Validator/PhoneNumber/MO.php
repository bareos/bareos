<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '853',
    'patterns' => [
        'national' => [
            'general' => '/^[268]\\d{7}$/',
            'fixed' => '/^(?:28[2-57-9]|8[2-57-9]\\d)\\d{5}$/',
            'mobile' => '/^6[2356]\\d{6}$/',
            'emergency' => '/^999$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
