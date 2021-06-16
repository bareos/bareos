<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '290',
    'patterns' => [
        'national' => [
            'general' => '/^[2-9]\\d{3}$/',
            'fixed' => '/^(?:[2-468]\\d|7[01])\\d{2}$/',
            'premium' => '/^(?:[59]\\d|7[2-9])\\d{2}$/',
            'shortcode' => '/^1\\d{2,3}$/',
            'emergency' => '/^9(?:11|99)$/',
        ],
        'possible' => [
            'general' => '/^\\d{4}$/',
            'shortcode' => '/^\\d{3,4}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
