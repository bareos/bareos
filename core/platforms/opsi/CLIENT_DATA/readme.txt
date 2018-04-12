TODO
* (Programme not "Program Files")
* properties?
        bconsole.conf:
        Director {
        Name = @director_name@
        DIRport = 9101
        address = @director_address@
        Password = "@director_password@"
        }

        bacula-fd.conf:
        @director_name@
        Director {
        Name = opsiwinxpjs1-mon
        Password = "2T7xTQg7uCi+RCUpQBUVYCSe+imYWbBfxOBa5QmDxDiD"
        Monitor = yes
        }

        bat.conf:
        Director {
        Name = @director_name@
        DIRport = 9101
        address = @director_address@
        Password = "@director_password@"
        }

        tray-monitor.conf:
        Monitor {
        Name = opsiwinxpjs1-mon
        Password = "@mon_password@"         # password for the Directors
        RefreshInterval = 30 seconds
        }

        Client {
        Name = opsiwinxpjs1-fd
        Address = localhost
        FDPort = 9102
        Password = "2T7xTQg7uCi+RCUpQBUVYCSe+imYWbBfxOBa5QmDxDiD"
        }

        #Storage {
        #  Name = @basename@-sd
        #  Address = @hostname@
        #  SDPort = @sd_port@
        #  Password = "@mon_sd_password@"          # password for StorageDaemon
        #}
        #
        #Director {
        #  Name = @basename@-dir
        #  DIRport = @dir_port@
        #  address = @hostname@
        #}
        #
* (fd service l??uft? start automatisch)
* firewall
* gui feedback?

installer:
http://www.bacula.org/git/cgit.cgi/bacula/tree/bacula/src/win32/full_win32_installer/winbacula.nsi

@philipp:
- \\ in pfaden bei windows configuration erforderlich?
- benennung der properties?
