--List clients/filesets with backup units required to subscribe this installation
SELECT *
FROM backup_unit_overview
ORDER BY client, fileset;
