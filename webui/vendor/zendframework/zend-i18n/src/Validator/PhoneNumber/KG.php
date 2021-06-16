<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '996',
    'patterns' => [
        'national' => [
            'general' => '/^[35-8]\\d{8,9}$/',
            'fixed' => '/^(?:3(?:1(?:2\\d|3[1-9]|47|5[02]|6[1-8])|2(?:22|3[0-479]|6[0-7])|4(?:22|5[6-9]|6[0-4])|5(?:22|3[4-7]|59|6[0-5])|6(?:22|5[35-7]|6[0-3])|7(?:22|3[468]|4[1-9]|59|6\\d|7[5-7])|9(?:22|4[1-8]|6[0-8]))|6(?:09|12|2[2-4])\\d)\\d{5}$/',
            'mobile' => '/^(?:5[124-7]\\d{7}|7(?:0[0-357-9]|7\\d)\\d{6})$/',
            'tollfree' => '/^800\\d{6,7}$/',
            'emergency' => '/^10[123]$/',
        ],
        'possible' => [
            'general' => '/^\\d{5,10}$/',
            'mobile' => '/^\\d{9}$/',
            'tollfree' => '/^\\d{9,10}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
