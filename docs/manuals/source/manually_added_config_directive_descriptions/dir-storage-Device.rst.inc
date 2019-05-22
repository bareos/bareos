If :config:option:`dir/job/Protocol`\  is not :strong:`NDMP_NATIVE` (default is :config:option:`dir/job/Protocol = Native`\ ), this directive refers to one or multiple :config:option:`sd/device/Name`\  or a single :config:option:`sd/autochanger/Name`\ .

If an Autochanger should be used, it had to refer to a configured :config:option:`sd/autochanger/Name`\ . In this case, also set :config:option:`dir/storage/AutoChanger = yes`\ .

Otherwise it refers to one or more configured :config:option:`sd/device/Name`\ , see :ref:`section-MultipleStorageDevices`.

This name is not the physical device name, but the logical device name as defined in the |sd| resource.

If :config:option:`dir/job/Protocol = NDMP_NATIVE`\ , it refers to tape devices on the NDMP Tape Agent, see :ref:`section-NdmpNative`.

