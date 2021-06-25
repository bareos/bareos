# Optional Input Filters

> Available since version 2.8.0

Normally, input filters are _required_, which means that if you compose them as
a subset of another input filter (e.g., to validate a subset of a larger set of
data), and no data is provided for that item, or an empty set of data is
provided, then the input filter will consider the data invalid.

If you want to allow a set of data to be empty, you can use
`Zend\InputFilter\OptionalInputFilter`.

To illustrate this, let's consider a form where a user provides profile
information. The user can provide an optional "title" and a required "email",
and _optionally_ details about a project they lead, which will include the
project title and a URL, both of which are required if present.

First, let's create an `OptionalInputFilter` instance for the project data:

```php
$projectFilter = new OptionalInputFilter();
$projectFilter->add([
    'name' => 'project_name',
    'required' => true,
]);
$projectFilter->add([
    'name' => 'url',
    'required' => true,
    'validators' => [
        ['type' => 'uri'],
    ],
]);
```

Now, we'll create our primary input filter:

```php
$profileFilter = new InputFilter();
$profileFilter->add([
    'name' => 'title',
    'required' => false,
]);
$profileFilter->add([
    'name' => 'email',
    'required' => true,
    'validators' => [
        ['type' => 'EmailAddress'],
    ],
]);

// And, finally, compose our project sub-filter:
$profileFilter->add($projectFilter, 'project');
```

With this defined, we can now validate the following sets of data, presented in
JSON for readability:

- Just profile information:

  ```json
  {
    "email": "user@example.com",
    "title": "Software Developer"
  }
  ```

- `null` project provided:

  ```json
  {
    "email": "user@example.com",
    "title": "Software Developer",
    "project": null
  }
  ```

- Empty project provided:

  ```json
  {
    "email": "user@example.com",
    "title": "Software Developer",
    "project": {}
  }
  ```

- Valid project provided:

  ```json
  {
    "email": "user@example.com",
    "title": "Software Developer",
    "project": {
      "project_name": "zend-inputfilter",
      "url": "https://github.com/zend-inputfilter",
    }
  }
  ```
