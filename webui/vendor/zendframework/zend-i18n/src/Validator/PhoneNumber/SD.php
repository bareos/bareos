<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '249',
    'patterns' => [
        'national' => [
            'general' => '/^[19]\\d{8}$/',
            'fixed' => '/^1(?:[125]\\d|8[3567])\\d{6}$/',
            'mobile' => '/^9[012569]\\d{7}$/',
            'emergency' => '/^999$/',
        ],
        'possible' => [
            'general' => '/^\\d{9}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
