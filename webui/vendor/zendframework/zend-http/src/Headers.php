<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace Zend\Http;

use ArrayIterator;
use Countable;
use Iterator;
use Traversable;
use Zend\Http\Header\Exception;
use Zend\Http\Header\GenericHeader;
use Zend\Loader\PluginClassLocator;

/**
 * Basic HTTP headers collection functionality
 * Handles aggregation of headers
 *
 * @see        http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
 */
class Headers implements Countable, Iterator
{
    /**
     * @var PluginClassLocator
     */
    protected $pluginClassLoader;

    /**
     * @var array key names for $headers array
     */
    protected $headersKeys = [];

    /**
     * @var array Array of header array information or Header instances
     */
    protected $headers = [];

    /**
     * Populates headers from string representation
     *
     * Parses a string for headers, and aggregates them, in order, in the
     * current instance, primarily as strings until they are needed (they
     * will be lazy loaded)
     *
     * @param  string $string
     * @return Headers
     * @throws Exception\RuntimeException
     */
    public static function fromString($string)
    {
        $headers   = new static();
        $current   = [];
        $emptyLine = 0;

        // iterate the header lines, some might be continuations
        foreach (explode("\r\n", $string) as $line) {
            // CRLF*2 is end of headers; an empty line by itself or between header lines
            // is an attempt at CRLF injection.
            if (preg_match('/^\s*$/', $line)) {
                // empty line indicates end of headers
                $emptyLine += 1;
                if ($emptyLine > 2) {
                    throw new Exception\RuntimeException('Malformed header detected');
                }
                continue;
            }

            if ($emptyLine) {
                throw new Exception\RuntimeException('Malformed header detected');
            }

            // check if a header name is present
            if (preg_match('/^(?P<name>[^()><@,;:\"\\/\[\]?={} \t]+):.*$/', $line, $matches)) {
                if ($current) {
                    // a header name was present, then store the current complete line
                    $headers->headersKeys[] = static::createKey($current['name']);
                    $headers->headers[]     = $current;
                }
                $current = [
                    'name' => $matches['name'],
                    'line' => trim($line),
                ];

                continue;
            }

            if (preg_match("/^[ \t][^\r\n]*$/", $line, $matches)) {
                // continuation: append to current line
                $current['line'] .= trim($line);
                continue;
            }

            // Line does not match header format!
            throw new Exception\RuntimeException(sprintf(
                'Line "%s" does not match header format!',
                $line
            ));
        }
        if ($current) {
            $headers->headersKeys[] = static::createKey($current['name']);
            $headers->headers[]     = $current;
        }
        return $headers;
    }

    /**
     * Set an alternate implementation for the PluginClassLoader
     *
     * @param \Zend\Loader\PluginClassLocator $pluginClassLoader
     * @return Headers
     */
    public function setPluginClassLoader(PluginClassLocator $pluginClassLoader)
    {
        $this->pluginClassLoader = $pluginClassLoader;
        return $this;
    }

    /**
     * Return an instance of a PluginClassLocator, lazyload and inject map if necessary
     *
     * @return PluginClassLocator
     */
    public function getPluginClassLoader()
    {
        if ($this->pluginClassLoader === null) {
            $this->pluginClassLoader = new HeaderLoader();
        }
        return $this->pluginClassLoader;
    }

    /**
     * Add many headers at once
     *
     * Expects an array (or Traversable object) of type/value pairs.
     *
     * @param  array|Traversable $headers
     * @return Headers
     * @throws Exception\InvalidArgumentException
     */
    public function addHeaders($headers)
    {
        if (! is_array($headers) && ! $headers instanceof Traversable) {
            throw new Exception\InvalidArgumentException(sprintf(
                'Expected array or Traversable; received "%s"',
                (is_object($headers) ? get_class($headers) : gettype($headers))
            ));
        }

        foreach ($headers as $name => $value) {
            if (is_int($name)) {
                if (is_string($value)) {
                    $this->addHeaderLine($value);
                } elseif (is_array($value) && count($value) == 1) {
                    $this->addHeaderLine(key($value), current($value));
                } elseif (is_array($value) && count($value) == 2) {
                    $this->addHeaderLine($value[0], $value[1]);
                } elseif ($value instanceof Header\HeaderInterface) {
                    $this->addHeader($value);
                }
            } elseif (is_string($name)) {
                $this->addHeaderLine($name, $value);
            }
        }

        return $this;
    }

    /**
     * Add a raw header line, either in name => value, or as a single string 'name: value'
     *
     * This method allows for lazy-loading in that the parsing and instantiation of Header object
     * will be delayed until they are retrieved by either get() or current()
     *
     * @throws Exception\InvalidArgumentException
     * @param string $headerFieldNameOrLine
     * @param string $fieldValue optional
     * @return Headers
     */
    public function addHeaderLine($headerFieldNameOrLine, $fieldValue = null)
    {
        $matches = null;
        if (preg_match('/^(?P<name>[^()><@,;:\"\\/\[\]?=}{ \t]+):.*$/', $headerFieldNameOrLine, $matches)
            && $fieldValue === null) {
            // is a header
            $headerName = $matches['name'];
            $headerKey  = static::createKey($matches['name']);
            $line = $headerFieldNameOrLine;
        } elseif ($fieldValue === null) {
            throw new Exception\InvalidArgumentException('A field name was provided without a field value');
        } else {
            $headerName = $headerFieldNameOrLine;
            $headerKey  = static::createKey($headerFieldNameOrLine);
            if (is_array($fieldValue)) {
                $fieldValue = implode(', ', $fieldValue);
            }
            $line = $headerFieldNameOrLine . ': ' . $fieldValue;
        }

        $this->headersKeys[] = $headerKey;
        $this->headers[]     = ['name' => $headerName, 'line' => $line];

        return $this;
    }

    /**
     * Add a Header to this container, for raw values @see addHeaderLine() and addHeaders()
     *
     * @param  Header\HeaderInterface $header
     * @return Headers
     */
    public function addHeader(Header\HeaderInterface $header)
    {
        $key = static::createKey($header->getFieldName());
        $index = array_search($key, $this->headersKeys);

        // No header by that key presently; append key and header to list.
        if ($index === false) {
            $this->headersKeys[] = $key;
            $this->headers[]     = $header;
            return $this;
        }

        // Header exists, and is a multi-value header; append key and header to
        // list (as multi-value headers are aggregated on retrieval)
        $class = ($this->getPluginClassLoader()->load(str_replace('-', '', $key))) ?: Header\GenericHeader::class;
        if (in_array(Header\MultipleHeaderInterface::class, class_implements($class, true))) {
            $this->headersKeys[] = $key;
            $this->headers[] = $header;
            return $this;
        }

        // Otherwise, we replace the current instance.
        $this->headers[$index] = $header;

        return $this;
    }

    /**
     * Remove a Header from the container
     *
     * @param Header\HeaderInterface $header
     * @return bool
     */
    public function removeHeader(Header\HeaderInterface $header)
    {
        $index = array_search($header, $this->headers, true);
        if ($index !== false) {
            unset($this->headersKeys[$index]);
            unset($this->headers[$index]);

            return true;
        }
        return false;
    }

    /**
     * Clear all headers
     *
     * Removes all headers from queue
     *
     * @return Headers
     */
    public function clearHeaders()
    {
        $this->headers = $this->headersKeys = [];
        return $this;
    }

    /**
     * Get all headers of a certain name/type
     *
     * @param  string $name
     * @return bool|Header\HeaderInterface|ArrayIterator
     */
    public function get($name)
    {
        $key = static::createKey($name);
        if (! $this->has($name)) {
            return false;
        }

        $class = ($this->getPluginClassLoader()->load(str_replace('-', '', $key))) ?: 'Zend\Http\Header\GenericHeader';

        if (in_array('Zend\Http\Header\MultipleHeaderInterface', class_implements($class, true))) {
            $headers = [];
            foreach (array_keys($this->headersKeys, $key) as $index) {
                if (is_array($this->headers[$index])) {
                    $this->lazyLoadHeader($index);
                }
            }
            foreach (array_keys($this->headersKeys, $key) as $index) {
                $headers[] = $this->headers[$index];
            }
            return new ArrayIterator($headers);
        }

        $index = array_search($key, $this->headersKeys);
        if ($index === false) {
            return false;
        }

        if (is_array($this->headers[$index])) {
            return $this->lazyLoadHeader($index);
        }

        return $this->headers[$index];
    }

    /**
     * Test for existence of a type of header
     *
     * @param  string $name
     * @return bool
     */
    public function has($name)
    {
        return in_array(static::createKey($name), $this->headersKeys);
    }

    /**
     * Advance the pointer for this object as an iterator
     *
     * @return void
     */
    public function next()
    {
        next($this->headers);
    }

    /**
     * Return the current key for this object as an iterator
     *
     * @return mixed
     */
    public function key()
    {
        return (key($this->headers));
    }

    /**
     * Is this iterator still valid?
     *
     * @return bool
     */
    public function valid()
    {
        return (current($this->headers) !== false);
    }

    /**
     * Reset the internal pointer for this object as an iterator
     *
     * @return void
     */
    public function rewind()
    {
        reset($this->headers);
    }

    /**
     * Return the current value for this iterator, lazy loading it if need be
     *
     * @return array|Header\HeaderInterface
     */
    public function current()
    {
        $current = current($this->headers);
        if (is_array($current)) {
            $current = $this->lazyLoadHeader(key($this->headers));
        }
        return $current;
    }

    /**
     * Return the number of headers in this contain, if all headers have not been parsed, actual count could
     * increase if MultipleHeader objects exist in the Request/Response.  If you need an exact count, iterate
     *
     * @return int count of currently known headers
     */
    public function count()
    {
        return count($this->headers);
    }

    /**
     * Render all headers at once
     *
     * This method handles the normal iteration of headers; it is up to the
     * concrete classes to prepend with the appropriate status/request line.
     *
     * @return string
     */
    public function toString()
    {
        $headers = '';
        foreach ($this->toArray() as $fieldName => $fieldValue) {
            if (is_array($fieldValue)) {
                // Handle multi-value headers
                foreach ($fieldValue as $value) {
                    $headers .= $fieldName . ': ' . $value . "\r\n";
                }
                continue;
            }
            // Handle single-value headers
            $headers .= $fieldName . ': ' . $fieldValue . "\r\n";
        }
        return $headers;
    }

    /**
     * Return the headers container as an array
     *
     * @todo determine how to produce single line headers, if they are supported
     * @return array
     */
    public function toArray()
    {
        $headers = [];
        /* @var $header Header\HeaderInterface */
        foreach ($this->headers as $header) {
            if ($header instanceof Header\MultipleHeaderInterface) {
                $name = $header->getFieldName();
                if (! isset($headers[$name])) {
                    $headers[$name] = [];
                }
                $headers[$name][] = $header->getFieldValue();
            } elseif ($header instanceof Header\HeaderInterface) {
                $headers[$header->getFieldName()] = $header->getFieldValue();
            } else {
                $matches = null;
                preg_match('/^(?P<name>[^()><@,;:\"\\/\[\]?=}{ \t]+):\s*(?P<value>.*)$/', $header['line'], $matches);
                if ($matches) {
                    $headers[$matches['name']] = $matches['value'];
                }
            }
        }
        return $headers;
    }

    /**
     * By calling this, it will force parsing and loading of all headers, after this count() will be accurate
     *
     * @return bool
     */
    public function forceLoading()
    {
        foreach ($this as $item) {
            // $item should now be loaded
        }
        return true;
    }

    /**
     * @param $index
     * @param bool $isGeneric
     * @return mixed|void
     */
    protected function lazyLoadHeader($index, $isGeneric = false)
    {
        $current = $this->headers[$index];

        $key = $this->headersKeys[$index];
        /* @var $class Header\HeaderInterface */
        $class = $this->getPluginClassLoader()->load(str_replace('-', '', $key));
        if ($isGeneric || ! $class) {
            $class = GenericHeader::class;
        }

        try {
            $headers = $class::fromString($current['line']);
        } catch (Exception\InvalidArgumentException $exception) {
            return $this->lazyLoadHeader($index, true);
        }
        if (is_array($headers)) {
            $this->headers[$index] = $current = array_shift($headers);
            foreach ($headers as $header) {
                $this->headersKeys[] = $key;
                $this->headers[]     = $header;
            }
            return $current;
        }

        $this->headers[$index] = $current = $headers;
        return $current;
    }

    /**
     * Create array key from header name
     *
     * @param string $name
     * @return string
     */
    protected static function createKey($name)
    {
        return str_replace(['_', ' ', '.'], '-', strtolower($name));
    }
}
