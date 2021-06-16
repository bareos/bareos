<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '41',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[2-9]\\d{8}|860\\d{9})$/',
            'fixed' => '/^(?:2[12467]|3[1-4]|4[134]|5[12568]|6[12]|[7-9]1)\\d{7}$/',
            'mobile' => '/^7[46-9]\\d{7}$/',
            'tollfree' => '/^800\\d{6}$/',
            'premium' => '/^90[016]\\d{6}$/',
            'shared' => '/^84[0248]\\d{6}$/',
            'personal' => '/^878\\d{6}$/',
            'voicemail' => '/^860\\d{9}$/',
            'emergency' => '/^1(?:1[278]|44)$/',
        ],
        'possible' => [
            'general' => '/^\\d{9}(?:\\d{3})?$/',
            'fixed' => '/^\\d{9}$/',
            'mobile' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{9}$/',
            'premium' => '/^\\d{9}$/',
            'shared' => '/^\\d{9}$/',
            'personal' => '/^\\d{9}$/',
            'voicemail' => '/^\\d{12}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
