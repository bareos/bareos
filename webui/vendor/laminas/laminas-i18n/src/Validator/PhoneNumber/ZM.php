<?php

/**
 * @see       https://github.com/laminas/laminas-i18n for the canonical source repository
 * @copyright https://github.com/laminas/laminas-i18n/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '260',
    'patterns' => [
        'national' => [
            'general' => '/^[289]\\d{8}$/',
            'fixed' => '/^21[1-8]\\d{6}$/',
            'mobile' => '/^9(?:5[05]|6\\d|7[13-9])\\d{6}$/',
            'tollfree' => '/^800\\d{6}$/',
            'emergency' => '/^(?:112|99[139])$/',
        ],
        'possible' => [
            'general' => '/^\\d{9}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
