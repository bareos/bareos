<?php
/* ------------------------------------------------------
 This script is used in crontab to update /var/www/bacula/rss.html
 file with latest Blog entries
------------------------------------------------------- */

$site = "http://sourceforge.net/apps/wordpress/bacula/feed/";
$fp = @fopen($site,"r");
while(!feof($fp)) $raw .= @fgets($fp, 4096);
fclose($fp);

echo "<ul class='menuitem'>\n";
if( eregi("<item>(.*)</item>", $raw, $rawitems ) ) {
 $items = explode("<item>", $rawitems[0]);

 for( $i = 0; ($i < count($items)-1) && ($i < 5); $i++ ) {
  eregi("<title>(.*)</title>",$items[$i+1], $title );
  eregi("<link>(.*)</link>",$items[$i+1], $url );
  eregi("<pubDate>(.*)</pubDate>", $item[$i+1], $date);

  // Try to avoid cross scripting problem
  $t = str_replace(array("<", ">", '"', "'" ),
                   array("", "", "", ""), $title[1]);

  $u = str_replace(array("<", ">", '"', "'" ),
                   array("", "", "", ""), $url[1]);

  echo "<li class='menuItem'><a href='".$u."'>".$t."</a>\n";
 }
}
echo "</ul>\n";

?>
