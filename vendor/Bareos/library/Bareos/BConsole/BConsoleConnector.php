<?php

namespace Bareos\BConsole;

class BConsoleConnector
{

        protected $bconsole_exec_path;
        protected $bconsole_config_path;
        protected $bconsole_sudo;
        protected $bconsole_output = array();

        public function __construct($config)
        {
                if(!empty($config['exec_path'])) {
                        $this->bconsole_exec_path = $config['exec_path'];
                }
                else {
                        throw new \Exception("Sorry, bconsole settings in /config/autoload/local.php not setup correctly. Missing parameter bconsole exec_path.");
                }

                if(!empty($config['config_path'])) {
                        $this->bconsole_config_path = $config['config_path'];
                }
                else {
                        throw new \Exception("Sorry, bconsole settings in /config/autoload/local.php not setup correctly. Missing parameter bconsole config_path.");
                }

                if(!empty($config['sudo'])) {
                        $this->bconsole_sudo = $config['sudo'];
                }
                else {
                        throw new \Exception("Sorry, bconsole settings in /config/autoload/local.php not setup correctly. Missing parameter bconsole sudo.");
                }
        }

	public function getBConsoleOutput($cmd)
        {

                $descriptorspec = array(
                        0 => array("pipe", "r"),
                        1 => array("pipe", "w"),
                        2 => array("pipe", "r")
                );

                $cwd = '/usr/sbin';
                $env = array('/usr/sbin');

                if($this->bconsole_sudo == "true") {
                        $process = proc_open('sudo ' . $this->bconsole_exec_path . ' -c ' . $this->bconsole_config_path, $descriptorspec, $pipes, $cwd, $env);
                }
                else {
                        $process = proc_open($this->bconsole_exec_path, $descriptorspec, $pipes, $cwd, $env);
                }

                if(!is_resource($process))
                        throw new \Exception("proc_open error");

                if(is_resource($process))
                {
                        fwrite($pipes[0], $cmd);
                        fclose($pipes[0]);
                        while(!feof($pipes[1])) {
                          array_push($this->bconsole_output, fread($pipes[1],8192));
                        }
                        fclose($pipes[1]);
                }

                $return_value = proc_close($process);

                return $this->bconsole_output;

        }

}

