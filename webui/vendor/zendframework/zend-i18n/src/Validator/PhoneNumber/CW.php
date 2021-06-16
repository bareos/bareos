<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

return [
    'code' => '599',
    'patterns' => [
        'national' => [
            'general' => '/^[169]\\d{6,7}$/',
            'fixed' => '/^9(?:[48]\\d{2}|50\\d|7(?:2[0-2]|[34]\\d|6[35-7]|77))\\d{4}$/',
            'mobile' => '/^9(?:5(?:[1246]\\d|3[01])|6(?:[1679]\\d|3[01]))\\d{4}$/',
            'pager' => '/^955\\d{5}$/',
            'shared' => '/^(?:10|69)\\d{5}$/',
            'emergency' => '/^(?:112|911)$/',
        ],
        'possible' => [
            'general' => '/^\\d{7,8}$/',
            'shared' => '/^\\d{7}$/',
            'emergency' => '/^\\d{3}$/',
        ],
    ],
];
