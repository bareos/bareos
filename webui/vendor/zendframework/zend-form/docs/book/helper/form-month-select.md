# FormMonthSelect

The `FormMonthSelect` view helper renders two select elements, one with a list
of months, another with a list of years. It works in conjection with the
[MonthSelect](../element/month-select.md) element, which provides the data for
the selects, as well as validation around input provided by them.

## Basic usage

```php
use Zend\Form\Element;

$monthYear = new Element\MonthSelect('monthyear');
$monthYear->setLabel('Select a month and a year');
$monthYear->setMinYear(1986);

// Within your view...

echo $this->formMonthSelect($monthYear);
// Result:
// <select name="monthyear[month]"> ... </select>
// <select name="monthyear[year]"> ... </select>
```
