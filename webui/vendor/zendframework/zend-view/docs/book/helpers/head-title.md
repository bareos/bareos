# HeadTitle

The HTML `<title>` element is used to **provide a title for an HTML document**.
The `HeadTitle` helper allows you to programmatically create and store the title
for later retrieval and output.

The `HeadTitle` helper is a concrete implementation of the [Placeholder helper](placeholder.md).
It overrides the `toString()` method to enforce generating a `<title>` element,
and adds a `headTitle()` method for overwriting and aggregation of title
elements. The signature for that method is `headTitle($title, $setType = null)`;
by default, the value is appended to the stack (aggregating title segments) if
left at `null`, but you may also specify either 'PREPEND' (place at top of
stack) or 'SET' (overwrite stack).

Since setting the aggregating (attach) order on each call to `headTitle` can be
cumbersome, you can set a default attach order by calling
`setDefaultAttachOrder()` which is applied to all `headTitle()` calls unless you
explicitly pass a different attach order as the second parameter.

## Basic Usage

Specify a title tag in a view script, e.g.
`module/Album/view/album/album/index.phtml`:

```php
$this->headTitle('My albums');
```

Render the title in the layout script, e.g.
`module/Application/view/layout/layout.phtml`

```php
<?= $this->headTitle() ?>
```

Output:

```html
<title>My albums</title>
```

### Add the Website Name

A typical usage includes the website name in the title. Add the name and [set a
separator](#using-separator) in the layout script, e.g.
`module/Application/view/layout/layout.phtml`

```php
<?= $this->headTitle('Music')->setSeparator(' - ') ?>
```

Output:

```html
<title>My albums - Music</title>
```

## Set Content

The normal behaviour is to append the content to the title (container).

```php
$this->headTitle('My albums')
$this->headTitle('Music');

echo $this->headTitle(); // <title>My albumsMusic</title>
```

### Append Content

To explicitly append content, the second paramater `$setType` or the concrete
method `append()` of the helper can be used:

```php fct_label="Invoke Usage"
$this->headTitle('My albums')
$this->headTitle('Music', 'APPEND');

echo $this->headTitle(); // <title>My albumsMusic</title>
```

```php fct_label="Setter Usage"
$this->headTitle('My albums')
$this->headTitle()->append('Music');

echo $this->headTitle(); // <title>My albumsMusic</title>
```

The constant `Zend\View\Helper\Placeholder\Container\AbstractContainer::APPEND`
can also be used as value for the second parameter `$setType`.

### Prepend Content

To prepend content, the second paramater `$setType` or the concrete method
`prepend()` of the helper can be used:

```php fct_label="Invoke Usage"
$this->headTitle('My albums')
$this->headTitle('Music', 'PREPEND');

echo $this->headTitle(); // <title>MusicMy albums</title>
```

```php fct_label="Setter Usage"
$this->headTitle('My albums')
$this->headTitle()->prepend('Music');

echo $this->headTitle(); // <title>MusicMy albums</title>
```

The constant `Zend\View\Helper\Placeholder\Container\AbstractContainer::PREPEND`
can also be used as value for the second parameter `$setType`.

### Overwrite existing Content

To overwrite the entire content of title helper, the second parameter `$setType`
or the concrete method `set()` of the helper can be used:

```php fct_label="Invoke Usage"
$this->headTitle('My albums')
$this->headTitle('Music', 'SET');

echo $this->headTitle(); // <title>Music</title>
```

```php fct_label="Setter Usage"
$this->headTitle('My albums')
$this->headTitle()->set('Music');

echo $this->headTitle(); // <title>Music</title>
```

The constant `Zend\View\Helper\Placeholder\Container\AbstractContainer::SET`
can also be used as value for the second parameter `$setType`.

### Set a default Order to add Content

```php
$this->headTitle()->setDefaultAttachOrder('PREPEND');
$this->headTitle('My albums');
$this->headTitle('Music');

echo $this->headTitle(); // <title>MusicMy albums</title>
```

#### Get Current Value

To get the current value of this option, use the `getDefaultAttachOrder()`
method.

```php
$this->headTitle()->setDefaultAttachOrder('PREPEND');

echo $this->headTitle()->getDefaultAttachOrder(); // PREPEND
```

#### Default Value

The default value is
`Zend\View\Helper\Placeholder\Container\AbstractContainer::APPEND` which
corresponds to the value `APPEND`.

## Using Separator

```php
$this->headTitle()->setSeparator(' | ');
$this->headTitle('My albums');
$this->headTitle('Music');

echo $this->headTitle(); // <title>My albums | Music</title>
```

### Get Current Value

To get the current value of this option, use the `getSeparator()`
method.

```php
$this->headTitle()->setSeparator(' | ');

echo $this->headTitle()->getSeparator(); //  |
```

### Default Value

The default value is an empty `string` that means no extra content is added
between the titles on rendering.

## Add Prefix and Postfix

The content of the helper can be formatted with a prefix and a postfix.

```php
$this->headTitle('My albums')->setPrefix('Music: ')->setPostfix('𝄞');

echo $this->headTitle(); // <title>Music: My albums 𝄞</title>
```

More descriptions and another example of usage can be found at the
[`Placeholder` helper](placeholder.md#aggregate-content).

## Render without Tags

In case the title is needed without the `<title>` and `</title>` tags the
`renderTitle()` method can be used.

```php
echo $this->headTitle('My albums')->renderTitle(); // My albums
```
