<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '354',
    'patterns' => [
        'national' => [
            'general' => '/^(?:[4-9]\\d{6}|38\\d{7})$/',
            'fixed' => '/^(?:4(?:[14][0-245]|2[0-7]|[37][0-8]|5[0-3568]|6\\d|8[0-36-8])|5(?:05|[156]\\d|2[02578]|3[013-7]|4[03-7]|7[0-2578]|8[0-35-9]|9[013-689])|87[23])\\d{4}$/',
            'mobile' => '/^(?:38[59]\\d{6}|(?:6(?:1[0-8]|3[0-27-9]|4[0-27]|5[0-29]|[67][0-69]|9\\d)|7(?:5[057]|7\\d|8[0-3])|8(?:2[0-5]|[469]\\d|5[1-9]))\\d{4})$/',
            'tollfree' => '/^800\\d{4}$/',
            'premium' => '/^90\\d{5}$/',
            'voip' => '/^49[0-24-79]\\d{4}$/',
            'voicemail' => '/^(?:388\\d{6}|(?:6(?:2[0-8]|49|8\\d)|8(?:2[6-9]|[38]\\d|50|7[014-9])|95[48])\\d{4})$/',
            'emergency' => '/^112$/',
        ],
        'possible' => [
            'general' => '/^\\d{7,9}$/',
            'fixed' => '/^\\d{7}$/',
            'tollfree' => '/^\\d{7}$/',
            'premium' => '/^\\d{7}$/',
            'voip' => '/^\\d{7}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
