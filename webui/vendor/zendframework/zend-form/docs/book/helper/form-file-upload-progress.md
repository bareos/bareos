# FormFileUploadProgress

The `FormFileUploadProgress` view helper can be used to render an `<input
type="hidden" ...>` containing the upload progress token, and which can be used by
the PECL uploadprogress extension. Unlike other zend-form view helpers, the
`FormFileUploadProgress` helper does not accept an `Element` as a parameter.

An `id` attribute with a value of `progress_key` will automatically be added.

> ### Render early
>
> The view helper **must** be rendered *before* the file input in the form, or
> upload progress will not work correctly.

This element should be used with the [Zend\ProgressBar\Upload\UploadProgress](https://docs.zendframework.com/zend-progressbar/upload/#upload-progress-handler)
handler.

See the [PECL uploadprogress extension](http://pecl.php.net/package/uploadprogress)
for more information.

## Basic usage

```php
// Within your view...

echo $this->formFileSessionProgress();
// Result <input type="hidden" id="progress_key" name="UPLOAD_IDENTIFIER" value="12345abcde">
```
