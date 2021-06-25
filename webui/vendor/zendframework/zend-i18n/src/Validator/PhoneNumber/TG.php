<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '228',
    'patterns' => [
        'national' => [
            'general' => '/^[29]\\d{7}$/',
            'fixed' => '/^2(?:2[2-7]|3[23]|44|55|66|77)\\d{5}$/',
            'mobile' => '/^9[0-289]\\d{6}$/',
            'emergency' => '/^1(?:01|1[78]|7[17])$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
