<?

/**
 * load header, menu 
 *
 */
require_once("inc/header.php");


/**
 * load content
 *
 */
if(is_file($page_current)) {
        include_once($page_current);
} else {
   printf('
      &nbsp;
      Es ist ein <b>Fehler</b> aufgetreten:<br>
      <p class="error">
      Die Seite <b>%s</b> ist nicht vorhanden.
      </p>', $page);
}


/**
 * load footer
 *
 */
require_once("inc/footer.php");

?>
