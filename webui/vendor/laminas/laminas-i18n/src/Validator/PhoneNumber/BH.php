<?php

/**
 * @see       https://github.com/laminas/laminas-i18n for the canonical source repository
 * @copyright https://github.com/laminas/laminas-i18n/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '973',
    'patterns' => [
        'national' => [
            'general' => '/^[136-9]\\d{7}$/',
            'fixed' => '/^(?:1(?:3[3-6]|6[0156]|7\\d)\\d|6(?:1[16]\\d|6(?:0\\d|3[12]|44)|9(?:69|9[6-9]))|77\\d{2})\\d{4}$/',
            'mobile' => '/^(?:3(?:[23469]\\d|5[35]|77|8[348])\\d|6(?:1[16]\\d|6(?:[06]\\d|3[03-9]|44)|9(?:69|9[6-9]))|77\\d{2})\\d{4}$/',
            'tollfree' => '/^80\\d{6}$/',
            'premium' => '/^(?:87|9[014578])\\d{6}$/',
            'shared' => '/^84\\d{6}$/',
            'emergency' => '/^999$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
