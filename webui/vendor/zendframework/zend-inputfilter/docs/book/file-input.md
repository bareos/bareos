# File Upload Input

The `Zend\InputFilter\FileInput` class is a special `Input` type for uploaded
files found in the `$_FILES` array.

While `FileInput` uses the same interface as `Input`, it differs in a few ways:

1. It expects the raw value to be in a normalized `$_FILES` array format. See
   the [PSR-7 Uploaded files](http://www.php-fig.org/psr/psr-7/#uploaded-files)
   chapter for details on how to accomplish this.
   [Diactoros](https://docs.zendframework.com/zend-diactoros/) and
   [zend-http](https://docs.zendframework.com/zend-http/) can do this for you.

   Alternately, you may provide an array of PSR-7 uploaded file instances.

2. The validators are run **before** the filters (which is the opposite behavior
   of `Input`). This is so that any validation can be run
   prior to any filters that may rename/move/modify the file; we should not do
   those operations if the file upload was invalid!

3. Instead of adding a `NotEmpty` validator, it will (by default) automatically
   add a `Zend\Validator\File\UploadFile` validator.

The biggest thing to be concerned about is that if you are using a `<input
type="file">` element in your form, you will need to use the `FileInput`
**instead of** `Input` or else you will encounter issues.

## Basic Usage

Usage of `FileInput` is essentially the same as `Input`:

```php
use Zend\Http\PhpEnvironment\Request;
use Zend\Filter;
use Zend\InputFilter\InputFilter;
use Zend\InputFilter\Input;
use Zend\InputFilter\FileInput;
use Zend\Validator;

// Description text input
$description = new Input('description'); // Standard Input type

// Filters are run first w/ Input
$description
    ->getFilterChain()
    ->attach(new Filter\StringTrim());

// Validators are run second w/ Input
$description
    ->getValidatorChain()
    ->attach(new Validator\StringLength(['max' => 140]));

// File upload input
$file = new FileInput('file'); // Special File Input type

// Validators are run first w/ FileInput
$file
    ->getValidatorChain()
    ->attach(new Validator\File\UploadFile());

// Filters are run second w/ FileInput
$file
    ->getFilterChain()
    ->attach(new Filter\File\RenameUpload([
         'target'    => './data/tmpuploads/file',
         'randomize' => true,
    ]));

// Merge $_POST and $_FILES data together
$request  = new Request();
$postData = array_merge_recursive(
    $request->getPost()->toArray(),
    $request->getFiles()->toArray()
);

$inputFilter = new InputFilter();
$inputFilter
    ->add($description)
    ->add($file)
    ->setData($postData);

if ($inputFilter->isValid()) {           // FileInput validators are run, but not the filters...
    echo "The form is valid\n";
    $data = $inputFilter->getValues();   // This is when the FileInput filters are run.
} else {
    echo "The form is not valid\n";
    foreach ($inputFilter->getInvalidInput() as $error) {
        print_r ($error->getMessages());
    }
}
```

## PSR-7 Support

> Available since version 2.9.0

You may also pass an array of uploaded files from a [PSR-7 ServerRequestInterface](http://www.php-fig.org/psr/psr-7/#serverrequestinterface).

```php
use Psr\Http\Message\ServerRequestInterface;
use Zend\Filter;
use Zend\InputFilter\InputFilter;
use Zend\InputFilter\FileInput;
use Zend\Validator;

// File upload input
$file = new FileInput('file');
$file
    ->getValidatorChain()
    ->attach(new Validator\File\UploadFile());
$file
    ->getFilterChain()
    ->attach(new Filter\File\RenameUpload([
         'target'    => './data/tmpuploads/file',
         'randomize' => true,
    ]));

// Merge form and uploaded file data together
// Unlike the previous example, we get the form data from `getParsedBody()`, and
// the uploaded file data from `getUploadedFiles()`.
// @var ServerRequestInterface $request
$postData = array_merge_recursive(
    $request->getParsedBody(),
    $request->getUploadedFiles()
);

$inputFilter = new InputFilter();
$inputFilter
    ->add($file)
    ->setData($postData);

if ($inputFilter->isValid()) {           // FileInput validators are run, but not the filters...
    echo "The form is valid\n";
    $data = $inputFilter->getValues();   // This is when the FileInput filters are run.
} else {
    echo "The form is not valid\n";
    foreach ($inputFilter->getInvalidInput() as $error) {
        print_r ($error->getMessages());
    }
}
```
