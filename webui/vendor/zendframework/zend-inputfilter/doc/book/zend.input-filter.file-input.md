# File Upload Input

The `Zend\InputFilter\FileInput` class is a special `Input` type for uploaded files found in the
`$_FILES` array.

While `FileInput` uses the same interface as `Input`, it differs in a few ways:

1.  It expects the raw value to be in the `$_FILES` array format.
2.  The validators are run **before** the filters (which is the opposite behavior of `Input`). This
is so that any `is_uploaded_file()` validation can be run prior to any filters that may
rename/move/modify the file.
3.  Instead of adding a `NotEmpty` validator, it will (by default) automatically add a
`Zend\Validator\File\UploadFile` validator.

The biggest thing to be concerned about is that if you are using a `<input type="file">` element in
your form, you will need to use the `FileInput` **instead of** `Input` or else you will encounter
issues.

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
$description->getFilterChain()           // Filters are run first w/ Input
            ->attach(new Filter\StringTrim());
$description->getValidatorChain()        // Validators are run second w/ Input
            ->attach(new Validator\StringLength(array('max' => 140)));

// File upload input
$file = new FileInput('file');           // Special File Input type
$file->getValidatorChain()               // Validators are run first w/ FileInput
     ->attach(new Validator\File\UploadFile());
$file->getFilterChain()                  // Filters are run second w/ FileInput
     ->attach(new Filter\File\RenameUpload(array(
         'target'    => './data/tmpuploads/file',
         'randomize' => true,
     )));

// Merge $_POST and $_FILES data together
$request  = new Request();
$postData = array_merge_recursive($request->getPost()->toArray(), $request->getFiles()->toArray());

$inputFilter = new InputFilter();
$inputFilter->add($description)
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

Also see

- File filter classes&lt;zend.filter.file&gt;
- File validator classes&lt;zend.validator.file&gt;

