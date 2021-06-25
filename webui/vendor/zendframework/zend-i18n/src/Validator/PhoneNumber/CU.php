<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '53',
    'patterns' => [
        'national' => [
            'general' => '/^[2-57]\\d{5,7}$/',
            'fixed' => '/^(?:2[1-4]\\d{5,6}|3(?:1\\d{6}|[23]\\d{4,6})|4(?:[125]\\d{5,6}|[36]\\d{6}|[78]\\d{4,6})|7\\d{6,7})$/',
            'mobile' => '/^5\\d{7}$/',
            'shortcode' => '/^1(?:1(?:6111|8)|40)$/',
            'emergency' => '/^10[456]$/',
        ],
        'possible' => [
            'general' => '/^\\d{4,8}$/',
            'mobile' => '/^\\d{8}$/',
            'shortcode' => '/^\\d{3,6}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
