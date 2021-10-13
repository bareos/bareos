-- Show total backup units required to subscribe this installation
SELECT
  COUNT(client) AS clients,
  SUM(db_units) AS db_units,
  SUM(vm_units) AS vm_units,
  SUM(filer_units) AS filer_units,
  SUM(normal_units) AS normal_units,
  COALESCE(SUM(db_units), 0)
    + COALESCE(SUM(vm_units), 0)
    + COALESCE(SUM(filer_units), 0)
    + COALESCE(SUM(normal_units), 0)
    AS total_units
  FROM backup_unit_overview;
