Some :config:option:`sd/device/DeviceType`\  require additional configuration. This can be specified in this directive, e.g. for

:ref:`SdBackendDroplet`
:ref:`SdBackendGfapi`
:ref:`SdBackendRados`

Before the Device Options directive have been introduced, these options have to be configured in the :config:option:`sd/device/ArchiveDevice`\  directive. This behavior have changed with :index:`Version >= 15.2.0 <pair: bareos-15.2.0; Device Options>`.

