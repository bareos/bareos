<? require_once("inc/header.php"); ?>
<table>
<tr>
   <td class="contentTopic"> Soporte para Bacula </td>
</tr>
<tr>
   <td class="content">


   <h3>Soporte Profesional</h3>
   Si necesita soporte profesional vea la p&aacute;gina de 
   <a href="http://www.bacula.org/es/?page=professional">
   soporte profesional</a> en este mismo sitio.<br>

   <h3>Soporte de la comunidad</h3>
   Por favor, tenga en cuenta que a nadie le pagan por hacer esto. Sin embargo, nuestro deseo es ver a la mayor cantidad de gente usando <b>Bacula</b>. Hay una gran cantidad de voluntarios deseando dar un soporte razonable por email.

   <p>Antes de preguntar algo, lea <b>Informaci&oacute;n necesaria</b> listada abajo, y podr&iacute;a ser &uacute;til revisar el archivo de emails, ya que quiz&aacute;s la soluci&oacute;n a su problema ya fue discutida o existe un parche para eso. Vea:  <a href="http://news.gmane.org/search.php?match=bacula"> http://news.gmane.org/search.php?match=bacula</a>.
   <p>Adem&aacute;s, si utiliza Bacula en producci&oacute;n, le recomendamos suscribirse a la base de datos de "bugs" en: <a href="http://bugs.bacula.org">http://bugs.bacula.org</a> para mantenerse informado de los problemas y parches. Tambi&eacute;n quiz&aacute;s quiera buscar soporte profesional en : <a href="http://www.bacula.org/es/?page=professional">http://www.bacula.org/es/?page=professional</a>.<br>
   <p>Por favor, no emita peticiones de soporte a la base de datos de "bugs". Para m&aacute;s informaci&oacute;n sobre "bugs" vea <a href=http://www.bacula.org/es/?page=bugs>Bugs page</a> en este sitio. 
   <p>Para obtener soporte de la comunidad, env&iacute;e un email a <b>bacula-users arroba lists.sourceforge.net</b>, y si es lo suficientemente espec&iacute;fico, alg&uacute;n usuario amable de Bacula lo ayudar&aacute;. Tenga en cuenta que si no menciona al menos la versi&oacute;n de Bacula y la plataforma donde corre, ser&aacute; dif&iacute;cil dar con una respuesta v&aacute;lida.a La direcci&oacute;n de email de arriba se modific&oacute; para prevenir spam. Para utilizarla debe reemplazar el <b>en</b> por el s&iacute;mbolo "@". Debido al volumen creciente de spam, debe estar suscripto para enviar emails a la lista. El enlace a su izquierda <b>Listas de email</b> provee enlaces a todas las listas de Bacula a las cuales puede suscribirse.

   Los usuarios constantemente monitorean estas listas y generalmente dar&aacute;n soporte. Vea <b>Informaci&oacute;n necesaria</b> debajo para lo que debe incluir en su petici&oacute;n de soporte. Si no da la informaci&oacute;n necesaria, tomar&aacute; m&aacute;s tiempo responder, y los usuarios podr&iacute;an tener miedo de responder si su pregunta es muy compleja o no est&aacute; bien formulada.

   <p>A m&iacute; (Kern) me llegan varios emails por fuera de la lista enviados directamente a mi direcci&oacute;n. Desafortunadamente ya no puedo dar soporte directo a los usuarios. Sin embargo, s&iacute; leo todo el email que me llega, y ocasionalmente doy alg&uacute;n que otro consejo. Si me env&iacute;a un email, h&aacute;galo con copia a la lista apropiada tambi&eacute;n, dado que podr&iacute; no responderle o podr&iacute;a responder a la lista. Si tiene algo <em>realmente</em> confidencial, ind&iacute;quelo claramente.

   <p>No env&iacute;e preguntas de soporte a la lista de <b>bacula-devel</b>. Puede enviar una pregunta sobre "bugs", una pregunta sobre desarrollo, o una mejora m&iacute;nima a la lista <b>bacula-devel</b>. Si no provee la informaci&oacute;n listada abajo, principalmente la versi&oacute;n de Bacula, ser&aacute; <b>muy</b> frustrante para nosotros, dado que generalmente el problema depende de la versi&oacute;n, y quiz&aacute;s ya est&eacute; resuelto. En esos casos, damos cuenta del problema, pero seguramente no obtendr&aacute; una respuesta, especialmente si estamos ocupados, porque nos fuerza a preguntarle primero qu&eacute; versi&oacute;n de Bacula usa (u otra informaci&oacute;n), luego enviarle la respuesta, doblando el tiempo para nosotros. Si le solicitamos m&aacute;s informaci&oacute;n, incluya por favor <b>todo</b> el hilo de emails en el mensaje, para evitarnos ir a los archivos para leer lo que nos escribi&oacute; previamente. En definitiva, si quiere una respuesta, por favor lea &quot;Informaci&oacute;n Necesaria &quot; abajo.
 

   <p>Para soporte en vivo puede visitar nuestro canal de IRC en <a href="http://www.freenode.net">Freenode</a> net, llamado  #bacula.

<h3>Informaci&oacute;n necesaria</h3>
Para que podamos responder a un reporte de bugs generalmente necesitamos como m&iacute;nimo la siguiente informaci&oacute;, que encajan en los campos apropiados del sistema de reportes de bug.

<ul>
<li>Su sistema operativo</li>
<li>La versi&oacute;n de Bacula que est&aacute; usando</li>
<li>Una descripci&oacute;n <a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs.html">clara y concisa (en ingl&eacute;s)</a> del problema</li>
<li>Si dice que el sistema &quot;se rompe&quot;, &quot;se cuelga&quot;, &quot;no anda&quot; o algo similar, 
deber&iacute;a incluir alguna salida de Bacula donde se vea eso.</li>
<li>Si respondemos a su email, y usted responde con m&aacute;s informaci&oacute;n, aseg&uacute;rese de incluir el texto <b>completo</b> de los emails anteriores as&iacute; disponemos de toda la informaci&oacute;n en un solo lugar.</li>
</ul>
Si tiene problemas con la cinta, incluya: 
<ul>
 <li>El tipo de cinta que tiene </li>
 <li>&iquest;Ha ejecutado el comando <b>btape</b> &quot;test&quot;?</li>
</ul>

Si tiene problemas con la base de datos, incluya:
<ul>
 <li>La base de datos que usa: MySQL, PostgreSQL, SQLite, SQLite3 </li>
 <li>La versi&oacute;n de base de datos</li>
</ul>
Los primeros dos items los puede completar enviando una copia de <b>config.out</b>, que lo 
encontrar&aacute; en el directorio principal de Bacula luego de correr <b>./configure</b>.
<p>Adem&aacute;s, a veces necesitamos una copia de su configuraci&oacute;n de Bacula (especialmente bacula-dir.conf). 
Si piensa que se trata de un problema de configuraci&oacute;n, no dude en enviar tambi&eacute;n la configuraci&oacute;n.</td>

</tr>
<tr>
   <td style="font-size: 14px; padding: 0px 20px 0px 20px">
   Por favor, lea tambi&eacute;n el peque&ntilde;o <a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs.html">HowTo</a> para reportar bugs.
   </td>
</tr>

</table>
<? require_once("inc/footer.php"); ?>
