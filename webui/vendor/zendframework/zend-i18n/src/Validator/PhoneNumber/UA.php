<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '380',
    'patterns' => [
        'national' => [
            'general' => '/^[3-689]\\d{8}$/',
            'fixed' => '/^(?:3[1-8]|4[13-8]|5[1-7]|6[12459])\\d{7}$/',
            'mobile' => '/^(?:39|50|6[36-8]|9[1-9])\\d{7}$/',
            'tollfree' => '/^800\\d{6}$/',
            'premium' => '/^900\\d{6}$/',
            'emergency' => '/^1(?:0[123]|12)$/',
        ],
        'possible' => [
            'general' => '/^\\d{5,9}$/',
            'mobile' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{9}$/',
            'premium' => '/^\\d{9}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
