<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '688',
    'patterns' => [
        'national' => [
            'general' => '/^[29]\\d{4,5}$/',
            'fixed' => '/^2[02-9]\\d{3}$/',
            'mobile' => '/^90\\d{4}$/',
            'emergency' => '/^911$/',
        ],
        'possible' => [
            'general' => '/^\\d{5,6}$/',
            'fixed' => '/^\\d{5}$/',
            'mobile' => '/^\\d{6}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
