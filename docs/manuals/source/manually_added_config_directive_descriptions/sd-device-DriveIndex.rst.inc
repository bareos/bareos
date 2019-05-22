The Drive Index that you specify is passed to the :config:option:`sd/device/ChangerCommand`\ . By default, the Drive Index is zero, so if you have only one drive in your autochanger, everything will work normally. However, if you have multiple drives, you must specify multiple Bareos Device resources (one for each drive). The first Device should have the Drive Index set to 0, and the second Device Resource should contain a Drive Index set to 1, and so on. This will then
permit you to use two or more drives in your autochanger.

For details, refer to :ref:`section-MultipleDevices`.

