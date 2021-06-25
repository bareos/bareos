<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '218',
    'patterns' => [
        'national' => [
            'general' => '/^[25679]\\d{8}$/',
            'fixed' => '/^(?:2[1345]|5[1347]|6[123479]|71)\\d{7}$/',
            'mobile' => '/^9[1-6]\\d{7}$/',
            'emergency' => '/^19[013]$/',
        ],
        'possible' => [
            'general' => '/^\\d{7,9}$/',
            'mobile' => '/^\\d{9}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
