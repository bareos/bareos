This directive specifies the format of the labels contained in this pool. The format directive is used as a sort of template to create new Volume names during automatic Volume labeling.

The format should be specified in double quotes (:file:`"`), and consists of letters, numbers and the special characters hyphen (:file:`-`), underscore (:file:`_`), colon (:file:`:`), and period (:file:`.`), which are the legal characters for a Volume name.

In addition, the format may contain a number of variable expansion characters which will be expanded by a complex algorithm allowing you to create Volume names of many different formats. In all cases, the expansion process must resolve to the set of characters noted above that are legal Volume names. Generally, these variable expansion characters begin with a dollar sign (:file:`$`) or a left bracket (:file:`[`). For more details on variable expansion, please see
:ref:`section-VariableExpansionVolumeLabels`.

If no variable expansion characters are found in the string, then a strictly increasing number is appended to the label.
While the number is less than 10000, it is zero padded to four digits.
For example, with a :strong:`Label Format = "File-"`\ , the first volumes will be named **File-0001**,
**File-0002**, ..., **File-10000**, **File-10001**, ...
If you do not remove volumes from the pool, this number should be equal to the number of volumes plus one,
but this is not guaranteed.

In almost all cases, you should enclose the format specification (part after the equal sign) in double quotes (:file:`"`).
