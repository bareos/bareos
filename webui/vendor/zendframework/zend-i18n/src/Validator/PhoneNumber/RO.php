<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '40',
    'patterns' => [
        'national' => [
            'general' => '/^(?:2\\d{5,8}|[37-9]\\d{8})$/',
            'fixed' => '/^(?:2(?:1(?:\\d{7}|9\\d{3})|[3-6](?:\\d{7}|\\d9\\d{2}))|3[13-6]\\d{7})$/',
            'mobile' => '/^7[1-8]\\d{7}$/',
            'tollfree' => '/^800\\d{6}$/',
            'premium' => '/^90[036]\\d{6}$/',
            'shared' => '/^801\\d{6}$/',
            'personal' => '/^802\\d{6}$/',
            'uan' => '/^37\\d{7}$/',
            'emergency' => '/^112$/',
        ],
        'possible' => [
            'general' => '/^\\d{6,9}$/',
            'mobile' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{9}$/',
            'premium' => '/^\\d{9}$/',
            'shared' => '/^\\d{9}$/',
            'personal' => '/^\\d{9}$/',
            'uan' => '/^\\d{9}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
