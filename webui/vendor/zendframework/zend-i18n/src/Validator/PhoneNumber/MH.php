<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '692',
    'patterns' => [
        'national' => [
            'general' => '/^[2-6]\\d{6}$/',
            'fixed' => '/^(?:247|528|625)\\d{4}$/',
            'mobile' => '/^(?:235|329|45[56]|545)\\d{4}$/',
            'voip' => '/^635\\d{4}$/',
        ],
        'possible' => [
            'general' => '/^\\d{7}$/',
        ],
    ],
];
