<?php

/**
 * @see       https://github.com/laminas/laminas-i18n for the canonical source repository
 * @copyright https://github.com/laminas/laminas-i18n/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-i18n/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\I18n\Validator;

use IntlException;
use Laminas\I18n\Exception as I18nException;
use Laminas\Stdlib\ArrayUtils;
use Laminas\Validator\AbstractValidator;
use Laminas\Validator\Exception;
use Locale;
use NumberFormatter;
use Traversable;

class IsInt extends AbstractValidator
{
    const INVALID = 'intInvalid';
    const NOT_INT = 'notInt';

    /**
     * @var array
     */
    protected $messageTemplates = [
        self::INVALID => "Invalid type given. String or integer expected",
        self::NOT_INT => "The input does not appear to be an integer",
    ];

    /**
     * Optional locale
     *
     * @var string|null
     */
    protected $locale;

    /**
     * Constructor for the integer validator
     *
     * @param  array|Traversable $options
     * @throws Exception\ExtensionNotLoadedException if ext/intl is not present
     */
    public function __construct($options = [])
    {
        if (!extension_loaded('intl')) {
            throw new I18nException\ExtensionNotLoadedException(sprintf(
                '%s component requires the intl PHP extension',
                __NAMESPACE__
            ));
        }

        if ($options instanceof Traversable) {
            $options = ArrayUtils::iteratorToArray($options);
        }

        if (array_key_exists('locale', $options)) {
            $this->setLocale($options['locale']);
        }

        parent::__construct($options);
    }

    /**
     * Returns the set locale
     */
    public function getLocale()
    {
        if (null === $this->locale) {
            $this->locale = Locale::getDefault();
        }
        return $this->locale;
    }

    /**
     * Sets the locale to use
     *
     * @param  string $locale
     * @return Int
     */
    public function setLocale($locale)
    {
        $this->locale = $locale;
        return $this;
    }

    /**
     * Returns true if and only if $value is a valid integer
     *
     * @param  string|int $value
     * @return bool
     * @throws Exception\InvalidArgumentException
     */
    public function isValid($value)
    {
        if (!is_string($value) && !is_int($value) && !is_float($value)) {
            $this->error(self::INVALID);
            return false;
        }

        if (is_int($value)) {
            return true;
        }

        $this->setValue($value);

        $locale = $this->getLocale();
        try {
            $format = new NumberFormatter($locale, NumberFormatter::DECIMAL);
            if (intl_is_failure($format->getErrorCode())) {
                throw new Exception\InvalidArgumentException("Invalid locale string given");
            }
        } catch (IntlException $intlException) {
            throw new Exception\InvalidArgumentException("Invalid locale string given", 0, $intlException);
        }

        try {
            $parsedInt = $format->parse($value, NumberFormatter::TYPE_INT64);
            if (intl_is_failure($format->getErrorCode())) {
                $this->error(self::NOT_INT);
                return false;
            }
        } catch (IntlException $intlException) {
            $this->error(self::NOT_INT);
            return false;
        }

        $decimalSep  = $format->getSymbol(NumberFormatter::DECIMAL_SEPARATOR_SYMBOL);
        $groupingSep = $format->getSymbol(NumberFormatter::GROUPING_SEPARATOR_SYMBOL);

        $valueFiltered = str_replace($groupingSep, '', $value);
        $valueFiltered = str_replace($decimalSep, '.', $valueFiltered);

        if (strval($parsedInt) !== $valueFiltered) {
            $this->error(self::NOT_INT);
            return false;
        }

        return true;
    }
}
