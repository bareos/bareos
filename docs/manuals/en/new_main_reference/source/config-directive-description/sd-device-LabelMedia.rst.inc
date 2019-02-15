:index:`[TAG=Label->Label Media] <pair: Label; Label Media>` If Yes, permits this device to automatically label blank media without an explicit operator command. It does so by using an internal algorithm as defined on the :config:option:`dir/pool/LabelFormat`\  record in each Pool resource. If this is No as by default, Bareos will label tapes only by specific operator command (:strong:`label` in the Console) or when the tape has been recycled. The automatic labeling
feature is most useful when writing to disk rather than tape volumes.

