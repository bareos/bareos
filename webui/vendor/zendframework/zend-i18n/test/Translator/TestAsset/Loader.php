<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n\Translator\TestAsset;

use Zend\I18n\Translator\Loader\FileLoaderInterface;

/**
 * Test loader.
 */
class Loader implements FileLoaderInterface
{
    public $textDomain;

    /**
     * load(): defined by LoaderInterface.
     *
     * @see    LoaderInterface::load()
     * @param  string $filename
     * @param  string $locale
     * @return TextDomain|null
     */
    public function load($filename, $locale)
    {
        return $this->textDomain;
    }
}
