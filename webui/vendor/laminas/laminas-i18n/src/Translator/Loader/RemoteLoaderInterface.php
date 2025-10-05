<?php

/**
 * @see       https://github.com/laminas/laminas-i18n for the canonical source repository
 * @copyright https://github.com/laminas/laminas-i18n/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-i18n/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\I18n\Translator\Loader;

/**
 * Remote loader interface.
 */
interface RemoteLoaderInterface
{
    /**
     * Load translations from a remote source.
     *
     * @param  string $locale
     * @param  string $textDomain
     * @return \Laminas\I18n\Translator\TextDomain|null
     */
    public function load($locale, $textDomain);
}
