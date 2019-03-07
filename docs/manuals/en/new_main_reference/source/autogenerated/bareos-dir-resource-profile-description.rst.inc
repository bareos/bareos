.. config:option:: dir/profile/CatalogACL

   :type: ACL

   Lists the Catalog resources, this resource has access to. The special keyword *all* allows access to all Catalog resources.

   .. include:: /config-directive-description/dir-profile-CatalogACL.rst.inc



.. config:option:: dir/profile/ClientACL

   :type: ACL

   Lists the Client resources, this resource has access to. The special keyword *all* allows access to all Client resources.

   .. include:: /config-directive-description/dir-profile-ClientACL.rst.inc



.. config:option:: dir/profile/CommandACL

   :type: ACL

   Lists the commands, this resource has access to. The special keyword *all* allows using commands.

   .. include:: /config-directive-description/dir-profile-CommandACL.rst.inc



.. config:option:: dir/profile/Description

   :type: STRING

   Additional information about the resource. Only used for UIs.

   .. include:: /config-directive-description/dir-profile-Description.rst.inc



.. config:option:: dir/profile/FileSetACL

   :type: ACL

   Lists the File Set resources, this resource has access to. The special keyword *all* allows access to all File Set resources.

   .. include:: /config-directive-description/dir-profile-FileSetACL.rst.inc



.. config:option:: dir/profile/JobACL

   :type: ACL

   Lists the Job resources, this resource has access to. The special keyword *all* allows access to all Job resources.

   .. include:: /config-directive-description/dir-profile-JobACL.rst.inc



.. config:option:: dir/profile/Name

   :required: True
   :type: NAME

   The name of the resource.

   .. include:: /config-directive-description/dir-profile-Name.rst.inc



.. config:option:: dir/profile/PluginOptionsACL

   :type: ACL

   Specifies the allowed plugin options. An empty strings allows all Plugin Options.

   .. include:: /config-directive-description/dir-profile-PluginOptionsACL.rst.inc



.. config:option:: dir/profile/PoolACL

   :type: ACL

   Lists the Pool resources, this resource has access to. The special keyword *all* allows access to all Pool resources.

   .. include:: /config-directive-description/dir-profile-PoolACL.rst.inc



.. config:option:: dir/profile/ScheduleACL

   :type: ACL

   Lists the Schedule resources, this resource has access to. The special keyword *all* allows access to all Schedule resources.

   .. include:: /config-directive-description/dir-profile-ScheduleACL.rst.inc



.. config:option:: dir/profile/StorageACL

   :type: ACL

   Lists the Storage resources, this resource has access to. The special keyword *all* allows access to all Storage resources.

   .. include:: /config-directive-description/dir-profile-StorageACL.rst.inc



.. config:option:: dir/profile/WhereACL

   :type: ACL

   Specifies the base directories, where files could be restored. An empty string allows restores to all directories.

   .. include:: /config-directive-description/dir-profile-WhereACL.rst.inc



