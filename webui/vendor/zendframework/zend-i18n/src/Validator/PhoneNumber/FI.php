<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '358',
    'patterns' => [
        'national' => [
            'general' => '/^(?:1\\d{4,11}|[2-9]\\d{4,10})$/',
            'fixed' => '/^(?:1(?:[3569][1-8]\\d{3,9}|[47]\\d{5,10})|2[1-8]\\d{3,9}|3(?:[1-8]\\d{3,9}|9\\d{4,8})|[5689][1-8]\\d{3,9})$/',
            'mobile' => '/^(?:4\\d{5,10}|50\\d{4,8})$/',
            'tollfree' => '/^800\\d{4,7}$/',
            'premium' => '/^[67]00\\d{5,6}$/',
            'uan' => '/^(?:[13]0\\d{4,8}|2(?:0(?:[016-8]\\d{3,7}|[2-59]\\d{2,7})|9\\d{4,8})|60(?:[12]\\d{5,6}|6\\d{7})|7(?:1\\d{7}|3\\d{8}|5[03-9]\\d{2,7}))$/',
            'emergency' => '/^112$/',
        ],
        'possible' => [
            'general' => '/^\\d{5,12}$/',
            'mobile' => '/^\\d{6,11}$/',
            'tollfree' => '/^\\d{7,10}$/',
            'premium' => '/^\\d{8,9}$/',
            'uan' => '/^\\d{5,10}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
