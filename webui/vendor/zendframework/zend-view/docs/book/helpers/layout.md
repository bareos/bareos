# Layout

The `Layout` helper is used to get and set the template for the layout or to
retrieving the root view model.

## Basic Usage

### Change the Layout Template

If you're running a zend-mvc application then the layout template is set in the
configuration for the [`ViewManager`](https://docs.zendframework.com/zend-mvc/services/#viewmanager).

To change the layout template within a view script, call:

```php
$this->layout('layout/backend');
```

Or use the `setTemplate` method:

```php
$this->layout()->setTemplate('layout/backend');
```

### Set View Variable on Layout Model

The `Layout` helper can also retrieve the view model for the layout (root):

```php
/** @var \Zend\View\Model\ViewModel $rootViewModel */
$rootViewModel = $this->layout();
```

This offers the possibility to set variables for the layout script.

#### Set a Single Variable

```php
$this->layout()->setVariable('infoText', 'Some text for later');
```

Use in your layout script:

```php
if (isset($infoText)) {
    echo $infoText;
}
```

#### Set a Set of Variables

```php
$this->layout()->setVariables([
    'headerText' => '…',
    'footerText' => '…',
]);
```

More informations related to view models can be found in the
[quick start](https://docs.zendframework.com/zend-view/quick-start/).
