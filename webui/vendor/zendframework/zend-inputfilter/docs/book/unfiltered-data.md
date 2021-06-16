# Unfiltered Data

> Available since version 2.10.0

On input filters, there are several methods for retrieving the data:

- `getValues()` will return all known values after filtering them.
- `getRawValues()` will return all known values with no filtering applied.
- `getUnknown()` returns the set of all unknown values (values with no
  corresponding input or input filter).

At times, particularly when working with collections, you may want access to the
complete set of original data provided to the input filter. This can be
accomplished by merging the sets returned by `getRawValues()` and
`getUnknown()` when working with normal input filters, but this approach breaks
down when working with collections.

Version 2.10.0 introduces a new interface, `Zend\InputFilter\UnfilteredDataInterface`,
for dealing with this situation. `Zend\InputFilter\BaseInputFilter`, which
forms the parent class for all shipped input filter implementations, implements
the interface, which consists of the following methods:

```php
interface UnfilteredDataInterface
{
    /**
     * @return array|object
     */
    public function getUnfilteredData()
    {
        return $this->unfilteredData;
    }

    /**
     * @param array|object $data
     * @return $this
     */
    public function setUnfilteredData($data)
    {
        $this->unfilteredData = $data;
        return $this;
    }
}
```

The `setUnfilteredData()` method is called by `setData()` with the full `$data`
provided to that method, ensuring that `getUnfilteredData()` will always provide
the original data with which the input filter was initialized, with no filtering
applied.
