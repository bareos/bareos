<? require_once("inc/header.php"); ?>

<?php

$version = $_GET["version"];

$category_map = array(
   "bacula"                    => "Source Code",
   "Win32_64"                  => "Windows",
   "rpms"                      => "Linux RPMs (official)",
   "rpms-contrib-psheaffer"    => "Linux RPMs (psheaffer)",
   "rpms-contrib-fschwarz"     => "Linux RPMs (fschwarz)",
   "depkgs"                    => "Dependency package (mtx, SQLite3)",
   "depkgs-qt"                 => "Dependency package (qt4 to build bat)"
);

$URL  = 1;
$CAT  = 2;
$VER  = 3;
$NAM  = 4;
$SIZ  = 5;
$DAT  = 6;
$DLS  = 7;

function getfiles()
{
   $ch = curl_init();
   curl_setopt($ch, CURLOPT_URL, "http://sourceforge.net/projects/bacula/files/");
   curl_setopt($ch, CURLOPT_HEADER, false);
   curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
   curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
   $res = curl_exec($ch);
   curl_close($ch);

   $res = strstr($res, "All Files");
//   echo "res=$res";
   $res = str_replace("\n", "", $res);
   $res = str_replace("\r", "", $res);

   // get list of all available files and their attributes
   $count = preg_match_all(
      //                                  cat    vers    name
      "!href=\"(/projects/bacula/files/([^/]+)/([^/]+)/([^/]+)/download)\"" .
      //            size                     date             downloads
      ".*?<td>([\d\.]+ [a-zA-Z]+)</td>.*?<td>(.*?)</td>.*?<td>(.*?)</td>!",
      $res, $files, PREG_SET_ORDER);

   // remove duplicates (due to "latest files" list)
   $out = array_filter_unique(
      $files, create_function('$a,$b', 'return strcmp($a[1], $b[1]);'));
   return $out;
}

function array_filter_unique($array, $compare)
{
   usort($array, $compare);
   for ($x = 0; $x < count($array) - 1; $x++)
      if (call_user_func($compare, $array[$x], $array[$x+1]) != 0)
         $out[] = $array[$x];
   if (count($array))
      $out[] = $array[count($array) - 1]; // last one is never a dupe
   return $out;
}

function versioncmp($a, $b)
{
   // [0] = entire string, [1] = major, [2] = minor, [3] = rev
   for ($x = 1; $x < count($a); $x++)
   {
      if ($a[$x] < $b[$x])
         return -1;
      if ($a[$x] > $b[$x])
         return 1;
   }

   return 0;
}

function getversions(&$files)
{
   global $VER;

   // assemble version strings into their own array
   foreach ($files as $file)
      $versions[] = $file[$VER];

   // filter out versions not matching A.B.Cd format
   $versions = preg_grep("/^[0-9]+\.[0-9]+\.[0-9]+[a-z]?$/", $versions);

   // split string into array at '.' and prepend original string
   foreach ($versions as $version)
   {
      $tmp = explode(".", $version);
      $out[] = array_merge((array)$version, $tmp);
   }

   // remove identical versions
   $out = array_filter_unique($out, "versioncmp");

   // create result array containing original strings
   for ($x = 0; $x < count($out); $x++)
      $out2[] = $out[$x][0];

   // finally, return array in reverse order (most recent version first)
   return array_reverse($out2);
}

$files = getfiles();
$avail_versions = getversions($files);

if ($version == "")
   $version = $avail_versions[0];

$version_ = strtr($version, ".", "_");

$notes = "https://bacula.git.sourceforge.net/git/gitweb-index.cgi";
$chglog = "http://apcupsd.cvs.sourceforge.net/viewvc/*checkout*/apcupsd/apcupsd/ChangeLog?pathrev=Release-$version_";
$pubkey = "https://sourceforge.net/projects/apcupsd/files/apcupsd%20Public%20Key/Current%20Public%20Key/apcupsd.pub/download";
$rpmkey = "https://sourceforge.net/projects/apcupsd/files/apcupsd%20Public%20Key/Current%20Public%20Key/rpmkey-apcupsd-0.1-3.noarch.rpm/download";

echo "<html>\n";
echo "<title>Bacula $version Downloads</title>\n";
echo "<body>\n";
echo "<h1>Bacula $version Downloads</h1>\n";
echo "<form method=\"get\" action=\"/en/?page=dl&\">\n";
echo "Other Versions:\n";
echo "<select name=\"version\">\n";
foreach ($avail_versions as $ver)
{
   echo "  <option value=\"$ver\"";
   if ($ver == $version)
      echo " selected=\"selected\"";
   echo ">$ver</option>\n";
}
echo "</select>\n";
echo "<input type=\"submit\" value=\"Go\"></input>\n";
echo "</form>\n";
echo "<p>\n";
echo "<a href=\"$notes\">Release Notes</a>&nbsp;|&nbsp;\n";
echo "<a href=\"$chglog\">ChangeLog</a>&nbsp;|&nbsp;\n";
echo "<a href=\"$pubkey\">Public Key</a>&nbsp;|&nbsp;\n";
echo "<a href=\"$rpmkey\">RPM Public Key</a>\n";
echo "<p>\n";

$colors = array("#E8E8FF", "#B9B9FF");

foreach ($category_map as $category => $catname)
{
   $color = 0;
   $header = false;

   foreach ($files as $file)
   {
      $isrpm = preg_match("/\.rpm$/", $file[$NAM]);
      $issig = preg_match("/\.sig$/", $file[$NAM]);
      $isrel = $file[$NAM] == "ReleaseNotes";

      if (!$issig && !$isrel &&
          $file[$VER] == $version && $file[$CAT] == $category)
      {
         // only output the table header if table won't be empty
         if (!$header)
         {
            $header = true;
            echo "<table title=\"$catname\">\n";
            echo "<tr bgcolor=\"#2E2EFF\">\n";
            echo "  <th><font size=\"+1\" color=\"FFFFFF\">$catname</font></th>\n";
            echo "  <th><font color=\"FFFFFF\">&nbsp;Signature&nbsp;</font></th>\n";
            echo "  <th><font color=\"FFFFFF\">Size</font></th>\n";
            echo "  <th><font color=\"FFFFFF\">&nbsp;Release Date&nbsp;</font></th>\n";
            echo "  <th><font color=\"FFFFFF\">&nbsp;Downloads&nbsp;</font></th>\n";
            echo "</tr>\n";
         }

         echo "<tr bgcolor=\"$colors[$color]\">\n";
         echo "  <td><a href=\"https://sourceforge.net$file[$URL]\">$file[$NAM]</a></td></td>\n";
         if ($isrpm)
            echo "  <td align=\"center\">N/A</td>\n";
         else
            echo "  <td align=\"center\"><a href=\"https://sourceforge.net/projects/bacula/files/$category/$file[$VER]/$file[$NAM].sig/download\">sig</td>\n";
         echo "  <td align=\"right\">$file[$SIZ]</td>\n";
         echo "  <td align=\"center\">$file[$DAT]</td>\n";
         echo "  <td align=\"center\">$file[$DLS]</td>\n";
         echo "</tr>\n";

         $color = ($color + 1) & 1;
      }
   }

   if ($header)
      echo "</table><br>\n";
}

echo "</body>\n";
echo "</html>\n";
?>

<? require_once("inc/footer.php"); ?>
