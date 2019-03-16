.. code-block:: sql
    :caption: Optimize the performance of the SQL queries used by :bcommand:`.bvfs_lsdirs`

    CREATE INDEX PathId_JobId_FileIndex_FileNameId ON File(PathId,JobId,FileIndex,FilenameId);
