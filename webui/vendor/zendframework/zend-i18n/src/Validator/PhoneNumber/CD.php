<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '243',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[1-6]\\d{6}|8\\d{6,8}|9\\d{8})$/',
            'fixed' => '/^[1-6]\\d{6}$/',
            'mobile' => '/^(?:8(?:[0-259]\\d{2}|[48])\\d{5}|9[7-9]\\d{7})$/',
        ],
        'possible' => [
            'general' => '/^\\d{7,9}$/',
            'fixed' => '/^\\d{7}$/',
        ],
    ],
];
