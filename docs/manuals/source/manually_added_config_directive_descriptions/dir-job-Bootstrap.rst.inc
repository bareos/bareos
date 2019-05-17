The Bootstrap directive specifies a bootstrap file that, if provided, will be used during Restore Jobs and is ignored in other Job types. The bootstrap file contains the list of tapes to be used in a restore Job as well as which files are to be restored. Specification of this directive is optional, and if specified, it is used only for a restore job. In addition, when running a Restore job from the console, this value can be changed.

If you use the :ref:`restore <bcommandRestore>` command in the Console program, to start a restore job, the bootstrap file will be created automatically from the files you select to be restored.

For additional details see :ref:`BootstrapChapter` chapter.

