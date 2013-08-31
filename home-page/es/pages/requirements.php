<? require_once("inc/header.php"); ?>
<table>
<tr>
        <td class="contentTopic">
                Requerimientos del sistema
        </td>
</tr>
<tr>
   <td class="content">

   <ul class="hardware">
      <li><b>Bacula</b> se compil&oacute; exitosamente en sistemas Linux, FreeBSD, y Solaris.</li>
      <li>Require GNU C++ versi&oacute;n 2.95 o superior para compilar. Puede intentar con otros compiladores y versiones, aunque no es seguro que ande. Bacula compila y ejecuta exitosamente en RH8.0/RH9/RHEL 3.0 con GCC 3.2. Nota: generalmente GNU C++ es un paquete distinto (por ej.: RPM, DEB) que GNU C, por lo que necesita los dos instalados. En sistemas RedHat, el compilador de C++ es parte del paquete <b>gcc-c++</b></li>
      <li>Bacula tambi&eacute;n necesita algunos paquetes de terceros. Excepto por MySQL y PostgreSQL, se los puede encontrar en <b>depkgs</b> y <b>depkgs1</b>.</li>
      <li>Si quiere hacer un binario para Win32, debe saber que se crosscompilan en una m&aacute;quina Linux. Para m&aacute;s informaci&oacute;n puede leer src/win32/README.mingw32 en la distribuci&oacute;n de c&oacute;digo fuente. Aunque lo documentemos, no damos soporte para los binarios de Win32, excepto para el File daemon.</li>
      <li><b>Bacula</b> requiere una buena implementaci&oacute;n de pthreads para funcionar. Este no es el caso de algunos sistemas BSD.</li>
      <li>El c&oacute;digo est&aacute; escrito teniendo en cuenta la portabilidad y es casi completamente POSIX compatible. Por esto, portar el c&oacute;digo a otro sistema POSIX compatible deber&iacute;a ser relativamente f&aacute;cil.</li>
      <li>La consola de GNOME est&aacute; desarrollada y probada con GNOME 2.x</li>
      <li>La consola wxWidgets est&aacute; desarrollada y probada con la &uacute;ltima versi&oacute;n estable de <a href="http://www.wxwidgets.org/">wxWidgets</a> (2.6). Funciona bien con Windows y la versi&oacute;n GTK+-2.x de wxWidgets, y deber&iacute;a trabajar con otras plataformas soportadas por wxWidgets.</li>
      <li>El Tray Monitor est&aacute; desarrollado con GTK+-2.x. Requiere Gnome &gt;=2.2, KDE &gt;=3.1 o cualquier manejador de ventanas que soporte <a href="http://www.freedesktop.org/Standards/systemtray-spec">FreeDesktop system tray standard</a>.</li>
      <li>Si quiere habilitar edici&oacute;n en linea de comandos e historia, deber&aacute; tener cargado /usr/include/termcap.h y termcap o ncurses (libtermcap-devel o ncurses-devel).</li>
   </ul>

   </td>
</tr>
</table>
<? require_once("inc/footer.php"); ?>
