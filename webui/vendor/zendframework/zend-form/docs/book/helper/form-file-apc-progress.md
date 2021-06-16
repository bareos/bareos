# FormFileApcProgress

The `FormFileApcProgress` view helper can be used to render a `<input
type="hidden" ...>` with a progress ID value used by the APC File Upload
Progress feature. The APC PHP module is required for this view helper to work.
Unlike other Form view helpers, the `FormFileSessionProgress` helper does not
accept an `Element` as a parameter.

An `id` attribute with a value of `"progress_key"` will automatically be added.

> ### Render early
>
> The view helper **must** be rendered *before* the file input in the form, or
> upload progress will not work correctly.

Best used with the [Zend\ProgressBar\Upload\ApcProgress](https://docs.zendframework.com/zend-progressbar/upload/#apc-progress-handler)
handler.

See the `apc.rfc1867` ini setting in the [APC Configuration](http://php.net/apc.configuration)
documentation for more information.

## Basic usage

```php
// Within your view...

echo $this->formFileApcProgress();
// Result: <input type="hidden" id="progress_key" name="APC_UPLOAD_PROGRESS" value="12345abcde">
```
