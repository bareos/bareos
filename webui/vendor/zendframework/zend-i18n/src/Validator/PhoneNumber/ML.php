<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '223',
    'patterns' => [
        'national' => [
            'general' => '/^[246-8]\\d{7}$/',
            'fixed' => '/^(?:2(?:0(?:2[0-589]|7[027-9])|1(?:2[5-7]|[3-689]\\d))|44[239]\\d)\\d{4}$/',
            'mobile' => '/^(?:6[3569]|7\\d)\\d{6}$/',
            'tollfree' => '/^800\\d{5}$/',
            'emergency' => '/^1[578]$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{2}$/',
        ],
    ],
];
