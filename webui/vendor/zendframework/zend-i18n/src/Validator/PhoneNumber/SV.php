<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '503',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[267]\\d{7}|[89]\\d{6}(?:\\d{4})?)$/',
            'fixed' => '/^2[1-6]\\d{6}$/',
            'mobile' => '/^[67]\\d{7}$/',
            'tollfree' => '/^800\\d{4}(?:\\d{4})?$/',
            'premium' => '/^900\\d{4}(?:\\d{4})?$/',
            'emergency' => '/^911$/',
        ],
        'possible' => [
            'general' => '/^(?:\\d{7,8}|\\d{11})$/',
            'fixed' => '/^\\d{8}$/',
            'mobile' => '/^\\d{8}$/',
            'tollfree' => '/^\\d{7}(?:\\d{4})?$/',
            'premium' => '/^\\d{7}(?:\\d{4})?$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
