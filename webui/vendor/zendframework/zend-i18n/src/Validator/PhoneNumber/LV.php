<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '371',
    'patterns' => [
        'national' => [
            'general' => '/^[2689]\\d{7}$/',
            'fixed' => '/^6[3-8]\\d{6}$/',
            'mobile' => '/^2\\d{7}$/',
            'tollfree' => '/^80\\d{6}$/',
            'premium' => '/^90\\d{6}$/',
            'shared' => '/^81\\d{6}$/',
            'emergency' => '/^(?:0[123]|112)$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{2,3}$/',
        ],
    ],
];
