<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '504',
    'patterns' => [
        'national' => [
            'general' => '/^[237-9]\\d{7}$/',
            'fixed' => '/^2(?:2(?:0[019]|1[1-36]|[23]\\d|4[056]|5[57]|8[0146-9]|9[012])|4(?:2|3-59]|3[13-689]|4[0-68]|5[1-35])|5(?:4[3-5]|5\\d|6[56]|74)|6(?:4[0-378]|[56]\\d|[78][0-8]|9[01])|7(?:6[46-9]|7[02-9]|8[34])|8(?:79|8[0-35789]|9[1-57-9]))\\d{4}$/',
            'mobile' => '/^[37-9]\\d{7}$/',
            'emergency' => '/^199$/',
        ],
        'possible' => [
            'general' => '/^\\d{8}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
