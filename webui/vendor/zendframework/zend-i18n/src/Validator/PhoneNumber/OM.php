<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '968',
    'patterns' => [
        'national' => [
            'general' => '/^(?:(?:2[2-6]|5|9[1-9])\\d{6}|800\\d{5,6})$/',
            'fixed' => '/^2[2-6]\\d{6}$/',
            'mobile' => '/^9[1-9]\\d{6}$/',
            'tollfree' => '/^(?:8007\\d{4,5}|500\\d{4})$/',
            'emergency' => '/^9999$/',
        ],
        'possible' => [
            'general' => '/^\\d{7,9}$/',
            'fixed' => '/^\\d{8}$/',
            'mobile' => '/^\\d{8}$/',
            'tollfree' => '/^\\d{7,9}$/',
            'emergency' => '/^\\d{4}$/',
        ],
    ],
];
