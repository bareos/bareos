This directive specifies the format of the labels contained in this pool. The format directive is used as a sort of template to create new Volume names during automatic Volume labeling.

The format should be specified in double quotes (:file:`"`), and consists of letters, numbers and the special characters hyphen (:file:`-`), underscore (:file:`_`), colon (:file:`:`), and period (:file:`.`), which are the legal characters for a Volume name.

In addition, the format may contain a number of variable expansion characters which will be expanded by a complex algorithm allowing you to create Volume names of many different formats. In all cases, the expansion process must resolve to the set of characters noted above that are legal Volume names. Generally, these variable expansion characters begin with a dollar sign (:file:`$`) or a left bracket (:file:`[`). For more details on variable expansion, please see
:ref:`section-VariableExpansionVolumeLabels`.

If no variable expansion characters are found in the string, the Volume name will be formed from the format string appended with the a unique number that increases. If you do not remove volumes from the pool, this number should be the number of volumes plus one, but this is not guaranteed. The unique number will be edited as four digits with leading zeros. For example, with a :strong:`Label Format = "File-"`\ , the first volumes will be named **File-0001**,
**File-0002**, ...

In almost all cases, you should enclose the format specification (part after the equal sign) in double quotes (:file:`"`).

