<?php

/**
 * @see       https://github.com/laminas/laminas-i18n for the canonical source repository
 * @copyright https://github.com/laminas/laminas-i18n/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '967',
    'patterns' => [
        'national' => [
            'general' => '/^[1-7]\\d{6,8}$/',
            'fixed' => '/^(?:1(?:7\\d|[2-68])|2[2-68]|3[2358]|4[2-58]|5[2-6]|6[3-58]|7[24-68])\\d{5}$/',
            'mobile' => '/^7[0137]\\d{7}$/',
            'emergency' => '/^19[1459]$/',
        ],
        'possible' => [
            'general' => '/^\\d{6,9}$/',
            'fixed' => '/^\\d{6,8}$/',
            'mobile' => '/^\\d{9}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
