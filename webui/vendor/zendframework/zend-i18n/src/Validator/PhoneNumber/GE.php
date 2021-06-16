<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '995',
    'patterns' => [
        'national' => [
            'general' => '/^[3458]\\d{8}$/',
            'fixed' => '/^(?:3(?:[256]\\d|4[124-9]|7[0-4])|4(?:1\\d|2[2-7]|3[1-79]|4[2-8]|7[239]|9[1-7]))\\d{6}$/',
            'mobile' => '/^5(?:14|5[01578]|68|7[0147-9]|9[0-35-9])\\d{6}$/',
            'tollfree' => '/^800\\d{6}$/',
            'emergency' => '/^(?:0(?:11|22|33)|1(?:1[123]|22))$/',
        ],
        'possible' => [
            'general' => '/^\\d{6,9}$/',
            'fixed' => '/^\\d{6,9}$/',
            'mobile' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{9}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
