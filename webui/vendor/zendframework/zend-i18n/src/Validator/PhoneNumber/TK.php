<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '690',
    'patterns' => [
        'national' => [
            'general' => '/^[2-5]\\d{3}$/',
            'fixed' => '/^[2-4]\\d{3}$/',
            'mobile' => '/^5\\d{3}$/',
        ],
        'possible' => [
            'general' => '/^\\d{4}$/',
        ],
    ],
];
