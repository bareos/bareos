<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace Zend\I18n;

use Zend\ServiceManager\Factory\InvokableFactory;

class ConfigProvider
{
    /**
     * Return general-purpose zend-i18n configuration.
     *
     * @return array
     */
    public function __invoke()
    {
        return [
            'dependencies' => $this->getDependencyConfig(),
            'filters'      => $this->getFilterConfig(),
            'validators'   => $this->getValidatorConfig(),
            'view_helpers' => $this->getViewHelperConfig(),
        ];
    }

    /**
     * Return application-level dependency configuration.
     *
     * @return array
     */
    public function getDependencyConfig()
    {
        return [
            'aliases' => [
                'TranslatorPluginManager' => Translator\LoaderPluginManager::class,
            ],
            'factories' => [
                Translator\TranslatorInterface::class => Translator\TranslatorServiceFactory::class,
                Translator\LoaderPluginManager::class => Translator\LoaderPluginManagerFactory::class,
            ],
        ];
    }

    /**
     * Return zend-filter configuration.
     *
     * @return array
     */
    public function getFilterConfig()
    {
        return [
            'aliases' => [
                'alnum'        => Filter\Alnum::class,
                'Alnum'        => Filter\Alnum::class,
                'alpha'        => Filter\Alpha::class,
                'Alpha'        => Filter\Alpha::class,
                'numberformat' => Filter\NumberFormat::class,
                'numberFormat' => Filter\NumberFormat::class,
                'NumberFormat' => Filter\NumberFormat::class,
                'numberparse'  => Filter\NumberParse::class,
                'numberParse'  => Filter\NumberParse::class,
                'NumberParse'  => Filter\NumberParse::class,
            ],
            'factories' => [
                Filter\Alnum::class        => InvokableFactory::class,
                Filter\Alpha::class        => InvokableFactory::class,
                Filter\NumberFormat::class => InvokableFactory::class,
                Filter\NumberParse::class  => InvokableFactory::class,
            ],
        ];
    }

    /**
     * Return zend-validator configuration.
     *
     * @return array
     */
    public function getValidatorConfig()
    {
        return [
            'aliases' => [
                'alnum'       => Validator\Alnum::class,
                'Alnum'       => Validator\Alnum::class,
                'alpha'       => Validator\Alpha::class,
                'Alpha'       => Validator\Alpha::class,
                'datetime'    => Validator\DateTime::class,
                'dateTime'    => Validator\DateTime::class,
                'DateTime'    => Validator\DateTime::class,
                'float'       => Validator\IsFloat::class,
                'Float'       => Validator\IsFloat::class,
                'int'         => Validator\IsInt::class,
                'Int'         => Validator\IsInt::class,
                'isfloat'     => Validator\IsFloat::class,
                'isFloat'     => Validator\IsFloat::class,
                'IsFloat'     => Validator\IsFloat::class,
                'isint'       => Validator\IsInt::class,
                'isInt'       => Validator\IsInt::class,
                'IsInt'       => Validator\IsInt::class,
                'phonenumber' => Validator\PhoneNumber::class,
                'phoneNumber' => Validator\PhoneNumber::class,
                'PhoneNumber' => Validator\PhoneNumber::class,
                'postcode'    => Validator\PostCode::class,
                'postCode'    => Validator\PostCode::class,
                'PostCode'    => Validator\PostCode::class,
            ],
            'factories' => [
                Validator\Alnum::class       => InvokableFactory::class,
                Validator\Alpha::class       => InvokableFactory::class,
                Validator\DateTime::class    => InvokableFactory::class,
                Validator\IsFloat::class     => InvokableFactory::class,
                Validator\IsInt::class       => InvokableFactory::class,
                Validator\PhoneNumber::class => InvokableFactory::class,
                Validator\PostCode::class    => InvokableFactory::class,
            ],
        ];
    }

    /**
     * Return zend-view helper configuration.
     *
     * Obsoletes View\HelperConfig.
     *
     * @return array
     */
    public function getViewHelperConfig()
    {
        return [
            'aliases' => [
                'currencyformat'  => View\Helper\CurrencyFormat::class,
                'currencyFormat'  => View\Helper\CurrencyFormat::class,
                'CurrencyFormat'  => View\Helper\CurrencyFormat::class,
                'dateformat'      => View\Helper\DateFormat::class,
                'dateFormat'      => View\Helper\DateFormat::class,
                'DateFormat'      => View\Helper\DateFormat::class,
                'numberformat'    => View\Helper\NumberFormat::class,
                'numberFormat'    => View\Helper\NumberFormat::class,
                'NumberFormat'    => View\Helper\NumberFormat::class,
                'plural'          => View\Helper\Plural::class,
                'Plural'          => View\Helper\Plural::class,
                'translate'       => View\Helper\Translate::class,
                'Translate'       => View\Helper\Translate::class,
                'translateplural' => View\Helper\TranslatePlural::class,
                'translatePlural' => View\Helper\TranslatePlural::class,
                'TranslatePlural' => View\Helper\TranslatePlural::class,
            ],
            'factories' => [
                View\Helper\CurrencyFormat::class  => InvokableFactory::class,
                View\Helper\DateFormat::class      => InvokableFactory::class,
                View\Helper\NumberFormat::class    => InvokableFactory::class,
                View\Helper\Plural::class          => InvokableFactory::class,
                View\Helper\Translate::class       => InvokableFactory::class,
                View\Helper\TranslatePlural::class => InvokableFactory::class,
            ],
        ];
    }
}
