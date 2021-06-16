<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '233',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[235]\\d{8}|8\\d{7})$/',
            'fixed' => '/^3(?:0[237]\\d|[167](?:2[0-6]|7\\d)|2(?:2[0-5]|7\\d)|3(?:2[0-3]|7\\d)|4(?:2[013-9]|3[01]|7\\d)|5(?:2[0-7]|7\\d)|8(?:2[0-2]|7\\d)|9(?:20|7\\d))\\d{5}$/',
            'mobile' => '/^(?:2[034678]|5[047])\\d{7}$/',
            'tollfree' => '/^800\\d{5}$/',
            'emergency' => '/^(?:19[123]|999)$/',
        ],
        'possible' => [
            'general' => '/^\\d{7,9}$/',
            'fixed' => '/^\\d{7,9}$/',
            'mobile' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
