<?php
/**
 * @see       https://github.com/zendframework/zend-loader for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-loader/blob/master/LICENSE.md New BSD License
 */

namespace Zend\Loader;

// Grab SplAutoloader interface
require_once __DIR__ . '/SplAutoloader.php';

/**
 * PSR-0 compliant autoloader
 *
 * Allows autoloading both namespaced and vendor-prefixed classes. Class
 * lookups are performed on the filesystem. If a class file for the referenced
 * class is not found, a PHP warning will be raised by include().
 */
class StandardAutoloader implements SplAutoloader
{
    const NS_SEPARATOR     = '\\';
    const PREFIX_SEPARATOR = '_';
    const LOAD_NS          = 'namespaces';
    const LOAD_PREFIX      = 'prefixes';
    const ACT_AS_FALLBACK  = 'fallback_autoloader';
    const AUTOREGISTER_ZF  = 'autoregister_zf';

    /**
     * @var array Namespace/directory pairs to search; ZF library added by default
     */
    protected $namespaces = [];

    /**
     * @var array Prefix/directory pairs to search
     */
    protected $prefixes = [];

    /**
     * @var bool Whether or not the autoloader should also act as a fallback autoloader
     */
    protected $fallbackAutoloaderFlag = false;

    /**
     * Constructor
     *
     * @param  null|array|\Traversable $options
     */
    public function __construct($options = null)
    {
        if (null !== $options) {
            $this->setOptions($options);
        }
    }

    /**
     * Configure autoloader
     *
     * Allows specifying both "namespace" and "prefix" pairs, using the
     * following structure:
     * <code>
     * array(
     *     'namespaces' => array(
     *         'Zend'     => '/path/to/Zend/library',
     *         'Doctrine' => '/path/to/Doctrine/library',
     *     ),
     *     'prefixes' => array(
     *         'Phly_'     => '/path/to/Phly/library',
     *     ),
     *     'fallback_autoloader' => true,
     * )
     * </code>
     *
     * @param  array|\Traversable $options
     * @throws Exception\InvalidArgumentException
     * @return StandardAutoloader
     */
    public function setOptions($options)
    {
        if (! is_array($options) && ! ($options instanceof \Traversable)) {
            require_once __DIR__ . '/Exception/InvalidArgumentException.php';
            throw new Exception\InvalidArgumentException('Options must be either an array or Traversable');
        }

        foreach ($options as $type => $pairs) {
            switch ($type) {
                case self::AUTOREGISTER_ZF:
                    if ($pairs) {
                        $this->registerNamespace('Zend', dirname(__DIR__));
                        $this->registerNamespace(
                            'ZendXml',
                            dirname(dirname((__DIR__))) . DIRECTORY_SEPARATOR .  'ZendXml'
                        );
                    }
                    break;
                case self::LOAD_NS:
                    if (is_array($pairs) || $pairs instanceof \Traversable) {
                        $this->registerNamespaces($pairs);
                    }
                    break;
                case self::LOAD_PREFIX:
                    if (is_array($pairs) || $pairs instanceof \Traversable) {
                        $this->registerPrefixes($pairs);
                    }
                    break;
                case self::ACT_AS_FALLBACK:
                    $this->setFallbackAutoloader($pairs);
                    break;
                default:
                    // ignore
            }
        }
        return $this;
    }

    /**
     * Set flag indicating fallback autoloader status
     *
     * @param  bool $flag
     * @return StandardAutoloader
     */
    public function setFallbackAutoloader($flag)
    {
        $this->fallbackAutoloaderFlag = (bool) $flag;
        return $this;
    }

    /**
     * Is this autoloader acting as a fallback autoloader?
     *
     * @return bool
     */
    public function isFallbackAutoloader()
    {
        return $this->fallbackAutoloaderFlag;
    }

    /**
     * Register a namespace/directory pair
     *
     * @param  string $namespace
     * @param  string $directory
     * @return StandardAutoloader
     */
    public function registerNamespace($namespace, $directory)
    {
        $namespace = rtrim($namespace, self::NS_SEPARATOR) . self::NS_SEPARATOR;
        $this->namespaces[$namespace] = $this->normalizeDirectory($directory);
        return $this;
    }

    /**
     * Register many namespace/directory pairs at once
     *
     * @param  array $namespaces
     * @throws Exception\InvalidArgumentException
     * @return StandardAutoloader
     */
    public function registerNamespaces($namespaces)
    {
        if (! is_array($namespaces) && ! $namespaces instanceof \Traversable) {
            require_once __DIR__ . '/Exception/InvalidArgumentException.php';
            throw new Exception\InvalidArgumentException('Namespace pairs must be either an array or Traversable');
        }

        foreach ($namespaces as $namespace => $directory) {
            $this->registerNamespace($namespace, $directory);
        }
        return $this;
    }

    /**
     * Register a prefix/directory pair
     *
     * @param  string $prefix
     * @param  string $directory
     * @return StandardAutoloader
     */
    public function registerPrefix($prefix, $directory)
    {
        $prefix = rtrim($prefix, self::PREFIX_SEPARATOR). self::PREFIX_SEPARATOR;
        $this->prefixes[$prefix] = $this->normalizeDirectory($directory);
        return $this;
    }

    /**
     * Register many namespace/directory pairs at once
     *
     * @param  array $prefixes
     * @throws Exception\InvalidArgumentException
     * @return StandardAutoloader
     */
    public function registerPrefixes($prefixes)
    {
        if (! is_array($prefixes) && ! $prefixes instanceof \Traversable) {
            require_once __DIR__ . '/Exception/InvalidArgumentException.php';
            throw new Exception\InvalidArgumentException('Prefix pairs must be either an array or Traversable');
        }

        foreach ($prefixes as $prefix => $directory) {
            $this->registerPrefix($prefix, $directory);
        }
        return $this;
    }

    /**
     * Defined by Autoloadable; autoload a class
     *
     * @param  string $class
     * @return false|string
     */
    public function autoload($class)
    {
        $isFallback = $this->isFallbackAutoloader();
        if (false !== strpos($class, self::NS_SEPARATOR)) {
            if ($this->loadClass($class, self::LOAD_NS)) {
                return $class;
            } elseif ($isFallback) {
                return $this->loadClass($class, self::ACT_AS_FALLBACK);
            }
            return false;
        }
        if (false !== strpos($class, self::PREFIX_SEPARATOR)) {
            if ($this->loadClass($class, self::LOAD_PREFIX)) {
                return $class;
            } elseif ($isFallback) {
                return $this->loadClass($class, self::ACT_AS_FALLBACK);
            }
            return false;
        }
        if ($isFallback) {
            return $this->loadClass($class, self::ACT_AS_FALLBACK);
        }
        return false;
    }

    /**
     * Register the autoloader with spl_autoload
     *
     * @return void
     */
    public function register()
    {
        spl_autoload_register([$this, 'autoload']);
    }

    /**
     * Transform the class name to a filename
     *
     * @param  string $class
     * @param  string $directory
     * @return string
     */
    protected function transformClassNameToFilename($class, $directory)
    {
        // $class may contain a namespace portion, in  which case we need
        // to preserve any underscores in that portion.
        $matches = [];
        preg_match('/(?P<namespace>.+\\\)?(?P<class>[^\\\]+$)/', $class, $matches);

        $class     = (isset($matches['class'])) ? $matches['class'] : '';
        $namespace = (isset($matches['namespace'])) ? $matches['namespace'] : '';

        return $directory
             . str_replace(self::NS_SEPARATOR, '/', $namespace)
             . str_replace(self::PREFIX_SEPARATOR, '/', $class)
             . '.php';
    }

    /**
     * Load a class, based on its type (namespaced or prefixed)
     *
     * @param  string $class
     * @param  string $type
     * @return bool|string
     * @throws Exception\InvalidArgumentException
     */
    protected function loadClass($class, $type)
    {
        if (! in_array($type, [self::LOAD_NS, self::LOAD_PREFIX, self::ACT_AS_FALLBACK])) {
            require_once __DIR__ . '/Exception/InvalidArgumentException.php';
            throw new Exception\InvalidArgumentException();
        }

        // Fallback autoloading
        if ($type === self::ACT_AS_FALLBACK) {
            // create filename
            $filename     = $this->transformClassNameToFilename($class, '');
            $resolvedName = stream_resolve_include_path($filename);
            if ($resolvedName !== false) {
                return include $resolvedName;
            }
            return false;
        }

        // Namespace and/or prefix autoloading
        foreach ($this->$type as $leader => $path) {
            if (0 === strpos($class, $leader)) {
                // Trim off leader (namespace or prefix)
                $trimmedClass = substr($class, strlen($leader));

                // create filename
                $filename = $this->transformClassNameToFilename($trimmedClass, $path);
                if (file_exists($filename)) {
                    return include $filename;
                }
            }
        }
        return false;
    }

    /**
     * Normalize the directory to include a trailing directory separator
     *
     * @param  string $directory
     * @return string
     */
    protected function normalizeDirectory($directory)
    {
        $last = $directory[strlen($directory) - 1];
        if (in_array($last, ['/', '\\'])) {
            $directory[strlen($directory) - 1] = DIRECTORY_SEPARATOR;
            return $directory;
        }
        $directory .= DIRECTORY_SEPARATOR;
        return $directory;
    }
}
