<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '266',
    'patterns' => [
        'national' => [
            'general' => '/^[2568]\\d{7}$/',
            'fixed' => '/^2\\d{7}$/',
            'mobile' => '/^[56]\\d{7}$/',
            'tollfree' => '/^800[256]\\d{4}$/',
            'emergency' => '/^11[257]$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
