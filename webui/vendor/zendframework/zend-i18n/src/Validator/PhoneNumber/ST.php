<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '239',
    'patterns' => [
        'national' => [
            'general' => '/^[29]\\d{6}$/',
            'fixed' => '/^22\\d{5}$/',
            'mobile' => '/^9[89]\\d{5}$/',
            'emergency' => '/^112$/',
        ],
        'possible' => [
            'general' => '/^\\d{7}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
