<?php

/*
 * This file is part of Composer.
 *
 * (c) Nils Adermann <naderman@naderman.de>
 *     Jordi Boggiano <j.boggiano@seld.be>
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

namespace Composer;

use Composer\Autoload\ClassLoader;
use Composer\Semver\VersionParser;

/**
 * This class is copied in every Composer installed project and available to all
 *
 * See also https://getcomposer.org/doc/07-runtime.md#installed-versions
 *
 * To require it's presence, you can require `composer-runtime-api ^2.0`
 */
class InstalledVersions
{
    private static $installed = array (
  'root' => 
  array (
    'pretty_version' => 'dev-master',
    'version' => 'dev-master',
    'aliases' => 
    array (
    ),
    'reference' => '7fc426410c933a5ebdd1ffceb6ef0af3ee8c81b6',
    'name' => 'bareos/bareos-webui',
  ),
  'versions' => 
  array (
    'bareos/bareos-webui' => 
    array (
      'pretty_version' => 'dev-master',
      'version' => 'dev-master',
      'aliases' => 
      array (
      ),
      'reference' => '7fc426410c933a5ebdd1ffceb6ef0af3ee8c81b6',
    ),
    'container-interop/container-interop' => 
    array (
      'pretty_version' => '1.2.0',
      'version' => '1.2.0.0',
      'aliases' => 
      array (
      ),
      'reference' => '79cbf1341c22ec75643d841642dd5d6acd83bdb8',
    ),
    'psr/cache' => 
    array (
      'pretty_version' => '1.0.1',
      'version' => '1.0.1.0',
      'aliases' => 
      array (
      ),
      'reference' => 'd11b50ad223250cf17b86e38383413f5a6764bf8',
    ),
    'psr/cache-implementation' => 
    array (
      'provided' => 
      array (
        0 => '1.0',
      ),
    ),
    'psr/container' => 
    array (
      'pretty_version' => '1.1.1',
      'version' => '1.1.1.0',
      'aliases' => 
      array (
      ),
      'reference' => '8622567409010282b7aeebe4bb841fe98b58dcaf',
    ),
    'psr/http-message' => 
    array (
      'pretty_version' => '1.0.1',
      'version' => '1.0.1.0',
      'aliases' => 
      array (
      ),
      'reference' => 'f6561bf28d520154e4b0ec72be95418abe6d9363',
    ),
    'psr/http-message-implementation' => 
    array (
      'provided' => 
      array (
        0 => '1.0',
      ),
    ),
    'psr/log' => 
    array (
      'pretty_version' => '1.1.4',
      'version' => '1.1.4.0',
      'aliases' => 
      array (
      ),
      'reference' => 'd49695b909c3b7628b6289db5479a1c204601f11',
    ),
    'psr/log-implementation' => 
    array (
      'provided' => 
      array (
        0 => '1.0.0',
      ),
    ),
    'psr/simple-cache' => 
    array (
      'pretty_version' => '1.0.1',
      'version' => '1.0.1.0',
      'aliases' => 
      array (
      ),
      'reference' => '408d5eafb83c57f6365a3ca330ff23aa4a5fa39b',
    ),
    'psr/simple-cache-implementation' => 
    array (
      'provided' => 
      array (
        0 => '1.0',
      ),
    ),
    'zendframework/zend-cache' => 
    array (
      'pretty_version' => '2.8.3',
      'version' => '2.8.3.0',
      'aliases' => 
      array (
      ),
      'reference' => 'e6eb52d88a7305ac3a50780fe29be339a967fe8e',
    ),
    'zendframework/zend-config' => 
    array (
      'pretty_version' => '2.6.0',
      'version' => '2.6.0.0',
      'aliases' => 
      array (
      ),
      'reference' => '9dc083ce3341a12bfc1053df40b5f20e42b1f862',
    ),
    'zendframework/zend-console' => 
    array (
      'pretty_version' => '2.7.0',
      'version' => '2.7.0.0',
      'aliases' => 
      array (
      ),
      'reference' => 'df606a83773d17e0b9d76c2dcfa0e8eb7318b959',
    ),
    'zendframework/zend-diactoros' => 
    array (
      'pretty_version' => '1.8.7',
      'version' => '1.8.7.0',
      'aliases' => 
      array (
      ),
      'reference' => 'a600c450333e80b3d18ff5baafbbd78797ff5b6a',
    ),
    'zendframework/zend-escaper' => 
    array (
      'pretty_version' => '2.6.1',
      'version' => '2.6.1.0',
      'aliases' => 
      array (
      ),
      'reference' => '991a6929bf825fc2fb4cb7931795c23966f1c414',
    ),
    'zendframework/zend-eventmanager' => 
    array (
      'pretty_version' => '2.6.4',
      'version' => '2.6.4.0',
      'aliases' => 
      array (
      ),
      'reference' => '22817b6165075f9b240ab1c388f0b209023b95e9',
    ),
    'zendframework/zend-filter' => 
    array (
      'pretty_version' => '2.9.2',
      'version' => '2.9.2.0',
      'aliases' => 
      array (
      ),
      'reference' => '0ac729a755316202bd8745a9374dff050d42ae8a',
    ),
    'zendframework/zend-form' => 
    array (
      'pretty_version' => '2.13.0',
      'version' => '2.13.0.0',
      'aliases' => 
      array (
      ),
      'reference' => '539a302b119197efb04b1907aed2907aabcfff4f',
    ),
    'zendframework/zend-http' => 
    array (
      'pretty_version' => '2.8.4',
      'version' => '2.8.4.0',
      'aliases' => 
      array (
      ),
      'reference' => '91eca9e4e238d3d946cbdb26492d9e26552c8ff8',
    ),
    'zendframework/zend-hydrator' => 
    array (
      'pretty_version' => '1.1.0',
      'version' => '1.1.0.0',
      'aliases' => 
      array (
      ),
      'reference' => '9caab439f08227d258507a933a9c0181c019c004',
    ),
    'zendframework/zend-i18n' => 
    array (
      'pretty_version' => '2.10.1',
      'version' => '2.10.1.0',
      'aliases' => 
      array (
      ),
      'reference' => 'bc12fd36d7f2ec0e549faa6f6cbd3c54bd43ae9d',
    ),
    'zendframework/zend-inputfilter' => 
    array (
      'pretty_version' => '2.10.1',
      'version' => '2.10.1.0',
      'aliases' => 
      array (
      ),
      'reference' => 'd6d9e2cd72da2ba9bbed252e752518a4dce101c7',
    ),
    'zendframework/zend-json' => 
    array (
      'pretty_version' => '2.6.1',
      'version' => '2.6.1.0',
      'aliases' => 
      array (
      ),
      'reference' => 'bac51ce54b880969f3e40713dcbdf6bce5927ed1',
    ),
    'zendframework/zend-loader' => 
    array (
      'pretty_version' => '2.6.1',
      'version' => '2.6.1.0',
      'aliases' => 
      array (
      ),
      'reference' => '2a2a367bbf4060f4b43e01ceec257c69267fb2d8',
    ),
    'zendframework/zend-log' => 
    array (
      'pretty_version' => '2.12.0',
      'version' => '2.12.0.0',
      'aliases' => 
      array (
      ),
      'reference' => 'b861f9ba38e2cbcf744d8cab85222cd213d20507',
    ),
    'zendframework/zend-modulemanager' => 
    array (
      'pretty_version' => '2.8.4',
      'version' => '2.8.4.0',
      'aliases' => 
      array (
      ),
      'reference' => 'bf92ea9ce9c37aecb6e7364821a4ec8efb514d2a',
    ),
    'zendframework/zend-mvc' => 
    array (
      'pretty_version' => '2.7.15',
      'version' => '2.7.15.0',
      'aliases' => 
      array (
      ),
      'reference' => '855331d0f5e92c4f72637dcc9bd3bf836b1636e1',
    ),
    'zendframework/zend-navigation' => 
    array (
      'pretty_version' => '2.9.1',
      'version' => '2.9.1.0',
      'aliases' => 
      array (
      ),
      'reference' => '7d78984264da23f665bca32ee7b0581684d5cac1',
    ),
    'zendframework/zend-psr7bridge' => 
    array (
      'pretty_version' => '0.2.2',
      'version' => '0.2.2.0',
      'aliases' => 
      array (
      ),
      'reference' => '26ab87f414e638633592a14407b3988a8f557266',
    ),
    'zendframework/zend-router' => 
    array (
      'replaced' => 
      array (
        0 => '^2.0',
      ),
    ),
    'zendframework/zend-serializer' => 
    array (
      'pretty_version' => '2.9.1',
      'version' => '2.9.1.0',
      'aliases' => 
      array (
      ),
      'reference' => '37d6d68f730aae9c5e439559950372147cc8aba7',
    ),
    'zendframework/zend-servicemanager' => 
    array (
      'pretty_version' => '2.7.11',
      'version' => '2.7.11.0',
      'aliases' => 
      array (
      ),
      'reference' => '904dc66a7a3972de78bd3af1ef4f8e102814122e',
    ),
    'zendframework/zend-session' => 
    array (
      'pretty_version' => '2.8.7',
      'version' => '2.8.7.0',
      'aliases' => 
      array (
      ),
      'reference' => '52b3bc621469f27a23056acc184acdc6f24f34f8',
    ),
    'zendframework/zend-stdlib' => 
    array (
      'pretty_version' => '2.7.7',
      'version' => '2.7.7.0',
      'aliases' => 
      array (
      ),
      'reference' => 'baa65aec7bc75260254b5f03447f0c16360f9e59',
    ),
    'zendframework/zend-uri' => 
    array (
      'pretty_version' => '2.7.1',
      'version' => '2.7.1.0',
      'aliases' => 
      array (
      ),
      'reference' => 'b01ac33e2038af9340c6cbcda483ecb3427a10be',
    ),
    'zendframework/zend-validator' => 
    array (
      'pretty_version' => '2.11.1',
      'version' => '2.11.1.0',
      'aliases' => 
      array (
      ),
      'reference' => '0bb5cc294139d4f3513470cdf95d931f5b67b5d6',
    ),
    'zendframework/zend-version' => 
    array (
      'pretty_version' => '2.5.1',
      'version' => '2.5.1.0',
      'aliases' => 
      array (
      ),
      'reference' => '9635dd35e0e0d7b759c47c62a86ab3f558e58d4b',
    ),
    'zendframework/zend-view' => 
    array (
      'pretty_version' => '2.11.4',
      'version' => '2.11.4.0',
      'aliases' => 
      array (
      ),
      'reference' => 'f60542681f0bef261eaf4792cc3433e9c22b3832',
    ),
  ),
);
    private static $canGetVendors;
    private static $installedByVendor = array();

    /**
     * Returns a list of all package names which are present, either by being installed, replaced or provided
     *
     * @return string[]
     * @psalm-return list<string>
     */
    public static function getInstalledPackages()
    {
        $packages = array();
        foreach (self::getInstalled() as $installed) {
            $packages[] = array_keys($installed['versions']);
        }

        if (1 === \count($packages)) {
            return $packages[0];
        }

        return array_keys(array_flip(\call_user_func_array('array_merge', $packages)));
    }

    /**
     * Checks whether the given package is installed
     *
     * This also returns true if the package name is provided or replaced by another package
     *
     * @param  string $packageName
     * @return bool
     */
    public static function isInstalled($packageName)
    {
        foreach (self::getInstalled() as $installed) {
            if (isset($installed['versions'][$packageName])) {
                return true;
            }
        }

        return false;
    }

    /**
     * Checks whether the given package satisfies a version constraint
     *
     * e.g. If you want to know whether version 2.3+ of package foo/bar is installed, you would call:
     *
     *   Composer\InstalledVersions::satisfies(new VersionParser, 'foo/bar', '^2.3')
     *
     * @param VersionParser $parser      Install composer/semver to have access to this class and functionality
     * @param string        $packageName
     * @param string|null   $constraint  A version constraint to check for, if you pass one you have to make sure composer/semver is required by your package
     *
     * @return bool
     */
    public static function satisfies(VersionParser $parser, $packageName, $constraint)
    {
        $constraint = $parser->parseConstraints($constraint);
        $provided = $parser->parseConstraints(self::getVersionRanges($packageName));

        return $provided->matches($constraint);
    }

    /**
     * Returns a version constraint representing all the range(s) which are installed for a given package
     *
     * It is easier to use this via isInstalled() with the $constraint argument if you need to check
     * whether a given version of a package is installed, and not just whether it exists
     *
     * @param  string $packageName
     * @return string Version constraint usable with composer/semver
     */
    public static function getVersionRanges($packageName)
    {
        foreach (self::getInstalled() as $installed) {
            if (!isset($installed['versions'][$packageName])) {
                continue;
            }

            $ranges = array();
            if (isset($installed['versions'][$packageName]['pretty_version'])) {
                $ranges[] = $installed['versions'][$packageName]['pretty_version'];
            }
            if (array_key_exists('aliases', $installed['versions'][$packageName])) {
                $ranges = array_merge($ranges, $installed['versions'][$packageName]['aliases']);
            }
            if (array_key_exists('replaced', $installed['versions'][$packageName])) {
                $ranges = array_merge($ranges, $installed['versions'][$packageName]['replaced']);
            }
            if (array_key_exists('provided', $installed['versions'][$packageName])) {
                $ranges = array_merge($ranges, $installed['versions'][$packageName]['provided']);
            }

            return implode(' || ', $ranges);
        }

        throw new \OutOfBoundsException('Package "' . $packageName . '" is not installed');
    }

    /**
     * @param  string      $packageName
     * @return string|null If the package is being replaced or provided but is not really installed, null will be returned as version, use satisfies or getVersionRanges if you need to know if a given version is present
     */
    public static function getVersion($packageName)
    {
        foreach (self::getInstalled() as $installed) {
            if (!isset($installed['versions'][$packageName])) {
                continue;
            }

            if (!isset($installed['versions'][$packageName]['version'])) {
                return null;
            }

            return $installed['versions'][$packageName]['version'];
        }

        throw new \OutOfBoundsException('Package "' . $packageName . '" is not installed');
    }

    /**
     * @param  string      $packageName
     * @return string|null If the package is being replaced or provided but is not really installed, null will be returned as version, use satisfies or getVersionRanges if you need to know if a given version is present
     */
    public static function getPrettyVersion($packageName)
    {
        foreach (self::getInstalled() as $installed) {
            if (!isset($installed['versions'][$packageName])) {
                continue;
            }

            if (!isset($installed['versions'][$packageName]['pretty_version'])) {
                return null;
            }

            return $installed['versions'][$packageName]['pretty_version'];
        }

        throw new \OutOfBoundsException('Package "' . $packageName . '" is not installed');
    }

    /**
     * @param  string      $packageName
     * @return string|null If the package is being replaced or provided but is not really installed, null will be returned as reference
     */
    public static function getReference($packageName)
    {
        foreach (self::getInstalled() as $installed) {
            if (!isset($installed['versions'][$packageName])) {
                continue;
            }

            if (!isset($installed['versions'][$packageName]['reference'])) {
                return null;
            }

            return $installed['versions'][$packageName]['reference'];
        }

        throw new \OutOfBoundsException('Package "' . $packageName . '" is not installed');
    }

    /**
     * @return array
     * @psalm-return array{name: string, version: string, reference: string, pretty_version: string, aliases: string[]}
     */
    public static function getRootPackage()
    {
        $installed = self::getInstalled();

        return $installed[0]['root'];
    }

    /**
     * Returns the raw installed.php data for custom implementations
     *
     * @return array[]
     * @psalm-return array{root: array{name: string, version: string, reference: string, pretty_version: string, aliases: string[]}, versions: array<string, array{pretty_version?: string, version?: string, aliases?: string[], reference?: string, replaced?: string[], provided?: string[]}>}
     */
    public static function getRawData()
    {
        return self::$installed;
    }

    /**
     * Lets you reload the static array from another file
     *
     * This is only useful for complex integrations in which a project needs to use
     * this class but then also needs to execute another project's autoloader in process,
     * and wants to ensure both projects have access to their version of installed.php.
     *
     * A typical case would be PHPUnit, where it would need to make sure it reads all
     * the data it needs from this class, then call reload() with
     * `require $CWD/vendor/composer/installed.php` (or similar) as input to make sure
     * the project in which it runs can then also use this class safely, without
     * interference between PHPUnit's dependencies and the project's dependencies.
     *
     * @param  array[] $data A vendor/composer/installed.php data set
     * @return void
     *
     * @psalm-param array{root: array{name: string, version: string, reference: string, pretty_version: string, aliases: string[]}, versions: array<string, array{pretty_version?: string, version?: string, aliases?: string[], reference?: string, replaced?: string[], provided?: string[]}>} $data
     */
    public static function reload($data)
    {
        self::$installed = $data;
        self::$installedByVendor = array();
    }

    /**
     * @return array[]
     * @psalm-return list<array{root: array{name: string, version: string, reference: string, pretty_version: string, aliases: string[]}, versions: array<string, array{pretty_version?: string, version?: string, aliases?: string[], reference?: string, replaced?: string[], provided?: string[]}>}>
     */
    private static function getInstalled()
    {
        if (null === self::$canGetVendors) {
            self::$canGetVendors = method_exists('Composer\Autoload\ClassLoader', 'getRegisteredLoaders');
        }

        $installed = array();

        if (self::$canGetVendors) {
            foreach (ClassLoader::getRegisteredLoaders() as $vendorDir => $loader) {
                if (isset(self::$installedByVendor[$vendorDir])) {
                    $installed[] = self::$installedByVendor[$vendorDir];
                } elseif (is_file($vendorDir.'/composer/installed.php')) {
                    $installed[] = self::$installedByVendor[$vendorDir] = require $vendorDir.'/composer/installed.php';
                }
            }
        }

        $installed[] = self::$installed;

        return $installed;
    }
}
