<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

header("Content-type: text/plain; charset=UTF-8");
@ob_end_flush();
@ob_implicit_flush(true);

$text = <<<EOTEXT
לִבִּי בְמִזְרָח וְאָנֹכִי בְּסוֹף מַעֲרָב
אֵיךְ אֶטְעֲמָה אֵת אֲשֶׁר אֹכַל וְאֵיךְ יֶעֱרָב
אֵיכָה אֲשַׁלֵּם נְדָרַי וָאֱסָרַי, בְּעוֹד
צִיּוֹן בְּחֶבֶל אֱדוֹם וַאֲנִי בְּכֶבֶל עֲרָב
יֵקַל בְּעֵינַי עֲזֹב כָּל טוּב סְפָרַד, כְּמוֹ
יֵקַר בְּעֵינַי רְאוֹת עַפְרוֹת דְּבִיר נֶחֱרָב.
EOTEXT;

echo $text;
