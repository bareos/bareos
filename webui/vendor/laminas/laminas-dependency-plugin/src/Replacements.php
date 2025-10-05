<?php

declare(strict_types=1);

namespace Laminas\DependencyPlugin;

use function in_array;
use function preg_match;
use function sprintf;

final class Replacements
{
    /** @var string[] */
    private $ignore = [
        'zendframework/zend-debug',
        'zendframework/zend-version',
        'zendframework/zendservice-apple-apns',
        'zendframework/zendservice-google-gcm',
        'zfcampus/zf-apigility-example',
        'zfcampus/zf-angular',
        'zfcampus/zf-console',
        'zfcampus/zf-deploy',
    ];

    /**
     * @param string $name Original package name
     * @return string Transformed (or original) package name
     */
    public function transformPackageName($name)
    {
        switch ($name) {
            // Packages without replacements:
            case in_array($name, $this->ignore, true):
                /** @psalm-suppress MixedReturnStatement */
                return $name;
            // Packages with non-standard naming:
            case 'zendframework/zenddiagnostics':
                return 'laminas/laminas-diagnostics';
            case 'zendframework/zendoauth':
                return 'laminas/laminas-oauth';
            case 'zendframework/zendservice-recaptcha':
                return 'laminas/laminas-recaptcha';
            case 'zendframework/zendservice-twitter':
                return 'laminas/laminas-twitter';
            case 'zendframework/zendxml':
                return 'laminas/laminas-xml';
            case 'zendframework/zend-expressive':
                return 'mezzio/mezzio';
            case 'zendframework/zend-problem-details':
                return 'mezzio/mezzio-problem-details';
            case 'zfcampus/zf-apigility':
                return 'laminas-api-tools/api-tools';
            case 'zfcampus/zf-composer-autoloading':
                return 'laminas/laminas-composer-autoloading';
            case 'zfcampus/zf-development-mode':
                return 'laminas/laminas-development-mode';
            // All other packages:
            default:
                if (preg_match('#^zendframework/zend-expressive-zend(?<name>.*)$#', $name, $matches)) {
                    return sprintf('mezzio/mezzio-laminas%s', $matches['name']);
                }
                if (preg_match('#^zendframework/zend-expressive-(?<name>.*)$#', $name, $matches)) {
                    return sprintf('mezzio/mezzio-%s', $matches['name']);
                }
                if (preg_match('#^zfcampus/zf-apigility-(?<name>.*)$#', $name, $matches)) {
                    return sprintf('laminas-api-tools/api-tools-%s', $matches['name']);
                }
                if (preg_match('#^zfcampus/zf-(?<name>.*)$#', $name, $matches)) {
                    return sprintf('laminas-api-tools/api-tools-%s', $matches['name']);
                }
                if (preg_match('#^zendframework/zend-(?<name>.*)$#', $name, $matches)) {
                    return sprintf('laminas/laminas-%s', $matches['name']);
                }
                return $name;
        }
    }

    /**
     * @param string $name Original package name
     * @return bool
     */
    public function isZendPackage($name)
    {
        if (! preg_match('#^(zendframework|zfcampus)/#', $name)) {
            return false;
        }

        return true;
    }
}
