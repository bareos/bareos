This directive enables the compatible mode of the file daemon. In this mode the file daemon will try to be as compatible to a native Bacula file daemon as possible. Enabling this option means that certain new options available in Bareos cannot be used as they would lead to data not being able to be restored by a Native Bareos file daemon.

The default value for this directive was changed from yes to no since Bareos :index:`Version >= 15.2.0 <pair: bareos-15.2.0; Compatible = no>`.

When you want to use bareos-only features, the value of compatible must be no.

