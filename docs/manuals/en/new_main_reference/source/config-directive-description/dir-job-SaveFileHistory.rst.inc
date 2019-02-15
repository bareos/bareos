:index:`[TAG=NDMP->File History] <pair: NDMP; File History>` Allow disabling storing the file history, as this causes problems problems with some implementations of NDMP (out-of-order metadata).

With :config:option:`dir/job/FileHistorySize`\  the maximum number of files and directories inside a NDMP job can be configured.



.. warning::
   The File History is required to do a single file restore from NDMP backups. With this disabled, only full restores are possible.
