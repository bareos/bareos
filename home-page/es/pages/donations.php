<? require_once("inc/header.php"); ?>

<table width="100%">
<tr>
        <td class="contentTopic">
                Donaciones Recibidas
        </td>
</tr>
</table>
<table width="90%" align="center">
<tr>
        <td class="content">
        <table class="news" style="width: 99%">
<?

$max_news = 5;
$news_counter = 0;

// read file into an array and revert that
// revert cause array_pop always gets the last element
//
$file = "donations.txt";
$lines = array_reverse(file($file)) or
        die("No newsfile!");

// as long as there are lines ...
//
while(count($lines) > 0 && $news_counter < $max_news) {
        // next line
        $line = array_pop($lines);

        // start of news
        if(eregi("^[a-z0-9]+;;;", $line)) {
                // news header
                list($author,$date,$time) = explode(";;;",$line);

                // news subject
                $subject = array_pop($lines);
                printf('<tr><td class="newsHeader">%s</td></tr>', $subject);
                printf('<tr><td class="newsContent"><pre class="newsContent">');

                continue;
        }

        // end of news
        if(eregi("^;;;", $line)) {
                printf('</pre></td></tr>');
                printf('<tr>');
                printf('<td class="newsFooter">%s - %s, %s</td>', $date, $time, $author);
                printf('</tr>');
                printf('<tr><td><img src="/images/spacer.gif" width="1px" height="15px"></td></tr>');
                $news_counter++;
                continue;
        }

        // news content
        printf('%s', $line);
}
?>
        </table>
        </td>
</tr>
</table>
<? require_once("inc/header.php"); ?>
