<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\I18n\Filter;

use Locale;
use Traversable;

class Alnum extends AbstractLocale
{
    /**
     * @var array
     */
    protected $options = array(
        'locale'            => null,
        'allow_white_space' => false,
    );

    /**
     * Sets default option values for this instance
     *
     * @param array|Traversable|bool|null $allowWhiteSpaceOrOptions
     * @param string|null $locale
     */
    public function __construct($allowWhiteSpaceOrOptions = null, $locale = null)
    {
        parent::__construct();
        if ($allowWhiteSpaceOrOptions !== null) {
            if (static::isOptions($allowWhiteSpaceOrOptions)) {
                $this->setOptions($allowWhiteSpaceOrOptions);
            } else {
                $this->setAllowWhiteSpace($allowWhiteSpaceOrOptions);
                $this->setLocale($locale);
            }
        }
    }

    /**
     * Sets the allowWhiteSpace option
     *
     * @param  bool $flag
     * @return Alnum Provides a fluent interface
     */
    public function setAllowWhiteSpace($flag = true)
    {
        $this->options['allow_white_space'] = (bool) $flag;
        return $this;
    }

    /**
     * Whether white space is allowed
     *
     * @return bool
     */
    public function getAllowWhiteSpace()
    {
        return $this->options['allow_white_space'];
    }

    /**
     * Defined by Zend\Filter\FilterInterface
     *
     * Returns $value as string with all non-alphanumeric characters removed
     *
     * @param  string|array $value
     * @return string|array
     */
    public function filter($value)
    {
        if (!is_scalar($value) && !is_array($value)) {
            return $value;
        }

        $whiteSpace = $this->options['allow_white_space'] ? '\s' : '';
        $language   = Locale::getPrimaryLanguage($this->getLocale());

        if (!static::hasPcreUnicodeSupport()) {
            // POSIX named classes are not supported, use alternative a-zA-Z0-9 match
            $pattern = '/[^a-zA-Z0-9' . $whiteSpace . ']/';
        } elseif ($language == 'ja'|| $language == 'ko' || $language == 'zh') {
            // Use english alphabet
            $pattern = '/[^a-zA-Z0-9'  . $whiteSpace . ']/u';
        } else {
            // Use native language alphabet
            $pattern = '/[^\p{L}\p{N}' . $whiteSpace . ']/u';
        }

        return preg_replace($pattern, '', $value);
    }
}
