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
      Ocurri&oacute; un <b>ERROR</b>:<br>
      <p class="error">
      No se encuentra la p&aacute;gina <b>%s</b>.
      </p>', $page);
}


/**
 * load footer
 *
 */
require_once("inc/footer.php");

?>
