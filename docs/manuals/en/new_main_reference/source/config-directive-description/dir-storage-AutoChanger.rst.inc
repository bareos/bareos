When :config:option:`dir/storage/Device`\  refers to an Auto Changer (:config:option:`sd/device/Autochanger`\ ), this directive must be set to ``yes``.

If you specify ``yes``,

-  Volume management command like :strong:`label` or :strong:`add` will request a Autochanger Slot number.

-  Bareos will prefer Volumes, that are in a Auto Changer slot. If none of theses volumes can be used, even after recycling, pruning, ..., Bareos will search for any volume of the same :config:option:`dir/storage/MediaType`\  whether or not in the magazine.

Please consult the :ref:`AutochangersChapter` chapter for details.

