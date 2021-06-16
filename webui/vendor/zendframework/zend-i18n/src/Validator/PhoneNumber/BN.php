<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '673',
    'patterns' => [
        'national' => [
            'general' => '/^[2-578]\\d{6}$/',
            'fixed' => '/^[2-5]\\d{6}$/',
            'mobile' => '/^[78]\\d{6}$/',
            'emergency' => '/^99[135]$/',
        ],
        'possible' => [
            'general' => '/^\\d{7}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
