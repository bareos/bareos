<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '240',
    'patterns' => [
        'national' => [
            'general' => '/^[23589]\\d{8}$/',
            'fixed' => '/^3(?:3(?:3\\d[7-9]|[0-24-9]\\d[46])|5\\d{2}[7-9])\\d{4}$/',
            'mobile' => '/^(?:222|551)\\d{6}$/',
            'tollfree' => '/^80\\d[1-9]\\d{5}$/',
            'premium' => '/^90\\d[1-9]\\d{5}$/',
        ],
        'possible' => [
            'general' => '/^\\d{9}$/',
        ],
    ],
];
