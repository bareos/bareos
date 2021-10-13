--List clients/filesets that cannot be categorized for backup units yet
SELECT client, fileset, filesettext 
FROM latest_full_size_categorized 
WHERE unknown_mb > 0 
ORDER BY client, fileset;
