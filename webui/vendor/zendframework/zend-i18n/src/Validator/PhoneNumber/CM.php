<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '237',
    'patterns' => [
        'national' => [
            'general' => '/^[237-9]\\d{7}$/',
            'fixed' => '/^(?:22|33)\\d{6}$/',
            'mobile' => '/^[79]\\d{7}$/',
            'tollfree' => '/^800\\d{5}$/',
            'premium' => '/^88\\d{6}$/',
            'emergency' => '/^1?1[37]$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{2,3}$/',
        ],
    ],
];
