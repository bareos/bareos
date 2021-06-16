<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '245',
    'patterns' => [
        'national' => [
            'general' => '/^[3567]\\d{6}$/',
            'fixed' => '/^3(?:2[0125]|3[1245]|4[12]|5[1-4]|70|9[1-467])\\d{4}$/',
            'mobile' => '/^[5-7]\\d{6}$/',
            'emergency' => '/^11[378]$/',
        ],
        'possible' => [
            'general' => '/^\\d{7}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
