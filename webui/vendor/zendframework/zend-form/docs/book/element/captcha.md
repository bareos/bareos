# Captcha

`Zend\Form\Element\Captcha` can be used with forms where authenticated users are
not necessary, but you want to prevent spam submissions. It is paired with one
of the `Zend\Form\View\Helper\Captcha\*` view helpers that matches the type of
CAPTCHA adapter in use.

## Basic Usage

A CAPTCHA adapter must be attached in order for validation to be included in the
element's input filter specification. See the [zend-captcha documentation](https://docs.zendframework.com/zend-captcha/adapters/)
for more information on what adapters are available.

```php
use Zend\Captcha;
use Zend\Form\Element;
use Zend\Form\Form;

$captcha = new Element\Captcha('captcha');
$captcha->setCaptcha(new Captcha\Dumb());
$captcha->setLabel('Please verify you are human');

$form = new Form('my-form');
$form->add($captcha);
```

Here is an example using array notation:

```php
use Zend\Captcha;
use Zend\Form\Form;

$form = new Form('my-form');
$form->add([
    'type' => 'Zend\Form\Element\Captcha',
    'name' => 'captcha',
    'options' => [
        'label' => 'Please verify you are human',
        'captcha' => new Captcha\Dumb(),
    ],
]);
```

## Public Methods

The following methods are specific to the `Captcha` element; all other methods
defined by the [parent `Element` class](element.md#public-methods) are also
available.

Method signature                               | Description
---------------------------------------------- | -----------
`setCaptcha(array|Zend\Captcha\AdapterInterface $captcha) : void` | Set the CAPTCHA adapter for this element. If `$captcha` is an array, `Zend\Captcha\Factory::factory()` will be run to create the adapter from the array configuration.
`getCaptcha() : Zend\Captcha\AdapterInterface` | Return the CAPTCHA adapter for this element.
`getInputSpecification() : array`              | Returns a input filter specification, which includes a `Zend\Filter\StringTrim` filter, and a CAPTCHA validator.
