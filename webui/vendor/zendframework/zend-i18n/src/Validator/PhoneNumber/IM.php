<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '44',
    'patterns' => [
        'national' => [
            'general' => '/^[135789]\\d{6,9}$/',
            'fixed' => '/^1624\\d{6}$/',
            'mobile' => '/^7[569]24\\d{6}$/',
            'tollfree' => '/^808162\\d{4}$/',
            'premium' => '/^(?:872299|90[0167]624)\\d{4}$/',
            'shared' => '/^8(?:4(?:40[49]06|5624\\d)|70624\\d)\\d{3}$/',
            'personal' => '/^70\\d{8}$/',
            'voip' => '/^56\\d{8}$/',
            'uan' => '/^(?:3(?:08162\\d|3\\d{5}|4(?:40[49]06|5624\\d)|7(?:0624\\d|2299\\d))\\d{3}|55\\d{8})$/',
            'shortcode' => '/^1\\d{2}(?:\\d{3})?$/',
            'emergency' => '/^999$/',
        ],
        'possible' => [
            'general' => '/^\\d{6,10}$/',
            'mobile' => '/^\\d{10}$/',
            'tollfree' => '/^\\d{10}$/',
            'premium' => '/^\\d{10}$/',
            'shared' => '/^\\d{10}$/',
            'personal' => '/^\\d{10}$/',
            'voip' => '/^\\d{10}$/',
            'uan' => '/^\\d{10}$/',
            'shortcode' => '/^\\d{3,6}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
