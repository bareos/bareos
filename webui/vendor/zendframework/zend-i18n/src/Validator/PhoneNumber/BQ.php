<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '599',
    'patterns' => [
        'national' => [
            'general' => '/^[347]\\d{6}$/',
            'fixed' => '/^(?:318[023]|416[0239]|7(?:1[578]|50)\\d)\\d{3}$/',
            'mobile' => '/^(?:318[1456]|416[15-8]|7(?:0[01]|[89]\\d)\\d)\\d{3}$/',
            'emergency' => '/^(?:112|911)$/',
        ],
        'possible' => [
            'general' => '/^\\d{7}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
