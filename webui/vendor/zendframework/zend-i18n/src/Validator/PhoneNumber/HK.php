<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '852',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[235-7]\\d{7}|8\\d{7,8}|9\\d{4,10})$/',
            'fixed' => '/^(?:[23]\\d|5[78])\\d{6}$/',
            'mobile' => '/^(?:5[1-69]\\d|6\\d{2}|9(?:0[1-9]|[1-8]\\d))\\d{5}$/',
            'pager' => '/^7\\d{7}$/',
            'tollfree' => '/^800\\d{6}$/',
            'premium' => '/^900(?:[0-24-9]\\d{7}|3\\d{1,4})$/',
            'personal' => '/^8[1-3]\\d{6}$/',
            'emergency' => '/^(?:112|99[29])$/',
        ],
        'possible' => [
            'general' => '/^\\d{5,11}$/',
            'fixed' => '/^\\d{8}$/',
            'mobile' => '/^\\d{8}$/',
            'pager' => '/^\\d{8}$/',
            'tollfree' => '/^\\d{9}$/',
            'premium' => '/^\\d{5,11}$/',
            'personal' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
