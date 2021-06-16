<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '45',
    'patterns' => [
        'national' => [
            'general' => '/^[2-9]\\d{7}$/',
            'fixed' => '/^(?:[2-7]\\d|8[126-9]|9[126-9])\\d{6}$/',
            'mobile' => '/^(?:[2-7]\\d|8[126-9]|9[126-9])\\d{6}$/',
            'tollfree' => '/^80\\d{6}$/',
            'premium' => '/^90\\d{6}$/',
            'emergency' => '/^112$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
