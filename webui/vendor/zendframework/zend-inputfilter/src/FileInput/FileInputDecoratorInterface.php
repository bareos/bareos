<?php
/**
 * @see       https://github.com/zendframework/zend-inputfilter for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-inputfilter/blob/master/LICENSE.md New BSD License
 */

namespace Zend\InputFilter\FileInput;

/**
 * FileInputInterface defines expected methods for filtering uploaded files.
 *
 * FileInput will consume instances of this interface when filtering files,
 * allowing it to switch between SAPI uploads and PSR-7 UploadedFileInterface
 * instances.
 */
interface FileInputDecoratorInterface
{
    /**
     * Checks if the raw input value is an empty file input eg: no file was uploaded
     *
     * @param $rawValue
     * @return bool
     */
    public static function isEmptyFileDecorator($rawValue);

    /**
     * @return mixed
     */
    public function getValue();

    /**
     * @param  mixed $context Extra "context" to provide the validator
     * @return bool
     */
    public function isValid($context = null);
}
