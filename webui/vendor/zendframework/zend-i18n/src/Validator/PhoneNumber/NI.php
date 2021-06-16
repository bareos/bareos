<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '505',
    'patterns' => [
        'national' => [
            'general' => '/^[128]\\d{7}$/',
            'fixed' => '/^2\\d{7}$/',
            'mobile' => '/^[578]\\d{7}$/',
            'tollfree' => '/^1800\\d{4}$/',
            'emergency' => '/^118$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
