<?php

/**
 * @see       https://github.com/laminas/laminas-config for the canonical source repository
 * @copyright https://github.com/laminas/laminas-config/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-config/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Config\Reader;

use Laminas\Config\Exception;

/**
 * YAML config reader.
 */
class Yaml implements ReaderInterface
{
    /**
     * Directory of the YAML file
     *
     * @var string
     */
    protected $directory;

    /**
     * YAML decoder callback
     *
     * @var callable
     */
    protected $yamlDecoder;

    /**
     * Constructor
     *
     * @param callable $yamlDecoder
     */
    public function __construct($yamlDecoder = null)
    {
        if ($yamlDecoder !== null) {
            $this->setYamlDecoder($yamlDecoder);
        } else {
            if (function_exists('yaml_parse')) {
                $this->setYamlDecoder('yaml_parse');
            }
        }
    }

    /**
     * Set callback for decoding YAML
     *
     * @param  string|callable $yamlDecoder the decoder to set
     * @return Yaml
     * @throws Exception\RuntimeException
     */
    public function setYamlDecoder($yamlDecoder)
    {
        if (!is_callable($yamlDecoder)) {
            throw new Exception\RuntimeException(
                'Invalid parameter to setYamlDecoder() - must be callable'
            );
        }
        $this->yamlDecoder = $yamlDecoder;
        return $this;
    }

    /**
     * Get callback for decoding YAML
     *
     * @return callable
     */
    public function getYamlDecoder()
    {
        return $this->yamlDecoder;
    }

    /**
     * fromFile(): defined by Reader interface.
     *
     * @see    ReaderInterface::fromFile()
     * @param  string $filename
     * @return array
     * @throws Exception\RuntimeException
     */
    public function fromFile($filename)
    {
        if (!is_file($filename) || !is_readable($filename)) {
            throw new Exception\RuntimeException(sprintf(
                "File '%s' doesn't exist or not readable",
                $filename
            ));
        }

        if (null === $this->getYamlDecoder()) {
            throw new Exception\RuntimeException("You didn't specify a Yaml callback decoder");
        }

        $this->directory = dirname($filename);

        $config = call_user_func($this->getYamlDecoder(), file_get_contents($filename));
        if (null === $config) {
            throw new Exception\RuntimeException("Error parsing YAML data");
        }

        return $this->process($config);
    }

    /**
     * fromString(): defined by Reader interface.
     *
     * @see    ReaderInterface::fromString()
     * @param  string $string
     * @return array|bool
     * @throws Exception\RuntimeException
     */
    public function fromString($string)
    {
        if (null === $this->getYamlDecoder()) {
            throw new Exception\RuntimeException("You didn't specify a Yaml callback decoder");
        }
        if (empty($string)) {
            return [];
        }

        $this->directory = null;

        $config = call_user_func($this->getYamlDecoder(), $string);
        if (null === $config) {
            throw new Exception\RuntimeException("Error parsing YAML data");
        }

        return $this->process($config);
    }

    /**
     * Process the array for @include
     *
     * @param  array $data
     * @return array
     * @throws Exception\RuntimeException
     */
    protected function process(array $data)
    {
        foreach ($data as $key => $value) {
            if (is_array($value)) {
                $data[$key] = $this->process($value);
            }
            if (trim($key) === '@include') {
                if ($this->directory === null) {
                    throw new Exception\RuntimeException('Cannot process @include statement for a json string');
                }
                $reader = clone $this;
                unset($data[$key]);
                $data = array_replace_recursive($data, $reader->fromFile($this->directory . '/' . $value));
            }
        }
        return $data;
    }
}
