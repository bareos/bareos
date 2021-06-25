<?php
/**
 * @see       https://github.com/zendframework/zend-inputfilter for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-inputfilter/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\InputFilter\FileInput\TestAsset;

use Zend\Validator\ValidatorInterface;

final class FileUploadMock implements ValidatorInterface
{
    public function isValid($value)
    {
        return true;
    }

    public function getMessages()
    {
        return [];
    }
}
