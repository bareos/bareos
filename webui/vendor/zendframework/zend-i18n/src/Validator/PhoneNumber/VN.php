<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '84',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[17]\\d{6,9}|[2-69]\\d{7,9}|8\\d{6,8})$/',
            'fixed' => '/^(?:2(?:[025-79]|1[0189]|[348][01])|3(?:[0136-9]|[25][01])|4\\d|5(?:[01][01]|[2-9])|6(?:[0-46-8]|5[01])|7(?:[02-79]|[18][01])|8[1-9])\\d{7}$/',
            'mobile' => '/^(?:9\\d|1(?:2\\d|6[2-9]|8[68]|99))\\d{7}$/',
            'tollfree' => '/^1800\\d{4,6}$/',
            'premium' => '/^1900\\d{4,6}$/',
            'uan' => '/^(?:[17]99\\d{4}|69\\d{5,6}|80\\d{5})$/',
            'emergency' => '/^11[345]$/',
        ],
        'possible' => [
            'general' => '/^\\d{7,10}$/',
            'fixed' => '/^\\d{9,10}$/',
            'mobile' => '/^\\d{9,10}$/',
            'tollfree' => '/^\\d{8,10}$/',
            'premium' => '/^\\d{8,10}$/',
            'uan' => '/^\\d{7,8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
