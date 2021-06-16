<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '1',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[2-9]\\d{9}|3\\d{6})$/',
            'fixed' => '/^(?:(?:2(?:04|[23]6|[48]9|50)|3(?:06|43|65)|4(?:03|1[68]|3[178]|5[06])|5(?:0[06]|1[49]|79|8[17])|6(?:0[04]|13|39|47)|7(?:0[059]|80|78)|8(?:[06]7|19|73)|90[25])[2-9]\\d{6}|310\\d{4})$/',
            'mobile' => '/^(?:2(?:04|[23]6|[48]9|50)|3(?:06|43|65)|4(?:03|1[68]|3[178]|5[06])|5(?:0[06]|1[49]|79|8[17])|6(?:0[04]|13|39|47)|7(?:0[059]|80|78)|8(?:[06]7|19|73)|90[25])[2-9]\\d{6}$/',
            'tollfree' => '/^(?:8(?:00|55|66|77|88)[2-9]\\d{6}|310\\d{4})$/',
            'premium' => '/^900[2-9]\\d{6}$/',
            'personal' => '/^5(?:00|33|44)[2-9]\\d{6}$/',
            'emergency' => '/^(?:112|911)$/',
        ],
        'possible' => [
            'general' => '/^\\d{7}(?:\\d{3})?$/',
            'premium' => '/^\\d{10}$/',
            'personal' => '/^\\d{10}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
