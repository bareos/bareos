<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '211',
    'patterns' => [
        'national' => [
            'general' => '/^[19]\\d{8}$/',
            'fixed' => '/^18\\d{7}$/',
            'mobile' => '/^(?:12|9[1257])\\d{7}$/',
        ],
        'possible' => [
            'general' => '/^\\d{9}$/',
        ],
    ],
];
