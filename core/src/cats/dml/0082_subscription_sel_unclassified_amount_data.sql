--Show amount of data that cannot be categorized for backup units yet
SELECT
  SUM(unknown_mb) AS unknown_mb,
  ROUND(sum(unknown_mb)/sum(total_mb) * 100, 2) AS unknown_percentage
FROM latest_full_size_categorized;
