<? require_once("inc/header.php"); ?>
<table>
<tr>
  <td class="contentTopic">
  Reportes de Bug
  </td>
</tr>
<tr>
  <td class="content">
  <p>Antes de reportar un <em>bug</em>, por favor, primero aseg&uacute;rese que sea un bug y no una petici&oacute;n de mantenimiento o de una <em>feature</em> (Ver el men&uacute; a la izquierda).</p>
  <p>Son desarrolladores voluntarios quienes resuelven los bugs (y la comunidad en su totalidad). Si su bug tiene una prioridad muy alta o necesita ayuda profesional, por favor vea <a href="http://www.bacula.org/es/?page=professional">Servicios profesionales</a> </p>
  <p>Bacula tiene un sistema de reporte de incidentes, Mantis, implementado por Dan Langille y alojado en su servidor. Es un sistema basado en web muy f&aacute;cil de usar, y le recomendamos que lo pruebe. Sin embargo, antes de visitar nuestra base de datos de bugs, por favor lea lo siguiente:</p>
  <p>Para ver el reporte de bugs puede loguearse como usuario <b>anonymous</b> y contrase&ntilde;a <b>anonymous</b>. La ventaja de estar suscripto es la notificación por email de bugs serios y de sus arreglos.</p>
  <p>Para emitir un reporte de bug, debe crear una cuenta. Tambi&eacute;n debe ejecutar un navegador corriendo US ASCII o UTF-8. Algunos usuarios corriendo Win32 IE con Windows Eastern European han experimentado problemas con la interface de este sistema.</p>
  <p>La mayor&iacute;a de los problemas son en realidad preguntas de soporte, as&iacute; que si no est&aacute; seguro si el problema que tiene es un bug, vea la p&aacute;gina de soporte, donde encontrar&aacute; enlaces a las listas de email. Sin embargo, si determin&oacute; que el problema es un bug, debe ingresar un reporte de bug en la base de datos de bugs o enviar un email a la lista <em>bacula-devel</em>. De lo contrario, podr&iacute;a ocurrir que los desarrolladores nunca sepan del bug y, por ende, no ser&aacute; resuelto.</p>
  <p>Hay dos cosas diferentes en el manejo de incidentes respecto de otros proyectos Open Source. Primero, no podemos dar soporte o manejar pedidos de <em>features</em> v&iacute;a la base de datos de bugs; segundo, cerramos bugs muy r&aacute;pido, para evitar la saturaci&oacute;n. Por favor, no lo tome como algo personal. Si desea agregar una nota al reporte del bug una vez cerrado, puede reabrir el incidente, agregar la nota y volver a cerrar el reporte; o, para hacerlo m&aacute;s sencillo, puede enviar la nota por email a la lista <em>bacula-devel</em>. Si un desarrollador cierra el incidente pero usted cree que s&iacute; hay un bug y tiene nueva informaci&oacute;n, puede reabrir el reporte</p>

  <h3>Informaci&oacute;n necesaria para reportar un bug</h3>
  <p>Para que podamos responder a un bug, necesitamos, como m&iacute;nimo, la siguiente informaci&oacute;n (que deber&aacute; ingresar en los campos correspondientes en el sistema de reportes de bugs):</p>

  <ul>
    <li>Su sistema operativo.</li>
    <li>La versi&oacute;n de Bacula que est&aacute; utilizando.</li>
    <li>Una descripc&oacute;n <a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs.html">clara y concisa</a> del problema (Enlace en ingl&eacute;s).</li>
    <li>Si dice &quot;explota&quot;, &quot;no funciona&quot;, &quot;se cuelga&quot;, &quot;no anda&quot; o algo similar, debe incluir alguna captura de pantalla que lo muestre.</li>
  </ul>
  <p>Si tiene problemas con la cinta, incluya:</p>
  <ul>
    <li>El tipo de cinta que tiene </li>
    <li>&iquest;Ha ejecutado el comando <b>btape</b> &quot;test&quot;?</li>
  </ul>
  <p>El primero de estos items puede resolverse enviandonos una copia del archivo <b>config.out</b>, que se encuentra en el directorio principal de bacula luego de ejecutar <b>./configure</b>.</p>
  <p>Adem&aacute;s, a veces necesitaremos una copia de los archivos de configuraci&oacute;n de bacula (en particula <em>bacula-dir.conf</em>). Si cree que es un problema de configuraci&oacute;, no dude en enviarlos tambi&eacute;n.</p>
  </td>
</tr>
<tr>
   <td style="font-size: 14px; padding: 0px 20px 0px 20px">
   Por favor, lea este peque&ntilde;o <a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs.html">HowTo</a> de reportes de bug (En ingl&eacute;s).
   </td>
</tr>
<tr>
  <td class="content">
  <p>Puede enviar un bug o ver la lista de bugs abiertos y cerrados dirigiendose a:</p>
  <p style="text-align: center; font-size: 24px"><a href="http://bugs.bacula.org">http://bugs.bacula.org</a></p>
  </td>
</tr>

</table>
<? require_once("inc/footer.php"); ?>
