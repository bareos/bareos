# File Uploads

zend-form provides support for file uploading by using features from each of
[zend-inputfilter](https://github.com/zendframework/zend-inputfilter),
[zend-validator](https://github.com/zendframework/zend-validator),
[zend-filter](https://docs.zendframework.com/zend-filter), and
[zend-progressbar](https://github.com/zendframework/zend-progressbar). These
reusable framework components provide a convenient and secure way for handling
file uploads in your projects.

> ### Limited to POST uploads
>
> The file upload features described here are specifically for forms using the
> `POST` method.  zend-form does not currently provide specific support for
> handling uploads via the `PUT` method, but it is possible with vanilla PHP.
> See the [PUT Method Support](http://php.net/features.file-upload.put-method)
> in the PHP documentation for more information.

## Basic Example

Handling file uploads is *essentially* the same as how you would use `Zend\Form`
for form processing, but with some slight caveats that will be described below.

In this example we will:

- Define a **Form** for backend validation and filtering.
- Create a **view template** with a `<form>` containing a file input.
- Process the form within a **Controller action** (zend-mvc) or in a **Request Handler** (Expressive).

### The Form and InputFilter

Here we define a `Zend\Form\Element\File` input in a `Form` extension named
`UploadForm`.

```php
use Zend\Form\Element;
use Zend\Form\Form;

class UploadForm extends Form
{
    public function __construct($name = null, $options = [])
    {
        parent::__construct($name, $options);
        $this->addElements();
    }

    public function addElements()
    {
        // File Input
        $file = new Element\File('image-file');
        $file->setLabel('Avatar Image Upload');
        $file->setAttribute('id', 'image-file');

        $this->add($file);
    }
}
```

The `File` element provides several automated features:

- The form's `enctype` will automatically be set to `multipart/form-data` when
  the form's `prepare()` method is called.
- The file element's default input specification will create the correct `Input`
  type: [`Zend\InputFilter\FileInput`](https://docs.zendframework.com/zend-inputfilter/file-input/).
- The `FileInput` will automatically prepend an [UploadFile validator](https://docs.zendframework.com/zend-validator/validators/file/upload-file/)
  to securely validate that the file is actually an uploaded file, and to report
  any upload errors to the user.

### The View Template

In the view template, we render the `<form>`, a file input (with label and
errors), and a submit button.

```php
<!-- File: upload-form.phtml -->
<?php
    $form->prepare(); // The correct enctype is set here
    $fileElement = $form->get('image-file');
    echo $this->form()->openTag($form);
?>

    <div class="form-element">
        <?= $this->formLabel($fileElement) ?>
        <?= $this->formFile($fileElement) ?>
        <?= $this->formElementErrors($fileElement) ?>
    </div>

    <button>Submit</button>

<?= $this->form()->closeTag() ?>
```

When rendered, the HTML should look similar to:

```html
<form name="upload-form" id="upload-form" method="post" enctype="multipart/form-data">
    <div class="form-element">
        <label for="image-file">Avatar Image Upload</label>
        <input type="file" name="image-file" id="image-file">
    </div>

    <button>Submit</button>
</form>
```

### The Controller Action

When using zend-mvc, the final step will be to instantiate the `UploadForm` and
process any postbacks in a controller action.

The form processing in the controller action will be similar to normal forms,
*except* that you **must** merge the `$_FILES` information in the request with
the other post data.

```php
public function uploadFormAction()
{
    $form = new UploadForm('upload-form');

    $request = $this->getRequest();
    if ($request->isPost()) {
        // Make certain to merge the $_FILES info!
        $post = array_merge_recursive(
            $request->getPost()->toArray(),
            $request->getFiles()->toArray()
        );

        $form->setData($post);
        if ($form->isValid()) {
            $data = $form->getData();

            // Form is valid, save the form!
            return $this->redirect()->toRoute('upload-form/success');
        }
    }

    return ['form' => $form];
}
```

Upon a successful file upload, `$form->getData()` would return:

```php
array(1) {
    ["image-file"] => array(5) {
        ["name"]     => string(11) "myimage.png"
        ["type"]     => string(9)  "image/png"
        ["tmp_name"] => string(22) "/private/tmp/phpgRXd58"
        ["error"]    => int(0)
        ["size"]     => int(14908679)
    }
}
```

> ### Using nested array notation for uploads
>
> It is suggested that you always use the `Zend\Http\PhpEnvironment\Request`
> object to retrieve and merge the `$_FILES` information with the form, instead
> of using `$_FILES` directly,  due to how the file information is
> mapped in the `$_FILES` array:
>
> ```php
> // A $_FILES array with single input and multiple files:
> array(1) {
>     ["image-file"]=array(2) {
>         ["name"]=array(2) {
>             [0]=string(9)"file0.txt"
>             [1]=string(9)"file1.txt"
>         }
>         ["type"]=array(2) {
>             [0]=string(10)"text/plain"
>             [1]=string(10)"text/html"
>         }
>     }
> }
>
> // How Zend\Http\PhpEnvironment\Request remaps the $_FILES array:
> array(1) {
>     ["image-file"]=array(2) {
>         [0]=array(2) {
>             ["name"]=string(9)"file0.txt"
>             ["type"]=string(10)"text/plain"
>         },
>         [1]=array(2) {
>             ["name"]=string(9)"file1.txt"
>             ["type"]=string(10)"text/html"
>         }
>     }
> }
> ```
>
> [`Zend\InputFilter\FileInput`](https://docs.zendframework.com/zend-inputfilter/file-input/) expects the file data be in this
> re-mapped array format.
>
> Note: [PSR-7](http://www.php-fig.org/psr/psr-7/) also remaps the `$_FILES`
> array in this way.

### Expressive Request Handler

If you are using a [PSR-15](https://www.php-fig.org/psr/psr-15/) request handler
with [PSR-7](https://www.php-fig.org/psr/psr-7/) request payload, the final step
involves merging `$request->getParsedBody()` with
`$request->getUploadedFiles()`.

```php
public function handle(ServerRequestInterface $request) : ResponseInterface
{
    $form = new UploadForm('upload-form');

    if ($request->getMethod() === 'POST') {
        $post = array_merge_recursive(
            $request->getParsedBody(),
            $request->getUploadedFiles()
        );
        
        $form->setData($post);
        
        if ($form->isValid()) {
            $data = $form->getData();
            
            // Form is valid, save the form!
            
            return new RedirectResponse('upload-form/success');
        }
    }
    
    return new HtmlResponse(
        $this->template->render('app::page-template', [
            'form' => $form,
        ]);    
    );
}
```

Upon a successful file upload, `$form->getData()` would return array including
the file field name as a key, and a new instance of `UploadedFileInterface` as
its value.

> ### Further operations on the uploaded file
>
> After running `isValid()` on the form instance, you should no longer trust the
> `UploadedFileInterface` instance stored in the PSR-7 `$request` to perform further
> operations on the uploaded file. The file may be moved by one of the filters
> attached to form input, but since the request is immutable, the change will not
> be reflected in it. Therefore, after validation, always use the file
> information retrieved from `$form->getData()`, _not_ from
> `$request->getUploadedFiles()`.

## File Post-Redirect-Get Plugin

When using other standard form inputs (i.e. `text`, `checkbox`, `select`, etc.)
along with file inputs in a form, you can encounter a situation where some
inputs may become invalid and the user must re-select the file and re-upload.
PHP will delete uploaded files from the temporary directory at the end of the
request if it has not been moved away or renamed. Re-uploading a valid file each
time another form input is invalid is inefficient and annoying to users.

One strategy to get around this is to split the form into multiple forms. One
form for the file upload inputs and another for the other standard inputs.

When you cannot separate the forms, the [File Post-Redirect-Get
Plugin](https://docs.zendframework.com/zend-mvc-plugin-fileprg/) can be used to
manage the file inputs and save off valid uploads until the entire form is
valid.

Changing our earlier example to use the `fileprg()` plugin will require two
changes.

### RenameUpload filter

First, we need to add a `RenameUpload` filter to our form's file input, with
details on where the valid files should be stored:

```php
use Zend\InputFilter;
use Zend\Form\Element;
use Zend\Form\Form;

class UploadForm extends Form
{
    public function __construct($name = null, $options = [])
    {
        parent::__construct($name, $options);
        $this->addElements();
        $this->addInputFilter();
    }

    public function addElements()
    {
        // File Input
        $file = new Element\File('image-file');
        $file->setLabel('Avatar Image Upload')
        $file->setAttribute('id', 'image-file');

        $this->add($file);
    }

    public function addInputFilter()
    {
        $inputFilter = new InputFilter\InputFilter();

        // File Input
        $fileInput = new InputFilter\FileInput('image-file');
        $fileInput->setRequired(true);
        $fileInput->getFilterChain()->attachByName(
            'filerenameupload',
            [
                'target'    => './data/tmpuploads/avatar.png',
                'randomize' => true,
            ]
        );
        $inputFilter->add($fileInput);

        $this->setInputFilter($inputFilter);
    }
}
```

The `filerenameupload` options above would cause an uploaded file to be
renamed and moved to: `./data/tmpuploads/avatar_4b3403665fea6.png`.

See the [RenameUpload filter](http://docs.zendframework.com/zend-filter/file/#renameupload)
documentation for more information on its supported options.

> ### Further operations on the uploaded file
>
> If the file is coming in as a PSR-7 payload, the move operation will be
> performed on the passed `UploadedFileInterface` instance. Therefore, it will
> contain an expired stream and outdated target file name. After running this filter,
> _do not use_ the request object to get further details about the uploaded file;
> use the new instance of `UploadedFileInterface` returned from the filter
> invocation. 

### Call the fileprg plugin

Next, we need to update our controller action to use the `fileprg` plugin:

```php
public function uploadFormAction()
{
    $form     = new UploadForm('upload-form');
    $tempFile = null;

    $prg = $this->fileprg($form);

    if ($prg instanceof \Zend\Http\PhpEnvironment\Response) {
        return $prg; // Return PRG redirect response
    }

    if (is_array($prg)) {
        if ($form->isValid()) {
            $data = $form->getData();

            // Form is valid, save the form!
            return $this->redirect()->toRoute('upload-form/success');
        }

        // Form not valid, but file uploads might be valid...
        // Get the temporary file information to show the user in the view
        $fileErrors = $form->get('image-file')->getMessages();

        if (empty($fileErrors)) {
            $tempFile = $form->get('image-file')->getValue();
        }
    }

    return [
        'form'     => $form,
        'tempFile' => $tempFile,
    ];
}
```

Behind the scenes, the `FilePRG` plugin will:

- Run the Form's filters, namely the `RenameUpload` filter, to move the files
  out of temporary storage.
- Store the valid POST data in the session across requests.
- Change the `required` flag of any file inputs that had valid uploads to
  `false`. This is so that form re-submissions without uploads will not cause
  validation errors.

> ### User notifications
>
> In the case of a partially valid form, it is up to the developer whether to
> notify the user that files have been uploaded or not. For example, you may
> wish to hide the form input and/or display the file information. These things
> would be implementation details in the view or in a custom view helper. Just
> note that neither the `FilePRG` plugin nor the `formFile` view helper will do
> any automatic notifications or view changes when files have been successfully
> uploaded.

## HTML5 Multi-File Uploads

With HTML5, we are able to select multiple files from a single file input using
the `multiple` attribute. Not all [browsers support multiple file
uploads](http://caniuse.com/#feat=forms), but the file input will safely remain
a single file upload for those browsers that do not support the feature.

To enable multiple file uploads in zend-form, set the file element's
`multiple` attribute to true:

```php
use Zend\InputFilter;
use Zend\Form\Element;
use Zend\Form\Form;

class UploadForm extends Form
{
    public function __construct($name = null, $options = [])
    {
        parent::__construct($name, $options);
        $this->addElements();
        $this->addInputFilter();
    }

    public function addElements()
    {
        // File Input
        $file = new Element\File('image-file');
        $file->setLabel('Avatar Image Upload');
        $file->setAttribute('id', 'image-file');
        $file->setAttribute('multiple', true);  // Marking as multiple

        $this->add($file);
    }

    public function addInputFilter()
    {
        $inputFilter = new InputFilter\InputFilter();

        // File Input
        $fileInput = new InputFilter\FileInput('image-file');
        $fileInput->setRequired(true);

        // Define validators and filters as if only one file was being uploaded.
        // All files will be run through the same validators and filters
        // automatically.
        $fileInput->getValidatorChain()
            ->attachByName('filesize',      ['max' => 204800])
            ->attachByName('filemimetype',  ['mimeType' => 'image/png,image/x-png'])
            ->attachByName('fileimagesize', ['maxWidth' => 100, 'maxHeight' => 100]);

        // All files will be renamed, e.g.:
        //   ./data/tmpuploads/avatar_4b3403665fea6.png,
        //   ./data/tmpuploads/avatar_5c45147660fb7.png
        $fileInput->getFilterChain()->attachByName(
            'filerenameupload',
            [
                'target'    => './data/tmpuploads/avatar.png',
                'randomize' => true,
            ]
        );
        $inputFilter->add($fileInput);

        $this->setInputFilter($inputFilter);
    }
}
```

You do not need to do anything special with the validators and filters to
support multiple file uploads. All of the files that are uploaded will have the
same validators and filters run against them automatically (from logic within
`FileInput`). Define them as if a single file was being uploaded.

## Upload Progress

While pure client-based upload progress meters are starting to become available
with [HTML5's Progress Events](http://www.w3.org/TR/progress-events/), not all
browsers have [XMLHttpRequest level 2 support](http://caniuse.com/#feat=xhr2).
For upload progress to work in a greater number of browsers (IE9 and below), you
must use a server-side progress solution.

`Zend\ProgressBar\Upload` provides handlers that can give you the actual state
of a file upload in progress. To use this feature, you need to choose one of the
[Upload Progress Handlers](https://docs.zendframework.com/zend-progressbar/upload/)
(APC, uploadprogress, or Session) and ensure that your server setup has the
appropriate extension or feature enabled.

Verify your `php.ini` `session.upload` settings before you begin:

```ini
file_uploads = On
post_max_size = 50M
upload_max_filesize = 50M
session.upload_progress.enabled = On
session.upload_progress.freq =  "1%"
session.upload_progress.min_freq = "1"
; Also make certain 'upload_tmp_dir' is writable
```

When uploading a file with a form POST, you must also include the progress
identifier in a hidden input. The [file upload progress view helpers](helper/upload-progress-helpers.md)
provides a convenient way to add the hidden input based on your handler type.

```php
<!-- File: upload-form.phtml -->
<?php
    $fileElement = $form->get('image-file');
    $form->prepare();
    echo $this->form()->openTag($form);
    echo $this->formFileSessionProgress(); // Must come before the file input!
?>

    <div class="form-element">
        <?= $this->formLabel($fileElement) ?>
        <?= $this->formFile($fileElement) ?>
        <?= $this->formElementErrors($fileElement) ?>
    </div>

    <button>Submit</button>

<?= $this->form()->closeTag() ?>
```

When rendered, the HTML should look similar to:

```html
<form name="upload-form" id="upload-form" method="post" enctype="multipart/form-data">
    <input type="hidden" id="progress_key" name="PHP_SESSION_UPLOAD_PROGRESS" value="12345abcde">

    <div class="form-element">
        <label for="image-file">Avatar Image Upload</label>
        <input type="file" name="image-file" id="image-file">
    </div>

    <button>Submit</button>
</form>
```

There are a few different methods for getting progress information to the
browser (long vs. short polling). Here we will use short polling since it is
simpler and less taxing on server resources, though keep in mind it is not as
responsive as long polling.

When our form is submitted via AJAX, the browser will continuously poll the
server for upload progress.

The following is an example controller action which provides the progress
information:

```php
public function uploadProgressAction()
{
    $id = $this->params()->fromQuery('id', null);
    $progress = new \Zend\ProgressBar\Upload\SessionProgress();
    return new \Zend\View\Model\JsonModel($progress->getProgress($id));
}

// Returns JSON
//{
//    "total"    : 204800,
//    "current"  : 10240,
//    "rate"     : 1024,
//    "message"  : "10kB / 200kB",
//    "done"     : false
//}
```

> ### Performance overhead
>
> This is *not* the most efficient way of providing upload progress, since each
> polling request must go through the Zend Framework bootstrap process. A better
> example would be to use a standalone php file in the public folder that
> bypasses the MVC bootstrapping and only uses the essential `Zend\ProgressBar`
> adapters.

Back in our view template, we will add Javascript to perform the AJAX POST of
the form data, and to start a timeout interval for the progress polling. To keep
the example code relatively short, we are using the [jQuery Form plugin](https://github.com/malsup/form)
to do the AJAX form POST. If your project uses a different JavaScript framework
(or none at all), this will hopefully at least illustrate the necessary
high-level logic that would need to be performed.

```php
<?php // File: upload-form.phtml
      // ...after the form...
?>

<!-- Twitter Bootstrap progress bar styles:
     http://twitter.github.com/bootstrap/components.html#progress -->
<div id="progress" class="help-block">
    <div class="progress progress-info progress-striped">
        <div class="bar"></div>
    </div>
    <p></p>
</div>

<script src="https://ajax.googleapis.com/ajax/libs/jquery/1.8.3/jquery.min.js"></script>
<script src="/js/jquery.form.js"></script>
<script>
var progressInterval;

function getProgress() {
    // Poll our controller action with the progress id
    var url = '/upload-form/upload-progress?id=' + $('#progress_key').val();
    $.getJSON(url, function(data) {
        if (data.status && !data.status.done) {
            var value = Math.floor((data.status.current / data.status.total) * 100);
            showProgress(value, 'Uploading...');
        } else {
            showProgress(100, 'Complete!');
            clearInterval(progressInterval);
        }
    });
}

function startProgress() {
    showProgress(0, 'Starting upload...');
    progressInterval = setInterval(getProgress, 900);
}

function showProgress(amount, message) {
    $('#progress').show();
    $('#progress .bar').width(amount + '%');
    $('#progress > p').html(message);
    if (amount < 100) {
        $('#progress .progress')
            .addClass('progress-info active')
            .removeClass('progress-success');
    } else {
        $('#progress .progress')
            .removeClass('progress-info active')
            .addClass('progress-success');
    }
}

$(function() {
    // Register a 'submit' event listener on the form to perform the AJAX POST
    $('#upload-form').on('submit', function(e) {
        e.preventDefault();

        if ($('#image-file').val() == '') {
            // No files selected, abort
            return;
        }

        // Perform the submit
        //$.fn.ajaxSubmit.debug = true;
        $(this).ajaxSubmit({
            beforeSubmit: function(arr, $form, options) {
                // Notify backend that submit is via ajax
                arr.push({ name: "isAjax", value: "1" });
            },
            success: function (response, statusText, xhr, $form) {
                clearInterval(progressInterval);
                showProgress(100, 'Complete!');

                // TODO: You'll need to do some custom logic here to handle a successful
                // form post, and when the form is invalid with validation errors.
                if (response.status) {
                    // TODO: Do something with a successful form post, like redirect
                    // window.location.replace(response.redirect);
                } else {
                    // Clear the file input, otherwise the same file gets re-uploaded
                    // http://stackoverflow.com/a/1043969
                    var fileInput = $('#image-file');
                    fileInput.replaceWith( fileInput.val('').clone( true ) );

                    // TODO: Do something with these errors
                    // showErrors(response.formErrors);
                }
            },
            error: function(a, b, c) {
                // NOTE: This callback is *not* called when the form is invalid.
                // It is called when the browser is unable to initiate or complete the ajax submit.
                // You will need to handle validation errors in the 'success' callback.
                console.log(a, b, c);
            }
        });
        // Start the progress polling
        startProgress();
    });
});
</script>
```

And finally, our controller action can be modified to return form status and
validation messages in JSON format if we see the 'isAjax' post parameter (which
was set in the JavaScript just before submit):

```php
public function uploadFormAction()
{
    $form = new UploadForm('upload-form');

    $request = $this->getRequest();

    if (! $request->isPost()) {
        return ['form' => $form];
    }

    // Make certain to merge the files info!
    $post = array_merge_recursive(
        $request->getPost()->toArray(),
        $request->getFiles()->toArray()
    );

    $form->setData($post);
    if ($form->isValid()) {
        $data = $form->getData();

        // Form is valid, save the form!
        if (! empty($post['isAjax'])) {
            return new JsonModel(array(
                'status'   => true,
                'redirect' => $this->url()->fromRoute('upload-form/success'),
                'formData' => $data,
            ));
        }

        // Fallback for non-JS clients
        return $this->redirect()->toRoute('upload-form/success');
    }

    if (! empty($post['isAjax'])) {
        // Send back failure information via JSON
        return new JsonModel([
            'status'     => false,
            'formErrors' => $form->getMessages(),
            'formData'   => $form->getData(),
        ]);
    }

    return array('form' => $form);
}
```

## Additional Info

Related documentation:

- [Form File Element](element/file.md)
- [Form File View Helper](helper/form-file.md)
- [List of File Validators](https://docs.zendframework.com/zend-validator/validators/file/intro/)
- [List of File Filters](http://docs.zendframework.com/zend-filter/file/)
- [File Post-Redirect-Get Controller Plugin](https://docs.zendframework.com/zend-mvc-plugin-fileprg/)
- [Zend\InputFilter\FileInput](https://docs.zendframework.com/zend-inputfilter/file-input/)
- [Upload Progress Handlers](https://docs.zendframework.com/zend-progressbar/upload/)
- [Upload Progress View Helpers](helper/upload-progress-helpers.md)

External resources and blog posts from the community:

- [ZF2FileUploadExamples](https://github.com/cgmartin/ZF2FileUploadExamples) : A
  ZF2 module with several file upload examples.
