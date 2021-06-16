<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '231',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[29]\\d|[4-6]|7\\d{1,2}|[38]\\d{2})\\d{6}$/',
            'fixed' => '/^2\\d{7}$/',
            'mobile' => '/^(?:4[67]|5\\d|6[4-8]|7(?:7[67]\\d|\\d{2})|88\\d{2})\\d{5}$/',
            'premium' => '/^90\\d{6}$/',
            'voip' => '/^33200\\d{4}$/',
            'emergency' => '/^(?:355|911)$/',
        ],
        'possible' => [
            'general' => '/^\\d{7,9}$/',
            'fixed' => '/^\\d{8}$/',
            'premium' => '/^\\d{8}$/',
            'voip' => '/^\\d{9}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
