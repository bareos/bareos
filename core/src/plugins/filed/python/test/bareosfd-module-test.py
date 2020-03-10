import bareosfd
import bareos_fd_consts

def load_bareos_plugin(plugindef):
    print ("Hello from load_bareos_plugin")
    print (plugindef)
    print (bareosfd)
    bareosfd.JobMessage(100, "Kuckuck")
    bareosfd.DebugMessage(100, "Kuckuck")
