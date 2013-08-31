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
      An <b>ERROR</b> occurred:<br>
      <p class="error">
      The page <b>%s</b> is not available.
      </p>', $page);
}


/**
 * load footer
 *
 */
require_once("inc/footer.php");

?>
