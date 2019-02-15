Is Spool Attributes is disabled, the File attributes are sent by the Storage daemon to the Director as they are stored on tape. However, if you want to avoid the possibility that database updates will slow down writing to the tape, you may want to set the value to ``yes``, in which case the Storage daemon will buffer the File attributes and Storage coordinates to a temporary file in the Working Directory, then when writing the Job data to the tape is completed, the
attributes and storage coordinates will be sent to the Director.

NOTE: When :config:option:`dir/job/SpoolData`\  is set to yes, Spool Attributes is also automatically set to yes.

For details, see :ref:`section-spooling`.

