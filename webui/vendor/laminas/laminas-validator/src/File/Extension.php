<?php

/**
 * @see       https://github.com/laminas/laminas-validator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-validator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-validator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Validator\File;

use Laminas\Stdlib\ArrayUtils;
use Laminas\Validator\AbstractValidator;
use Laminas\Validator\Exception;
use Traversable;

/**
 * Validator for the file extension of a file
 */
class Extension extends AbstractValidator
{
    /**
     * @const string Error constants
     */
    const FALSE_EXTENSION = 'fileExtensionFalse';
    const NOT_FOUND       = 'fileExtensionNotFound';

    /**
     * @var array Error message templates
     */
    protected $messageTemplates = [
        self::FALSE_EXTENSION => "File has an incorrect extension",
        self::NOT_FOUND       => "File is not readable or does not exist",
    ];

    /**
     * Options for this validator
     *
     * @var array
     */
    protected $options = [
        'case'      => false,   // Validate case sensitive
        'extension' => '',      // List of extensions
    ];

    /**
     * @var array Error message template variables
     */
    protected $messageVariables = [
        'extension' => ['options' => 'extension'],
    ];

    /**
     * Sets validator options
     *
     * @param  string|array|Traversable $options
     */
    public function __construct($options = null)
    {
        if ($options instanceof Traversable) {
            $options = ArrayUtils::iteratorToArray($options);
        }

        $case = null;
        if (1 < func_num_args()) {
            $case = func_get_arg(1);
        }

        if (is_array($options)) {
            if (isset($options['case'])) {
                $case = $options['case'];
                unset($options['case']);
            }

            if (! array_key_exists('extension', $options)) {
                $options = ['extension' => $options];
            }
        } else {
            $options = ['extension' => $options];
        }

        if ($case !== null) {
            $options['case'] = $case;
        }

        parent::__construct($options);
    }

    /**
     * Returns the case option
     *
     * @return bool
     */
    public function getCase()
    {
        return $this->options['case'];
    }

    /**
     * Sets the case to use
     *
     * @param  bool $case
     * @return self Provides a fluent interface
     */
    public function setCase($case)
    {
        $this->options['case'] = (bool) $case;
        return $this;
    }

    /**
     * Returns the set file extension
     *
     * @return array
     */
    public function getExtension()
    {
        $extension = explode(',', $this->options['extension']);

        return $extension;
    }

    /**
     * Sets the file extensions
     *
     * @param  string|array $extension The extensions to validate
     * @return self Provides a fluent interface
     */
    public function setExtension($extension)
    {
        $this->options['extension'] = null;
        $this->addExtension($extension);
        return $this;
    }

    /**
     * Adds the file extensions
     *
     * @param  string|array $extension The extensions to add for validation
     * @return self Provides a fluent interface
     */
    public function addExtension($extension)
    {
        $extensions = $this->getExtension();
        if (is_string($extension)) {
            $extension = explode(',', $extension);
        }

        foreach ($extension as $content) {
            if (empty($content) || ! is_string($content)) {
                continue;
            }

            $extensions[] = trim($content);
        }

        $extensions = array_unique($extensions);

        // Sanity check to ensure no empty values
        foreach ($extensions as $key => $ext) {
            if (empty($ext)) {
                unset($extensions[$key]);
            }
        }

        $this->options['extension'] = implode(',', $extensions);
        return $this;
    }

    /**
     * Returns true if and only if the file extension of $value is included in the
     * set extension list
     *
     * @param  string|array $value Real file to check for extension
     * @param  array        $file  File data from \Laminas\File\Transfer\Transfer (optional)
     * @return bool
     */
    public function isValid($value, $file = null)
    {
        if (is_string($value) && is_array($file)) {
            // Legacy Laminas\Transfer API support
            $filename = $file['name'];
            $file     = $file['tmp_name'];
        } elseif (is_array($value)) {
            if (! isset($value['tmp_name']) || ! isset($value['name'])) {
                throw new Exception\InvalidArgumentException(
                    'Value array must be in $_FILES format'
                );
            }
            $file     = $value['tmp_name'];
            $filename = $value['name'];
        } else {
            $file     = $value;
            $filename = basename($file);
        }
        $this->setValue($filename);

        // Is file readable ?
        if (empty($file) || false === is_readable($file)) {
            $this->error(self::NOT_FOUND);
            return false;
        }

        $extension  = substr($filename, strrpos($filename, '.') + 1);
        $extensions = $this->getExtension();

        if ($this->getCase() && (in_array($extension, $extensions))) {
            return true;
        } elseif (! $this->getCase()) {
            foreach ($extensions as $ext) {
                if (strtolower($ext) == strtolower($extension)) {
                    return true;
                }
            }
        }

        $this->error(self::FALSE_EXTENSION);
        return false;
    }
}
