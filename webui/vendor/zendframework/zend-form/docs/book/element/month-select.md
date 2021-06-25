# Month Select

`Zend\Form\Element\MonthSelect` is meant to be paired with
[FormMonthSelect](../helper/form-month-select.md) view helper.  This element
creates two select elements, where the first one is populated with months and
the second is populated with years. By default, it sets 100 years in the past
for the year element, starting with the current year.

## Basic Usage

```php
use Zend\Form\Element;
use Zend\Form\Form;

$monthYear = new Element\MonthSelect('monthyear');
$monthYear->setLabel('Select a month and a year');
$monthYear->setMinYear(1986);

$form = new Form('dateselect');
$form->add($monthYear);
```

Using array notation:

```php
use Zend\Form\Element;
use Zend\Form\Form;

$form = new Form('dateselect');
$form->add([
    'type' => Element\MonthSelect::class,
    'name' => 'monthyear',
    'options' => [
        'label' => 'Select a month and a year',
        'min_year' => 1986,
    ],
]);
```

## Public Methods

The following methods are specific to the `MonthSelect` element; all other methods
defined by the [parent `Element` class](element.md#public-methods) are also
available.

Method signature                                    | Description
--------------------------------------------------- | -----------
`getMonthElement() : Zend\Form\Element\Select`      | Returns the [Select](select.md) element that is used for the months part.
`getYearElement() : Zend\Form\Element\Select`       | Returns the [Select](select.md) element that is used for the years part.
`setMonthAttributes(array $monthAttributes) : void` | Set attributes on the [Select](select.md) element that is used for the months part.
`getMonthAttributes() : array`                      | Get attributes on the [Select](select.md) element that is used for the months part.
`setYearAttributes(array $yearAttributes) : void`   | Set attributes on the [Select](select.md) element that is used for the years part.
`getYearAttributes() : array`                       | Get attributes on the [Select](select.md) element that is used for the years part.
`setMinYear(int $minYear) : void`                   | Set the minimum year.
`getMinYear() : int`                                | Get the minimum year.
`setMaxYear(int $maxYear) : void`                   | Set the maximum year.
`getMaxYear() : int`                                | Get the maximum year.
`setValue(mixed $value) : void`                     | Set the value for the `MonthSelect` element. If the value is an instance of PHP's `DateTime`, it will use the month and year values from that date. Otherwise, the value should be an associative array with the `month` key for the month value, and with the `year` key for the year value.
