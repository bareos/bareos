This directive specifies the number of bytes in the Pool which will stop a migration if :config:option:`dir/job/SelectionType`\  = PoolOccupancy has been specified and triggered by more than :config:option:`dir/pool/MigrationHighBytes`\  being in the pool. In other words, once a migration job is started with PoolOccupancy migration selection and it determines that there are more than Migration High Bytes, the migration job will continue to run jobs
until the number of bytes in the Pool drop to or below Migration Low Bytes.

