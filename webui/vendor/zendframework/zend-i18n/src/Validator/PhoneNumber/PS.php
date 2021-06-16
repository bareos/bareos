<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '970',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[24589]\\d{7,8}|1(?:[78]\\d{8}|[49]\\d{2,3}))$/',
            'fixed' => '/^(?:22[234789]|42[45]|82[01458]|92[369])\\d{5}$/',
            'mobile' => '/^5[69]\\d{7}$/',
            'tollfree' => '/^1800\\d{6}$/',
            'premium' => '/^1(?:4|9\\d)\\d{2}$/',
            'shared' => '/^1700\\d{6}$/',
        ],
        'possible' => [
            'general' => '/^\\d{4,10}$/',
            'fixed' => '/^\\d{7,8}$/',
            'mobile' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{10}$/',
            'premium' => '/^\\d{4,5}$/',
            'shared' => '/^\\d{10}$/',
        ],
    ],
];
