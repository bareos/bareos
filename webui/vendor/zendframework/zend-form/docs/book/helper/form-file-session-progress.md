# FormFileSessionProgress

The `FormFileSessionProgress` view helper can be used to render a `<input
type="hidden" ...>` which can be used by the PHP 5.4+ File Upload Session
Progress feature.Unlike other zend-form view helpers, the
`FormFileSessionProgress` helper does not accept an `Element` as a parameter.

An `id` attribute with a value of `"progress_key"` will automatically be added.

> ### Render early
>
> The view helper **must** be rendered *before* the file input in the form, or
> upload progress will not work correctly.

Best used with the [Zend\ProgressBar\Upload\SessionProgress](https://docs.zendframework.com/zend-progressbar/upload/#session-progress-handler)
handler.

See the [Session Upload Progress](http://php.net/session.upload-progress)
chapter in the PHP documentation for more information.

## Basic usage

```php
// Within your view...

echo $this->formFileSessionProgress();
// Result: <input type="hidden" id="progress_key" name="PHP_SESSION_UPLOAD_PROGRESS" value="12345abcde">
```
