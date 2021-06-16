<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '299',
    'patterns' => [
        'national' => [
            'general' => '/^[1-689]\\d{5}$/',
            'fixed' => '/^(?:19|3[1-6]|6[14689]|8[14-79]|9\\d)\\d{4}$/',
            'mobile' => '/^[245][2-9]\\d{4}$/',
            'tollfree' => '/^80\\d{4}$/',
            'voip' => '/^3[89]\\d{4}$/',
            'emergency' => '/^112$/',
        ],
        'possible' => [
            'general' => '/^\\d{6}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
