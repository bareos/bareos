# File

`Zend\Form\Element\File` represents a form file input and
provides a default input specification with a type of
[FileInput](https://docs.zendframework.com/zend-inputfilter/file-input/)
(important for handling validators and filters correctly).
It is intended for use with the [FormFile](../helper/form-file.md) view helper.

## Basic Usage

This element automatically adds a `type` attribute of value `file`.  It will
also set the form's `enctype` to `multipart/form-data` during
`$form->prepare()`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

// Single file upload:
$file = new Element\File('file');
$file->setLabel('Single file input');

// HTML5 multiple file upload:
$multiFile = new Element\File('multi-file');
$multiFile->setLabel('Multi file input');
$multiFile->setAttribute('multiple', true);

$form = new Form('my-file');
$form->add($file);
$form->add($multiFile);
```
