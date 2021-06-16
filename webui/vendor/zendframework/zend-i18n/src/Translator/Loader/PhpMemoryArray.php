<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace Zend\I18n\Translator\Loader;

use Zend\I18n\Exception;
use Zend\I18n\Translator\Plural\Rule as PluralRule;
use Zend\I18n\Translator\TextDomain;

/**
 * PHP Memory array loader.
 */
class PhpMemoryArray implements RemoteLoaderInterface
{
    /**
     * @var array
     */
    protected $messages;

    public function __construct($messages)
    {
        $this->messages = $messages;
    }

    /**
     * Load translations from a remote source.
     *
     * @param  string $locale
     * @param  string $textDomain
     * @return TextDomain
     * @throws Exception\InvalidArgumentException
     */
    public function load($locale, $textDomain)
    {
        if (! is_array($this->messages)) {
            throw new Exception\InvalidArgumentException(
                sprintf('Expected an array, but received %s', gettype($this->messages))
            );
        }

        if (! isset($this->messages[$textDomain])) {
            throw new Exception\InvalidArgumentException(
                sprintf('Expected textdomain "%s" to be an array, but it is not set', $textDomain)
            );
        }

        if (! isset($this->messages[$textDomain][$locale])) {
            throw new Exception\InvalidArgumentException(
                sprintf('Expected locale "%s" to be an array, but it is not set', $locale)
            );
        }

        $textDomain = new TextDomain($this->messages[$textDomain][$locale]);

        if ($textDomain->offsetExists('')) {
            if (isset($textDomain['']['plural_forms'])) {
                $textDomain->setPluralRule(
                    PluralRule::fromString($textDomain['']['plural_forms'])
                );
            }

            unset($textDomain['']);
        }

        return $textDomain;
    }
}
