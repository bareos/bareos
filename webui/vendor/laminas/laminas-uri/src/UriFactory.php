<?php

/**
 * @see       https://github.com/laminas/laminas-uri for the canonical source repository
 * @copyright https://github.com/laminas/laminas-uri/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-uri/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Uri;

/**
 * URI Factory Class
 *
 * The URI factory can be used to generate URI objects from strings, using a
 * different URI subclass depending on the input URI scheme. New scheme-specific
 * classes can be registered using the registerScheme() method.
 *
 * Note that this class contains only static methods and should not be
 * instantiated
 */
abstract class UriFactory
{
    /**
     * Registered scheme-specific classes
     *
     * @var array
     */
    protected static $schemeClasses = [
        'http'   => 'Laminas\Uri\Http',
        'https'  => 'Laminas\Uri\Http',
        'mailto' => 'Laminas\Uri\Mailto',
        'file'   => 'Laminas\Uri\File',
        'urn'    => 'Laminas\Uri\Uri',
        'tag'    => 'Laminas\Uri\Uri',
    ];

    /**
     * Register a scheme-specific class to be used
     *
     * @param string $scheme
     * @param string $class
     */
    public static function registerScheme($scheme, $class)
    {
        $scheme = strtolower($scheme);
        static::$schemeClasses[$scheme] = $class;
    }

    /**
     * Unregister a scheme
     *
     * @param string $scheme
     */
    public static function unregisterScheme($scheme)
    {
        $scheme = strtolower($scheme);
        if (isset(static::$schemeClasses[$scheme])) {
            unset(static::$schemeClasses[$scheme]);
        }
    }

    /**
     * Get the class name for a registered scheme
     *
     * If provided scheme is not registered, will return NULL
     *
     * @param  string $scheme
     * @return string|null
     */
    public static function getRegisteredSchemeClass($scheme)
    {
        if (isset(static::$schemeClasses[$scheme])) {
            return static::$schemeClasses[$scheme];
        }

        return;
    }

    /**
     * Create a URI from a string
     *
     * @param  string $uriString
     * @param  string $defaultScheme
     * @throws Exception\InvalidArgumentException
     * @return \Laminas\Uri\Uri
     */
    public static function factory($uriString, $defaultScheme = null)
    {
        if (! is_string($uriString)) {
            throw new Exception\InvalidArgumentException(sprintf(
                'Expecting a string, received "%s"',
                (is_object($uriString) ? get_class($uriString) : gettype($uriString))
            ));
        }

        $uri    = new Uri($uriString);
        $scheme = strtolower($uri->getScheme());
        if (! $scheme && $defaultScheme) {
            $scheme = $defaultScheme;
        }

        if ($scheme && ! isset(static::$schemeClasses[$scheme])) {
            throw new Exception\InvalidArgumentException(sprintf(
                'no class registered for scheme "%s"',
                $scheme
            ));
        }
        if ($scheme && isset(static::$schemeClasses[$scheme])) {
            $class = static::$schemeClasses[$scheme];
            $uri = new $class($uri);
            if (! $uri instanceof UriInterface) {
                throw new Exception\InvalidArgumentException(
                    sprintf(
                        'class "%s" registered for scheme "%s" does not implement Laminas\Uri\UriInterface',
                        $class,
                        $scheme
                    )
                );
            }
        }

        return $uri;
    }
}
