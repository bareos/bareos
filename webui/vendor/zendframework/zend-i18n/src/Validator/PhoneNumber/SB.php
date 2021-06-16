<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '677',
    'patterns' => [
        'national' => [
            'general' => '/^[1-9]\\d{4,6}$/',
            'fixed' => '/^(?:1[4-79]|[23]\\d|4[01]|5[03]|6[0-37])\\d{3}$/',
            'mobile' => '/^(?:48\\d{3}|7(?:[46-8]\\d|5[025-9]|90)\\d{4}|8[4-8]\\d{5}|9(?:[46]\\d|5[0-46-9]|7[0-689]|8[0-79]|9[0-8])\\d{4})$/',
            'tollfree' => '/^1[38]\\d{3}$/',
            'voip' => '/^5[12]\\d{3}$/',
            'shortcode' => '/^(?:1(?:0[02-79]|1[12]|2[0-26]|4[189]|68)|9(?:[01]1|22|33|55|77|88))$/',
            'emergency' => '/^999$/',
        ],
        'possible' => [
            'general' => '/^\\d{5,7}$/',
            'fixed' => '/^\\d{5}$/',
            'tollfree' => '/^\\d{5}$/',
            'voip' => '/^\\d{5}$/',
            'shortcode' => '/^\\d{3}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
