<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '357',
    'patterns' => [
        'national' => [
            'general' => '/^[257-9]\\d{7}$/',
            'fixed' => '/^2[2-6]\\d{6}$/',
            'mobile' => '/^9[5-79]\\d{6}$/',
            'tollfree' => '/^800\\d{5}$/',
            'premium' => '/^90[09]\\d{5}$/',
            'shared' => '/^80[1-9]\\d{5}$/',
            'personal' => '/^700\\d{5}$/',
            'uan' => '/^(?:50|77)\\d{6}$/',
            'emergency' => '/^1(?:12|99)$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
