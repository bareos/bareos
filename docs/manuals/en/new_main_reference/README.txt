

Automatic conversion of the old documentation into the new Sphinx rst format.

The conversion of the original textfiles works in two passes. The
files that are processed are listed in "destfiles.txt".

The script "convert_all_tex_files_to_rst.sh" converts all files to the new format.
The conversion hast two phases:

* **pre_conversion_changes.sh** changes things in the original tex files
  that cause problems for pandoc. These are mostly the self-created environments
  which are converted to "verbatim" enviroments.

* then pandoc is called to convert the .tex file to .rst files.

* **post_conversion_changes.sh** then converts the things that pandoc was not able to convert
  (:raw-latex:`xxxxx`) output to the correct restructured text structures.

The resulting rst files are then processed by sphinx.

As the developers guide is already converted to rst. these files are only copied over.
