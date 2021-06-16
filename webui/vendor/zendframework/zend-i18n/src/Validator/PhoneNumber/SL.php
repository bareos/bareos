<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '232',
    'patterns' => [
        'national' => [
            'general' => '/^[2-578]\\d{7}$/',
            'fixed' => '/^[235]2[2-4][2-9]\\d{4}$/',
            'mobile' => '/^(?:2[15]|3[034]|4[04]|5[05]|7[6-9]|88)\\d{6}$/',
            'emergency' => '/^(?:01|99)9$/',
        ],
        'possible' => [
            'general' => '/^\\d{6,8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
