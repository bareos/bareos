<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '267',
    'patterns' => [
        'national' => [
            'general' => '/^[2-79]\\d{6,7}$/',
            'fixed' => '/^(?:2(?:4[0-48]|6[0-24]|9[0578])|3(?:1[0235-9]|55|6\\d|7[01]|9[0-57])|4(?:6[03]|7[1267]|9[0-5])|5(?:3[0389]|4[0489]|7[1-47]|88|9[0-49])|6(?:2[1-35]|5[149]|8[067]))\\d{4}$/',
            'mobile' => '/^7(?:[1-35]\\d{6}|[46][0-7]\\d{5}|7[01]\\d{5})$/',
            'premium' => '/^90\\d{5}$/',
            'voip' => '/^79[12][01]\\d{4}$/',
            'emergency' => '/^99[789]$/',
        ],
        'possible' => [
            'general' => '/^\\d{7,8}$/',
            'fixed' => '/^\\d{7}$/',
            'mobile' => '/^\\d{8}$/',
            'premium' => '/^\\d{7}$/',
            'voip' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
