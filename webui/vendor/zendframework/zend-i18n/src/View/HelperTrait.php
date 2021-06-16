<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace Zend\I18n\View;

use IntlDateFormatter;

// @codingStandardsIgnoreStart

/**
 * Helper trait for auto-completion of code in modern IDEs.
 *
 * The trait provides convenience methods for view helpers,
 * defined by the zend-i18n component. It is designed to be used
 * for type-hinting $this variable inside zend-view templates via doc blocks.
 *
 * The base class is PhpRenderer, followed by the helper trait from
 * the zend-i18n component. However, multiple helper traits from different
 * Zend Framework components can be chained afterwards.
 *
 * @example @var \Zend\View\Renderer\PhpRenderer|\Zend\I18n\View\HelperTrait $this
 *
 * @method string currencyFormat(float $number, string|null $currencyCode = null, bool|null $showDecimals = null, string|null $locale = null, string|null $pattern = null)
 * @method string dateFormat(\DateTime|int|array $date, int $dateType = IntlDateFormatter::NONE, int $timeType = IntlDateFormatter::NONE, string|null $locale = null, string|null $pattern = null)
 * @method string numberFormat(int|float $number, int|null $formatStyle = null, int|null $formatType = null, string|null $locale = null, int|null $decimals = null, array|null $textAttributes = null)
 * @method string plural(array|string $strings, int $number)
 * @method string translate(string $message, string|null $textDomain = null, string|null $locale = null)
 * @method string translatePlural(string $singular, string $plural, int $number, string|null $textDomain = null, string|null $locale = null)
 */
trait HelperTrait
{
}
// @codingStandardsIgnoreEnd
