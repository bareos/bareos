<?php

/**
 * @see       https://github.com/laminas/laminas-i18n for the canonical source repository
 * @copyright https://github.com/laminas/laminas-i18n/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '682',
    'patterns' => [
        'national' => [
            'general' => '/^[2-57]\\d{4}$/',
            'fixed' => '/^(?:2\\d|3[13-7]|4[1-5])\\d{3}$/',
            'mobile' => '/^(?:5[0-68]|7\\d)\\d{3}$/',
            'emergency' => '/^99[689]$/',
        ],
        'possible' => [
            'general' => '/^\\d{5}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
