<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '687',
    'patterns' => [
        'national' => [
            'general' => '/^[2-47-9]\\d{5}$/',
            'fixed' => '/^(?:2[03-9]|3[0-5]|4[1-7]|88)\\d{4}$/',
            'mobile' => '/^(?:[79]\\d|8[0-79])\\d{4}$/',
            'premium' => '/^36\\d{4}$/',
            'shortcode' => '/^10(?:0[06]|1[02-46]|20|3[0125]|42|5[058]|77)$/',
            'emergency' => '/^1[5-8]$/',
        ],
        'possible' => [
            'general' => '/^\\d{6}$/',
            'shortcode' => '/^\\d{4}$/',
            'emergency' => '/^\\d{2}$/',
        ],
    ],
];
