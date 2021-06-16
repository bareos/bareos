<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '509',
    'patterns' => [
        'national' => [
            'general' => '/^[2-489]\\d{7}$/',
            'fixed' => '/^2(?:[24]\\d|5[1-5]|94)\\d{5}$/',
            'mobile' => '/^(?:3[1-9]|4\\d)\\d{6}$/',
            'tollfree' => '/^8\\d{7}$/',
            'voip' => '/^98[89]\\d{5}$/',
            'shortcode' => '/^1\\d{2}$/',
            'emergency' => '/^11[48]$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'tollfree' => '/^\\d{8}$/',
            'voip' => '/^\\d{8}$/',
            'shortcode' => '/^\\d{3}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
