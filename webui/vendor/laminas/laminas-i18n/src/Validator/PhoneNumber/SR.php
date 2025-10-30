<?php

/**
 * @see       https://github.com/laminas/laminas-i18n for the canonical source repository
 * @copyright https://github.com/laminas/laminas-i18n/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '597',
    'patterns' => [
        'national' => [
            'general' => '/^[2-8]\\d{5,6}$/',
            'fixed' => '/^(?:2[1-3]|3[0-7]|4\\d|5[2-58]|68\\d)\\d{4}$/',
            'mobile' => '/^(?:7[1-57]|8[1-9])\\d{5}$/',
            'voip' => '/^56\\d{4}$/',
            'shortcode' => '/^1(?:[02-9]\\d|1[0-46-9]|\\d{3})$/',
            'emergency' => '/^115$/',
        ],
        'possible' => [
            'general' => '/^\\d{6,7}$/',
            'mobile' => '/^\\d{7}$/',
            'voip' => '/^\\d{6}$/',
            'shortcode' => '/^\\d{3,4}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
