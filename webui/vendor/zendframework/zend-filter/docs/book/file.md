# File Filters

zend-filter also comes with a set of classes for filtering file contents, and
performing file operations such as renaming.

> ## $_FILES
>
> All file filter `filter()` implementations support either a file path string
> *or* a `$_FILES` array as the supplied argument. When a `$_FILES` array is
> passed in, the `tmp_name` is used for the file path.

## Encrypt and Decrypt

These filters allow encrypting and decrypting file contents, and are derived
from the `Zend\Filter\Encrypt` and `Zend\Filter\Decrypt` filters. Only file reading and
writing operations are performed by the filer; encryption and decryption operations
are performed by the parent classes.

Usage:

```php
use Zend\Filter\File\Encrypt as EncryptFileFilter;
use Zend\Http\PhpEnvironment\Request;

$request = new Request();
$files   = $request->getFiles();
// i.e. $files['my-upload']['tmp_name'] === '/tmp/php5Wx0aJ'

$filter = new EncryptFileFilter([
    'adapter' => 'BlockCipher',
    'key' => '--our-super-secret-key--',
]);
$filter->filter($files['my-upload']);
```

In the above example, we pass options to our constructor in order to configure
the filter. We could instead use use setter methods to inject these options:

```php
use Zend\Filter\File\Encrypt as EncryptFileFilter;
use Zend\Http\PhpEnvironment\Request;

$request = new Request();
$files   = $request->getFiles();
// i.e. $files['my-upload']['tmp_name'] === '/tmp/php5Wx0aJ'

$filter = new EncryptFileFilter();
$filter->setAdapter('BlockCipher');
$filter->setKey('--our-super-secret-key--');
$filter->filter($files['my-upload']);
```

Check the [Encrypt and Decrypt filter documentation](/zend-filter/standard-filters/#encrypt-and-decrypt)
for more information about options and adapters.

## Lowercase

`Zend\Filter\File\Lowercase` can be used to convert all file contents to
lowercase.

### Supported Options

The following set of options are supported:

 - `encoding`: Set the encoding to use during conversion.

### Basic Usage

```php
use Zend\Filter\File\LowerCase;
use Zend\Http\PhpEnvironment\Request;

$request = new Request();
$files   = $request->getFiles();
// i.e. $files['my-upload']['tmp_name'] === '/tmp/php5Wx0aJ'

$filter = new LowerCase();
$filter->filter($files['my-upload']);
```

This example converts the contents of an uploaded file to lowercase.  After this
process, you can use the [Rename](#rename) or [RenameUpload](#renameupload)
filter to replace this file with your original file, or read directly from file.
But, don't forget, if you upload a file and send your `$_FILES` array to a
filter method, the `LowerCase` filter will only change the temporary file
(`tmp_name` index of array), not the original file. Let's check following
example:

```php
use Zend\Filter\File\LowerCase;
use Zend\Filter\File\Rename;
use Zend\Http\PhpEnvironment\Request;

$request = new Request();
$files   = $request->getFiles();
// i.e. $files['my-upload']['tmp_name'] === '/tmp/php5Wx0aJ'

$lowercaseFilter = new LowerCase();
$file = $lowercaseFilter->filter($files['userfile']);
$renameFilter = new Rename([
    'target'    => '/tmp/newfile.txt',
    'randomize' => true,
]);
$filename = $renameFilter->filter($file['tmp_name']);
```

With this example, the final, stored file on the server will have the lowercased
content.

If you want to use a specific encoding when converting file content, you should
specify the encoding when instantiating the `LowerCase` filter, or use the
`setEncoding` method to change it.

```php
use Zend\Filter\File\LowerCase;
use Zend\Http\PhpEnvironment\Request;

$request = new Request();
$files   = $request->getFiles();
// i.e. $files['my-upload']['tmp_name'] === '/tmp/php5Wx0aJ'

$filter = new LowerCase();
$filter->setEncoding('ISO-8859-1');
$filter->filter($files['my-upload']);
```

The `LowerCase` filter extends from the `StringToLower` filter; read the
[`StringToLower` documentation](/zend-filter/standard-filters/#stringtolower)
for more information about encoding and its exceptions.

## Rename

`Zend\Filter\File\Rename` can be used to rename a file and/or move a file to a new path.

### Supported Options

The following set of options are supported:

- `target` (string; default: `*`): Target filename or directory; the new name
  of the source file.
- `source` (string; default: `*`): Source filename or directory which will be
  renamed. Used to match the filtered file with an options set.
- `overwrite` (boolean; default: `false`): Shall existing files be overwritten?
  If the file is unable to be moved into the target path, a
  `Zend\Filter\Exception\RuntimeException` will be thrown.
- `randomize` (boolean; default: `false`): Shall target files have a random
  postfix attached? The random postfix will generated with `uniqid('_')` after
  the file name and before the extension. For example, `file.txt` might be
  randomized to `file_4b3403665fea6.txt`.

An array of option sets is also supported, where a single `Rename` filter
instance can filter several files using different options. The options used for
the filtered file will be matched from the `source` option in the options set.

### Usage Examples

Move all filtered files to a different directory:

```php
// 'target' option is assumed if param is a string
$filter = new \Zend\Filter\File\Rename('/tmp/');
echo $filter->filter('./myfile.txt');
// File has been moved to '/tmp/myfile.txt'
```

Rename all filtered files to a new name:

```php
$filter = new \Zend\Filter\File\Rename('/tmp/newfile.txt');
echo $filter->filter('./myfile.txt');
// File has been renamed to '/tmp/newfile.txt'
```

Move to a new path, and randomize file names:

```php
$filter = new \Zend\Filter\File\Rename([
    'target'    => '/tmp/newfile.txt',
    'randomize' => true,
]);
echo $filter->filter('./myfile.txt');
// File has been renamed to '/tmp/newfile_4b3403665fea6.txt'
```

Configure different options for several possible source files:

```php
$filter = new \Zend\Filter\File\Rename([
    [
        'source'    => 'fileA.txt'
        'target'    => '/dest1/newfileA.txt',
        'overwrite' => true,
    ],
    [
        'source'    => 'fileB.txt'
        'target'    => '/dest2/newfileB.txt',
        'randomize' => true,
    ],
]);
echo $filter->filter('fileA.txt');
// File has been renamed to '/dest1/newfileA.txt'
echo $filter->filter('fileB.txt');
// File has been renamed to '/dest2/newfileB_4b3403665fea6.txt'
```

### Public Methods

The `Rename` filter defines the following public methods in addition to `filter()`:
follows:

- `getFile() : array`: Returns the files to rename along with their new name and location.
- `setFile(string|array $options) : void`: Sets the file options for renaming.
  Removes any previously set file options.
- `addFile(string|array $options) : void`: Adds file options for renaming to
  the current list of file options.

## RenameUpload

`Zend\Filter\File\RenameUpload` can be used to rename or move an uploaded file to a new path.

### Supported Options

The following set of options are supported:

- `target` (string; default: `*`): Target directory or full filename path.
- `overwrite` (boolean; default: `false`): Shall existing files be overwritten?
  If the file is unable to be moved into the target path, a
  `Zend\Filter\Exception\RuntimeException` will be thrown.
- `randomize` (boolean; default: `false`): Shall target files have a random
  postfix attached? The random postfix will generated with `uniqid('_')` after
  the file name and before the extension. For example, `file.txt` might be
  randomized to `file_4b3403665fea6.txt`.
- `use_upload_name` (boolean; default: `false`): When true, this filter will
  use `$_FILES['name']` as the target filename. Otherwise, the default `target`
  rules and the `$_FILES['tmp_name']` will be used.
- `use_upload_extension` (boolean; default: `false`): When true, the uploaded
  file will maintains its original extension if not specified.  For example, if
  the uploaded file is `file.txt` and the target is `mynewfile`, the upload
  will be renamed to `mynewfile.txt`.
- `stream_factory` (`Psr\Http\Message\StreamFactoryInterface`; default: `null`):
  Required when passing a [PSR-7 UploadedFileInterface](https://www.php-fig.org/psr/psr-7/#36-psrhttpmessageuploadedfileinterface)
  to the filter; used to create a new stream representing the renamed file.
  (Since 2.9.0)
- `upload_file_factory` (`Psr\Http\Message\UploadedFileFactoryInterface`; default:
  `null`): Required when passing a [PSR-7 UploadedFileInterface](https://www.php-fig.org/psr/psr-7/#36-psrhttpmessageuploadedfileinterface)
  to the filter; used to create a new uploaded file representation of the
  renamed file.  (Since 2.9.0)

> #### Using the upload name is unsafe
>
> Be **very** careful when using the `use_upload_name` option. For instance,
> extremely bad things could happen if you were to allow uploaded `.php` files
> (or other CGI files) to be moved into the `DocumentRoot`.
>
> It is generally a better idea to supply an internal filename to avoid
> security risks.

`RenameUpload` does not support an array of options like the`Rename` filter.
When filtering HTML5 file uploads with the `multiple` attribute set, all files
will be filtered with the same option settings.

### Usage Examples

Move all filtered files to a different directory:

```php
use Zend\Http\PhpEnvironment\Request;

$request = new Request();
$files   = $request->getFiles();
// i.e. $files['my-upload']['tmp_name'] === '/tmp/php5Wx0aJ'
// i.e. $files['my-upload']['name'] === 'myfile.txt'

// 'target' option is assumed if param is a string
$filter = new \Zend\Filter\File\RenameUpload('./data/uploads/');
echo $filter->filter($files['my-upload']);
// File has been moved to './data/uploads/php5Wx0aJ'

// ... or retain the uploaded file name
$filter->setUseUploadName(true);
echo $filter->filter($files['my-upload']);
// File has been moved to './data/uploads/myfile.txt'
```

Rename all filtered files to a new name:

```php
use Zend\Http\PhpEnvironment\Request;

$request = new Request();
$files   = $request->getFiles();
// i.e. $files['my-upload']['tmp_name'] === '/tmp/php5Wx0aJ'

$filter = new \Zend\Filter\File\RenameUpload('./data/uploads/newfile.txt');
echo $filter->filter($files['my-upload']);
// File has been renamed to './data/uploads/newfile.txt'
```

Move to a new path and randomize file names:

```php
use Zend\Http\PhpEnvironment\Request;

$request = new Request();
$files   = $request->getFiles();
// i.e. $files['my-upload']['tmp_name'] === '/tmp/php5Wx0aJ'

$filter = new \Zend\Filter\File\RenameUpload([
    'target'    => './data/uploads/newfile.txt',
    'randomize' => true,
]);
echo $filter->filter($files['my-upload']);
// File has been renamed to './data/uploads/newfile_4b3403665fea6.txt'
```

Handle a PSR-7 uploaded file:

```php
use Psr\Http\Message\ServerRequestInterface;
use Psr\Http\Message\StreamFactoryInterface;
use Psr\Http\Message\UploadedFileFactoryInterface;
use Psr\Http\Message\UploadedFileInterface;
use Zend\Filter\File\RenameUpload;

$filter = new \Zend\Filter\File\RenameUpload([
    'target'              => './data/uploads/',
    'randomize'           => true,
    // @var StreamFactoryInterface $streamFactory
    'stream_factory'      => $streamFactory,
    // @var UploadedFileFactoryInterface $uploadedFileFactory
    'upload_file_factory' => $uploadedFileFactory,
]);

// @var ServerRequestInterface $request
foreach ($request->getUploadedFiles() as $uploadedFile) {
    // @var UploadedFileInterface $uploadedFile
    // @var UploadedFileInterface $movedFile
    $movedFile = $filter->filter($uploadedFile);
    echo $movedFile->getClientFilename();
    // File has been renamed to './data/uploads/newfile_4b3403665fea6.txt'
}
```

> ### PSR-7 support
>
> PSR-7/PSR-17 support has only been available since 2.9.0, and requires a valid
> [psr/http-factory-implementation](https://packagist.org/providers/psr/http-factory-implementation)
> in your application, as it relies on the stream and uploaded file factories in
> order to produce the final `UploadedFileInterface` artifact representing the
> filtered file.
>
> PSR-17 itself requires PHP 7, so your application will need to be running on
> PHP 7 in order to use this feature.
>
> [zendframework/zend-diactoros 2.0](https://docs.zendframework.com/zend-diactoros/)
> provides a PSR-17 implementation, but requires PHP 7.1. If you are still on
> PHP 7.0, either upgrade, or find a compatible psr/http-factory-implementation.

## Uppercase

`Zend\Filter\File\Uppercase` can be used to convert all file contents to
uppercase.

### Supported Options

The following set of options are supported:

 - `encoding`: Set the encoding to use during conversion.

### Basic Usage

```php
use Zend\Filter\File\UpperCase;
use Zend\Http\PhpEnvironment\Request;

$request = new Request();
$files   = $request->getFiles();
// i.e. $files['my-upload']['tmp_name'] === '/tmp/php5Wx0aJ'

$filter = new UpperCase();
$filter->filter($files['my-upload']);
```

See the documentation on the [`LowerCase`](#lowercase) filter, above, for more
information.
