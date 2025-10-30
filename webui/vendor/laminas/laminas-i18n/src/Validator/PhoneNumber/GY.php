<?php

/**
 * @see       https://github.com/laminas/laminas-i18n for the canonical source repository
 * @copyright https://github.com/laminas/laminas-i18n/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '592',
    'patterns' => [
        'national' => [
            'general' => '/^[2-4679]\\d{6}$/',
            'fixed' => '/^(?:2(?:1[6-9]|2[0-35-9]|3[1-4]|5[3-9]|6\\d|7[0-24-79])|3(?:2[25-9]|3\\d)|4(?:4[0-24]|5[56])|77[1-57])\\d{4}$/',
            'mobile' => '/^6\\d{6}$/',
            'tollfree' => '/^(?:289|862)\\d{4}$/',
            'premium' => '/^9008\\d{3}$/',
            'shortcode' => '/^0(?:02|171|444|7[67]7|801|9(?:0[78]|[2-47]))$/',
            'emergency' => '/^91[123]$/',
        ],
        'possible' => [
            'general' => '/^\\d{7}$/',
            'shortcode' => '/^\\d{3,4}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
