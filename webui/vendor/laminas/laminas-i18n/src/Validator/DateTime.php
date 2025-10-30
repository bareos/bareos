<?php

/**
 * @see       https://github.com/laminas/laminas-i18n for the canonical source repository
 * @copyright https://github.com/laminas/laminas-i18n/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-i18n/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\I18n\Validator;

use IntlDateFormatter;
use IntlException;
use Laminas\I18n\Exception as I18nException;
use Laminas\Validator\AbstractValidator;
use Laminas\Validator\Exception as ValidatorException;
use Locale;
use Traversable;

class DateTime extends AbstractValidator
{
    const INVALID          = 'datetimeInvalid';
    const INVALID_DATETIME = 'datetimeInvalidDateTime';

    /**
     *
     * @var array
     */
    protected $messageTemplates = [
        self::INVALID          => "Invalid type given. String expected",
        self::INVALID_DATETIME => "The input does not appear to be a valid datetime",
    ];

    /**
     * Optional locale
     *
     * @var string|null
     */
    protected $locale;

    /**
     * @var int
     */
    protected $dateType;

    /**
     * @var int
     */
    protected $timeType;

    /**
     * Optional timezone
     *
     * @var string
     */
    protected $timezone;

    /**
     * @var string
     */
    protected $pattern;

    /**
     * @var int
     */
    protected $calendar;

    /**
     * @var IntlDateFormatter
     */
    protected $formatter;

    /**
     * Is the formatter invalidated
     * Invalidation occurs when immutable properties are changed
     *
     * @var bool
     */
    protected $invalidateFormatter = false;

    /**
     * Constructor for the Date validator
     *
     * @param array|Traversable $options
     * @throws I18nException\ExtensionNotLoadedException if ext/intl is not present
     */
    public function __construct($options = [])
    {
        if (!extension_loaded('intl')) {
            throw new I18nException\ExtensionNotLoadedException(
                sprintf('%s component requires the intl PHP extension', __NAMESPACE__)
            );
        }

        // Delaying initialization until we know ext/intl is available
        $this->dateType = IntlDateFormatter::NONE;
        $this->timeType = IntlDateFormatter::NONE;
        $this->calendar = IntlDateFormatter::GREGORIAN;

        parent::__construct($options);

        if (null === $this->locale) {
            $this->locale = Locale::getDefault();
        }
        if (null === $this->timezone) {
            $this->timezone = date_default_timezone_get();
        }
    }

    /**
     * Sets the calendar to be used by the IntlDateFormatter
     *
     * @param int|null $calendar
     * @return DateTime provides fluent interface
     */
    public function setCalendar($calendar)
    {
        $this->calendar = $calendar;

        return $this;
    }

    /**
     * Returns the calendar to by the IntlDateFormatter
     *
     * @return int
     */
    public function getCalendar()
    {
        return ($this->formatter && !$this->invalidateFormatter) ? $this->getIntlDateFormatter()->getCalendar() : $this->calendar;
    }

    /**
     * Sets the date format to be used by the IntlDateFormatter
     *
     * @param int|null $dateType
     * @return DateTime provides fluent interface
     */
    public function setDateType($dateType)
    {
        $this->dateType            = $dateType;
        $this->invalidateFormatter = true;

        return $this;
    }

    /**
     * Returns the date format used by the IntlDateFormatter
     *
     * @return int
     */
    public function getDateType()
    {
        return $this->dateType;
    }

    /**
     * Sets the pattern to be used by the IntlDateFormatter
     *
     * @param string|null $pattern
     * @return DateTime provides fluent interface
     */
    public function setPattern($pattern)
    {
        $this->pattern = $pattern;

        return $this;
    }

    /**
     * Returns the pattern used by the IntlDateFormatter
     *
     * @return string
     */
    public function getPattern()
    {
        return ($this->formatter && !$this->invalidateFormatter) ? $this->getIntlDateFormatter()->getPattern() : $this->pattern;
    }

    /**
     * Sets the time format to be used by the IntlDateFormatter
     *
     * @param int|null $timeType
     * @return DateTime provides fluent interface
     */
    public function setTimeType($timeType)
    {
        $this->timeType            = $timeType;
        $this->invalidateFormatter = true;

        return $this;
    }

    /**
     * Returns the time format used by the IntlDateFormatter
     *
     * @return int
     */
    public function getTimeType()
    {
        return $this->timeType;
    }

    /**
     * Sets the timezone to be used by the IntlDateFormatter
     *
     * @param string|null $timezone
     * @return DateTime provides fluent interface
     */
    public function setTimezone($timezone)
    {
        $this->timezone = $timezone;

        return $this;
    }

    /**
     * Returns the timezone used by the IntlDateFormatter or the system default if none given
     *
     * @return string
     */
    public function getTimezone()
    {
        return ($this->formatter && !$this->invalidateFormatter) ? $this->getIntlDateFormatter()->getTimeZoneId() : $this->timezone;
    }

    /**
     * Sets the locale to be used by the IntlDateFormatter
     *
     * @param string|null $locale
     * @return DateTime provides fluent interface
     */
    public function setLocale($locale)
    {
        $this->locale              = $locale;
        $this->invalidateFormatter = true;

        return $this;
    }

    /**
     * Returns the locale used by the IntlDateFormatter or the system default if none given
     *
     * @return string
     */
    public function getLocale()
    {
        return $this->locale;
    }

    /**
     * Returns true if and only if $value is a floating-point value
     *
     * @param  string $value
     * @return bool
     * @throws ValidatorException\InvalidArgumentException
     */
    public function isValid($value)
    {
        if (!is_string($value)) {
            $this->error(self::INVALID);

            return false;
        }

        $this->setValue($value);

        try {
            $formatter = $this->getIntlDateFormatter();

            if (intl_is_failure($formatter->getErrorCode())) {
                throw new ValidatorException\InvalidArgumentException($formatter->getErrorMessage());
            }
        } catch (IntlException $intlException) {
            throw new ValidatorException\InvalidArgumentException($intlException->getMessage(), 0, $intlException);
        }

        try {
            $timestamp = $formatter->parse($value);

            if (intl_is_failure($formatter->getErrorCode()) || $timestamp === false) {
                $this->error(self::INVALID_DATETIME);
                $this->invalidateFormatter = true;
                return false;
            }
        } catch (IntlException $intlException) {
            $this->error(self::INVALID_DATETIME);
            $this->invalidateFormatter = true;
            return false;
        }

        return true;
    }

    /**
     * Returns a non lenient configured IntlDateFormatter
     *
     * @return IntlDateFormatter
     */
    protected function getIntlDateFormatter()
    {
        if ($this->formatter === null || $this->invalidateFormatter) {
            $this->formatter = new IntlDateFormatter(
                $this->getLocale(),
                $this->getDateType(),
                $this->getTimeType(),
                $this->timezone,
                $this->calendar,
                $this->pattern
            );

            $this->formatter->setLenient(false);

            $this->setTimezone($this->formatter->getTimezone());
            $this->setCalendar($this->formatter->getCalendar());
            $this->setPattern($this->formatter->getPattern());

            $this->invalidateFormatter = false;
        }

        return $this->formatter;
    }
}
