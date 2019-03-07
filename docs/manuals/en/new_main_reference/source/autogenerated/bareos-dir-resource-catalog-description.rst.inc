.. config:option:: dir/catalog/Address

   :type: STRING

   *This directive is an alias.*

   .. include:: /config-directive-description/dir-catalog-Address.rst.inc



.. config:option:: dir/catalog/DbAddress

   :type: STRING

   .. include:: /config-directive-description/dir-catalog-DbAddress.rst.inc



.. config:option:: dir/catalog/DbDriver

   :required: True
   :type: STRING

   .. include:: /config-directive-description/dir-catalog-DbDriver.rst.inc



.. config:option:: dir/catalog/DbName

   :required: True
   :type: STRING

   .. include:: /config-directive-description/dir-catalog-DbName.rst.inc



.. config:option:: dir/catalog/DbPassword

   :type: AUTOPASSWORD

   .. include:: /config-directive-description/dir-catalog-DbPassword.rst.inc



.. config:option:: dir/catalog/DbPort

   :type: PINT32

   .. include:: /config-directive-description/dir-catalog-DbPort.rst.inc



.. config:option:: dir/catalog/DbSocket

   :type: STRING

   .. include:: /config-directive-description/dir-catalog-DbSocket.rst.inc



.. config:option:: dir/catalog/DbUser

   :type: STRING

   .. include:: /config-directive-description/dir-catalog-DbUser.rst.inc



.. config:option:: dir/catalog/Description

   :type: STRING

   .. include:: /config-directive-description/dir-catalog-Description.rst.inc



.. config:option:: dir/catalog/DisableBatchInsert

   :type: BOOLEAN
   :default: no

   .. include:: /config-directive-description/dir-catalog-DisableBatchInsert.rst.inc



.. config:option:: dir/catalog/ExitOnFatal

   :type: BOOLEAN
   :default: no
   :version: 15.1.0

   Make any fatal error in the connection to the database exit the program

   .. include:: /config-directive-description/dir-catalog-ExitOnFatal.rst.inc



.. config:option:: dir/catalog/IdleTimeout

   :type: PINT32
   :default: 30

   This directive is used by the experimental database pooling functionality. Only use this for non production sites.  This sets the idle time after which a database pool should be shrinked.

   .. include:: /config-directive-description/dir-catalog-IdleTimeout.rst.inc



.. config:option:: dir/catalog/IncConnections

   :type: PINT32
   :default: 1

   This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the number of connections to add to a database pool when not enough connections are available on the pool anymore.

   .. include:: /config-directive-description/dir-catalog-IncConnections.rst.inc



.. config:option:: dir/catalog/MaxConnections

   :type: PINT32
   :default: 5

   This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the maximum number of connections to a database to keep in this database pool.

   .. include:: /config-directive-description/dir-catalog-MaxConnections.rst.inc



.. config:option:: dir/catalog/MinConnections

   :type: PINT32
   :default: 1

   This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the minimum number of connections to a database to keep in this database pool.

   .. include:: /config-directive-description/dir-catalog-MinConnections.rst.inc



.. config:option:: dir/catalog/MultipleConnections

   :type: BIT

   .. include:: /config-directive-description/dir-catalog-MultipleConnections.rst.inc



.. config:option:: dir/catalog/Name

   :required: True
   :type: NAME

   The name of the resource.

   .. include:: /config-directive-description/dir-catalog-Name.rst.inc



.. config:option:: dir/catalog/Password

   :type: AUTOPASSWORD

   *This directive is an alias.*

   .. include:: /config-directive-description/dir-catalog-Password.rst.inc



.. config:option:: dir/catalog/Reconnect

   :type: BOOLEAN
   :default: no
   :version: 15.1.0

   Try to reconnect a database connection when its dropped

   .. include:: /config-directive-description/dir-catalog-Reconnect.rst.inc



.. config:option:: dir/catalog/User

   :type: STRING

   *This directive is an alias.*

   .. include:: /config-directive-description/dir-catalog-User.rst.inc



.. config:option:: dir/catalog/ValidateTimeout

   :type: PINT32
   :default: 120

   This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the validation timeout after which the database connection is polled to see if its still alive.

   .. include:: /config-directive-description/dir-catalog-ValidateTimeout.rst.inc



