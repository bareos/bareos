<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Helper\TestAsset;

use Zend\I18n\Translator;

class ArrayTranslator implements Translator\Loader\FileLoaderInterface
{
    public $translations;

    public function load($filename, $locale)
    {
        $textDomain = new Translator\TextDomain($this->translations);
        return $textDomain;
    }
}
