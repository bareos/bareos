<?php

// autoload_static.php @generated by Composer

namespace Composer\Autoload;

class ComposerStaticInit6df5b8a1471c61873b9f5b638848d4d7
{
    public static $prefixLengthsPsr4 = array (
        'Z' => 
        array (
            'Zend\\View\\' => 10,
            'Zend\\Version\\' => 13,
            'Zend\\Validator\\' => 15,
            'Zend\\Uri\\' => 9,
            'Zend\\Stdlib\\' => 12,
            'Zend\\Session\\' => 13,
            'Zend\\ServiceManager\\' => 20,
            'Zend\\Serializer\\' => 16,
            'Zend\\Navigation\\' => 16,
            'Zend\\Mvc\\' => 9,
            'Zend\\ModuleManager\\' => 19,
            'Zend\\Math\\' => 10,
            'Zend\\Log\\' => 9,
            'Zend\\Loader\\' => 12,
            'Zend\\Json\\' => 10,
            'Zend\\InputFilter\\' => 17,
            'Zend\\I18n\\' => 10,
            'Zend\\Hydrator\\' => 14,
            'Zend\\Http\\' => 10,
            'Zend\\Form\\' => 10,
            'Zend\\Filter\\' => 12,
            'Zend\\EventManager\\' => 18,
            'Zend\\Escaper\\' => 13,
            'Zend\\Console\\' => 13,
            'Zend\\Config\\' => 12,
            'Zend\\Cache\\' => 11,
        ),
        'P' => 
        array (
            'Psr\\Log\\' => 8,
            'Psr\\Container\\' => 14,
        ),
        'I' => 
        array (
            'Interop\\Container\\' => 18,
        ),
    );

    public static $prefixDirsPsr4 = array (
        'Zend\\View\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-view/src',
        ),
        'Zend\\Version\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-version/src',
        ),
        'Zend\\Validator\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-validator/src',
        ),
        'Zend\\Uri\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-uri/src',
        ),
        'Zend\\Stdlib\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-stdlib/src',
        ),
        'Zend\\Session\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-session/src',
        ),
        'Zend\\ServiceManager\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-servicemanager/src',
        ),
        'Zend\\Serializer\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-serializer/src',
        ),
        'Zend\\Navigation\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-navigation/src',
        ),
        'Zend\\Mvc\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-mvc/src',
        ),
        'Zend\\ModuleManager\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-modulemanager/src',
        ),
        'Zend\\Math\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-math/src',
        ),
        'Zend\\Log\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-log/src',
        ),
        'Zend\\Loader\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-loader/src',
        ),
        'Zend\\Json\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-json/src',
        ),
        'Zend\\InputFilter\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-inputfilter/src',
        ),
        'Zend\\I18n\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-i18n/src',
        ),
        'Zend\\Hydrator\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-hydrator/src',
        ),
        'Zend\\Http\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-http/src',
        ),
        'Zend\\Form\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-form/src',
        ),
        'Zend\\Filter\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-filter/src',
        ),
        'Zend\\EventManager\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-eventmanager/src',
        ),
        'Zend\\Escaper\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-escaper/src',
        ),
        'Zend\\Console\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-console/src',
        ),
        'Zend\\Config\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-config/src',
        ),
        'Zend\\Cache\\' => 
        array (
            0 => __DIR__ . '/..' . '/zendframework/zend-cache/src',
        ),
        'Psr\\Log\\' => 
        array (
            0 => __DIR__ . '/..' . '/psr/log/Psr/Log',
        ),
        'Psr\\Container\\' => 
        array (
            0 => __DIR__ . '/..' . '/psr/container/src',
        ),
        'Interop\\Container\\' => 
        array (
            0 => __DIR__ . '/..' . '/container-interop/container-interop/src/Interop/Container',
        ),
    );

    public static $classMap = array (
        'Composer\\InstalledVersions' => __DIR__ . '/..' . '/composer/InstalledVersions.php',
    );

    public static function getInitializer(ClassLoader $loader)
    {
        return \Closure::bind(function () use ($loader) {
            $loader->prefixLengthsPsr4 = ComposerStaticInit6df5b8a1471c61873b9f5b638848d4d7::$prefixLengthsPsr4;
            $loader->prefixDirsPsr4 = ComposerStaticInit6df5b8a1471c61873b9f5b638848d4d7::$prefixDirsPsr4;
            $loader->classMap = ComposerStaticInit6df5b8a1471c61873b9f5b638848d4d7::$classMap;

        }, null, ClassLoader::class);
    }
}
