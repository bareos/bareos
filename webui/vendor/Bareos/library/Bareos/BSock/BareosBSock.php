<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2014-2019 Bareos GmbH & Co. KG
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

namespace Bareos\BSock;

class BareosBSock implements BareosBSockInterface
{
   const BNET_TLS_NONE = 0;   /* cannot do TLS */
   const BNET_TLS_OK = 1;      /* can do, but not required on my end */
   const BNET_TLS_REQUIRED = 2;   /* TLS is required */

   const BNET_EOD = -1;      /* End of data stream, new data may follow */
   const BNET_EOD_POLL = -2;   /* End of data and poll all in one */
   const BNET_STATUS = -3;      /* Send full status */
   const BNET_TERMINATE = -4;   /* Conversation terminated, doing close() */
   const BNET_POLL = -5;      /* Poll request, I'm hanging on a read */
   const BNET_HEARTBEAT = -6;   /* Heartbeat Response requested */
   const BNET_HB_RESPONSE = -7;   /* Only response permited to HB */
   const BNET_xxxxxxPROMPT = -8;   /* No longer used -- Prompt for subcommand */
   const BNET_BTIME = -9;      /* Send UTC btime */
   const BNET_BREAK = -10;      /* Stop current command -- ctl-c */
   const BNET_START_SELECT = -11;   /* Start of a selection list */
   const BNET_END_SELECT = -12;   /* End of a select list */
   const BNET_INVALID_CMD = -13;   /* Invalid command sent */
   const BNET_CMD_FAILED = -14;   /* Command failed */
   const BNET_CMD_OK = -15;   /* Command succeeded */
   const BNET_CMD_BEGIN = -16;   /* Start command execution */
   const BNET_MSGS_PENDING = -17;   /* Messages pending */
   const BNET_MAIN_PROMPT = -18;   /* Server ready and waiting */
   const BNET_SELECT_INPUT = -19;   /* Return selection input */
   const BNET_WARNING_MSG = -20;   /* Warning message */
   const BNET_ERROR_MSG = -21;   /* Error message -- command failed */
   const BNET_INFO_MSG = -22;   /* Info message -- status line */
   const BNET_RUN_CMD = -23;   /* Run command follows */
   const BNET_YESNO = -24;      /* Request yes no response */
   const BNET_START_RTREE = -25;   /* Start restore tree mode */
   const BNET_END_RTREE = -26;   /* End restore tree mode */
   const BNET_SUB_PROMPT = -27;   /* Indicate we are at a subprompt */
   const BNET_TEXT_INPUT = -28;   /* Get text input from user */

   const DIR_OK_AUTH = "1000 OK auth\n";
   const DIR_AUTH_FAILED = "1999 Authorization failed.\n";

   protected $config = array(
      'debug' => false,
      'host' => null,
      'port' => null,
      'password' => null,
      'console_name' => null,
      'pam_password' => null,
      'pam_username' => null,
      'tls_verify_peer' => null,
      'server_can_do_tls' => null,
      'server_requires_tls' => null,
      'client_can_do_tls' => null,
      'client_requires_tls' => null,
      'ca_file' => null,
      'cert_file' => null,
      'cert_file_passphrase' => null,
      'allowed_cns' => null,
      'catalog' => null,
   );

   private $socket = null;

   /**
    * Setter for testing purposes
    */
   public function set_config_param($key, $value)
   {
      $this->config[$key] = $value;
   }

   /**
    * Authenticate
    */
   public function connect_and_authenticate()
   {
      if(self::connect()) {
         return true;
      } else {
         return false;
      }
   }

   /**
    * Set configuration
    */
   private function set_config_keyword($setting, $key)
   {
      if (array_key_exists($key, $this->config)) {
         $this->config[$key] = $setting;
      } else {
         throw new \Exception("Illegal parameter $key in /config/autoload/local.php");
      }
   }

   /**
    * Set user credentials
    */
   public function set_user_credentials($username, $password)
   {
      if(!$this->config['console_name']) {
        $this->config['console_name'] = $username;
        $this->config['password'] = $password;
      }

      $this->config['pam_password'] = $password;
      $this->config['pam_username'] = $username;

      if($this->config['debug']) {
         // extended debug: print config array
         var_dump($this->config);
      }
   }

   /**
    * Set the connection configuration
    *
    * @param $config
    */
   public function set_config($config)
   {
      array_walk($config, array('self', 'set_config_keyword'));

      if($this->config['debug']) {
         // extended debug: print config array
         var_dump($this->config);
      }

      if(!empty($config['console_name'])) {
        $this->config['console_name'] = $config['console_name'];
      }
      if(!empty($config['console_password'])) {
        $this->config['password'] = $config['console_password'];
      }
   }

   /**
    * Network to host length
    *
    * @param $buffer
    * @return int
    */
   private function ntohl($buffer)
   {
      $len = array();
      $actual_length = 0;

      $len = unpack('N', $buffer);
      $actual_length = (float) $len[1];

      if($actual_length > (float)2147483647) {
         $actual_length -= (float)"4294967296";
      }

      return (int) $actual_length;
   }

   /**
    * Replace spaces in a string with the special escape character ^A which is used
    * to send strings with spaces to specific director commands.
    *
    * @param $str
    * @return string
    */
   private function bash_spaces($str)
   {
      $length = strlen($str);
      $bashed_str = "";

      for($i = 0; $i < $length; $i++) {
         if($str[$i] == ' ') {
            $bashed_str .= '^A';
         } else {
            $bashed_str .= $str[$i];
         }
      }

      return $bashed_str;
   }

   /**
    * Send a string over the console socket.
    * Encode the length as the first 4 bytes of the message and append the string.
    *
    * @param $msg
    * @return boolean
    */
   private function send($msg)
   {
      $str_length = 0;
      $str_length = strlen($msg);
      $msg = pack('N', $str_length) . $msg;
      $str_length += 4;
      while($this->socket && $str_length > 0) {
         $send = fwrite($this->socket, $msg, $str_length);
         if($send === 0 || $send === false) {
            fclose($this->socket);
            $this->socket = null;
            return false;
         } elseif($send < $str_length) {
            $msg = substr($msg, $send);
            $str_length -= $send;
         } else {
            return true;
         }
      }
      return false;
   }

   /**
    * Receive a string over the console socket.
    * First read first 4 bytes which encoded the length of the string and
    * the read the actual string.
    *
    * @return string
    */
   private function receive($len=0)
   {
      $buffer = "";
      $msg_len = 0;

      if (!$this->socket) {
        return $buffer;
      }

      if ($len === 0) {
         $buffer = stream_get_contents($this->socket, 4);
         if($buffer == false){
            return false;
         }
         $msg_len = self::ntohl($buffer);
      } else {
         $msg_len = $len;
      }

      if ($msg_len > 0) {
         $buffer = stream_get_contents($this->socket, $msg_len);
      }

      return $buffer;
   }

   /**
    * Special receive function that also knows the different so called BNET signals the
    * Bareos director can send as part of the data stream.
    *
    * @return string
    */
   private function receive_message()
   {
      $msg = "";
      $buffer = "";

      if (!$this->socket) {
        return $msg;
      }

      while (true) {
         $buffer = stream_get_contents($this->socket, 4);

         if ($buffer === false) {
            throw new \Exception("Error reading socket. " . socket_strerror(socket_last_error()) . "\n");
         }

         $len = self::ntohl($buffer);

         if ($len === 0) {
            break;
         }
         if ($len > 0) {
            $msg .= stream_get_contents($this->socket, $len);
         } elseif ($len < 0) {
            // signal received
            switch ($len) {
               case self::BNET_EOD:
                  if ($this->config['debug']) {
                     echo "Got BNET_EOD\n";
                  }
                  return $msg;
               case self::BNET_EOD_POLL:
                  if ($this->config['debug']) {
                     echo "Got BNET_EOD_POLL\n";
                  }
                  break;
               case self::BNET_STATUS:
                  if ($this->config['debug']) {
                     echo "Got BNET_STATUS\n";
                  }
                  break;
               case self::BNET_TERMINATE:
                  if ($this->config['debug']) {
                     echo "Got BNET_TERMINATE\n";
                  }
                  break;
               case self::BNET_POLL:
                  if ($this->config['debug']) {
                     echo "Got BNET_POLL\n";
                  }
                  break;
               case self::BNET_HEARTBEAT:
                  if ($this->config['debug']) {
                     echo "Got BNET_HEARTBEAT\n";
                  }
                  break;
               case self::BNET_HB_RESPONSE:
                  if ($this->config['debug']) {
                     echo "Got BNET_HB_RESPONSE\n";
                  }
                  break;
               case self::BNET_xxxxxxPROMPT:
                  if ($this->config['debug']) {
                     echo "Got BNET_xxxxxxPROMPT\n";
                  }
                  break;
               case self::BNET_BTIME:
                  if ($this->config['debug']) {
                     echo "Got BNET_BTIME\n";
                  }
                  break;
               case self::BNET_BREAK:
                  if ($this->config['debug']) {
                     echo "Got BNET_BREAK\n";
                  }
                  break;
               case self::BNET_START_SELECT:
                  if ($this->config['debug']) {
                     echo "Got BNET_START_SELECT\n";
                  }
                  break;
               case self::BNET_END_SELECT:
                  if ($this->config['debug']) {
                     echo "Got BNET_END_SELECT\n";
                  }
                  break;
               case self::BNET_INVALID_CMD:
                  if ($this->config['debug']) {
                     echo "Got BNET_INVALID_CMD\n";
                  }
                  break;
               case self::BNET_CMD_FAILED:
                  if ($this->config['debug']) {
                     echo "Got BNET_CMD_FAILED\n";
                  }
                  break;
               case self::BNET_CMD_OK:
                  if ($this->config['debug']) {
                     echo "Got BNET_CMD_OK\n";
                  }
                  break;
               case self::BNET_CMD_BEGIN:
                  if ($this->config['debug']) {
                     echo "Got BNET_CMD_BEGIN\n";
                  }
                  break;
               case self::BNET_MSGS_PENDING:
                  if ($this->config['debug']) {
                     echo "Got BNET_MSGS_PENDING\n";
                  }
                  break;
               case self::BNET_MAIN_PROMPT:
                  if ($this->config['debug']) {
                     echo "Got BNET_MAIN_PROMPT\n";
                  }
                  return $msg;
               case self::BNET_SELECT_INPUT:
                  if ($this->config['debug']) {
                     echo "Got BNET_SELECT_INPUT\n";
                  }
                  break;
               case self::BNET_WARNING_MSG:
                  if ($this->config['debug']) {
                     echo "Got BNET_WARNINGS_MSG\n";
                  }
                  break;
               case self::BNET_ERROR_MSG:
                  if ($this->config['debug']) {
                     echo "Got BNET_ERROR_MSG\n";
                  }
                  break;
               case self::BNET_INFO_MSG:
                  if ($this->config['debug']) {
                     echo "Got BNET_INFO_MSG\n";
                  }
                  break;
               case self::BNET_RUN_CMD:
                  if ($this->config['debug']) {
                     echo "Got BNET_RUN_CMD\n";
                  }
                  break;
               case self::BNET_YESNO:
                  if ($this->config['debug']) {
                     echo "Got BNET_YESNO\n";
                  }
                  break;
               case self::BNET_START_RTREE:
                  if ($this->config['debug']) {
                     echo "Got BNET_START_RTREE\n";
                  }
                  break;
               case self::BNET_END_RTREE:
                  if ($this->config['debug']) {
                     echo "Got BNET_END_RTREE\n";
                  }
                  break;
               case self::BNET_SUB_PROMPT:
                  if ($this->config['debug']) {
                     echo "Got BNET_SUB_PROMPT\n";
                  }
                  return $msg;
               case self::BNET_TEXT_INPUT:
                  if ($this->config['debug']) {
                     echo "Got BNET_TEXT_INPUT\n";
                  }
                  break;
               default:
                  throw new \Exception("Received unknown signal " . $len . "\n");
                  break;
            }
         } else {
            throw new \Exception("Received illegal packet of size " . $len . "\n");
         }
      }

      return $msg;
   }


   /**
    * Connect to a Bareos Director, authenticate the session and establish TLS if needed.
    *
    * @return boolean
    */
   private function connect()
   {
      if (!isset($this->config['host']) or !isset($this->config['port'])) {
         return false;
      }

      if($this->config['debug']) {
         error_log("console_name: ".$this->config['console_name']);
      }

      $port = $this->config['port'];
      $remote = "tcp://" . $this->config['host'] . ":" . $port;

      // set default stream context options
      $opts = array(
         'socket' => array(
            'bindto' => '0:0',
         ),
      );

      $context = stream_context_create($opts);

      try {
         //$this->socket = stream_socket_client($remote, $error, $errstr, 60, STREAM_CLIENT_CONNECT | STREAM_CLIENT_PERSISTENT, $context);
         $this->socket = stream_socket_client($remote, $error, $errstr, 60, STREAM_CLIENT_CONNECT, $context);

         if (!$this->socket) {
            throw new \Exception("Error: " . $errstr . ", director seems to be down or blocking our request.");
         }
         // socket_set_nonblock($this->socket);
      }
      catch(\Exception $e) {
         echo $e->getMessage();
         exit;
      }
      if($this->config['debug']) {
         echo "Connected to " . $this->config['host'] . " on port " . $this->config['port'] . "\n";
      }

      /*
       * It only makes sense to setup the whole TLS context when we as client support or
       * demand a TLS connection.
       */
      if ($this->config['client_can_do_tls'] || $this->config['client_requires_tls']) {
         /*
          * We verify the peer ourself so the normal stream layer doesn't need to.
          * But that does mean we need to capture the certficate.
          */
         $result = stream_context_set_option($context, 'ssl', 'verify_peer', false);
         $result = stream_context_set_option($context, 'ssl', 'verify_peer_name', false);
         $result = stream_context_set_option($context, 'ssl', 'capture_peer_cert', true);

         /*
          * Setup a CA file
          */
         if (!empty($this->config['ca_file'])) {
            $result = stream_context_set_option($context, 'ssl', 'cafile', $this->config['ca_file']);
            if ($this->config['tls_verify_peer']) {
               $result = stream_context_set_option($context, 'ssl', 'verify_peer', true);
               $result = stream_context_set_option($context, 'ssl', 'verify_peer_name', true);
            }
         } else {
            $result = stream_context_set_option($context, 'ssl', 'allow_self_signed', true);
         }

         /*
          * Cert file which needs to contain the client certificate and the key in PEM encoding.
          */
         if (!empty($this->config['cert_file'])) {
            $result = stream_context_set_option($context, 'ssl', 'local_cert', $this->config['cert_file']);

            /*
             * Passphrase needed to unlock the above cert file.
             */
            if (!empty($this->config['cert_file_passphrase'])) {
               $result = stream_context_set_option($context, 'ssl', 'passphrase', $this->config['cert_file_passphrase']);
            }
         }
      }

      if (($this->config['server_can_do_tls'] || $this->config['server_requires_tls']) &&
         ($this->config['client_can_do_tls'] || $this->config['client_requires_tls'])) {

        /*
         * STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT was introduced with PHP version
         * 5.6.0. We need to care that calling stream_socket_enable_crypto method
         * works with versions < 5.6.0 as well.
         */
         $crypto_method = STREAM_CRYPTO_METHOD_TLS_CLIENT;

         if (defined('STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT')) {
            $crypto_method |= STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT;
         }

         $result = stream_socket_enable_crypto($this->socket, true, $crypto_method);

         if (!$result) {
            throw new \Exception("Error in TLS handshake\n");
         }

         if ($this->config['tls_verify_peer']) {
            if (!empty($this->config['allowed_cns'])) {
               if (!self::tls_postconnect_verify_cn()) {
                  throw new \Exception("Error in TLS postconnect verify CN\n");
               }
            } else {
               if (!self::tls_postconnect_verify_host()) {
                  throw new \Exception("Error in TLS postconnect verify host\n");
               }
            }
         }
      }

      if (!self::login()) {
         return false;
      }

      $recv = self::receive();
      if($this->config['debug']) {
         error_log($recv);
      }

      if (!strncasecmp($recv, "1001", 4)) {
          $pam_answer = "4002".chr(0x1e).$this->config['pam_username'].chr(0x1e).$this->config['pam_password'];
          if (!self::send($pam_answer)) {
            error_log("Send failed for pam credentials");
            return false;
          }
          $recv = self::receive();
          if($this->config['debug']) {
             error_log($recv);
          }
      }

      if (!strncasecmp($recv, "1000", 4)) {
         $recv = self::receive();
         if($this->config['debug']) {
            error_log($recv);
         }
         return true;
      }

      return false;
   }

   /**
    * Disconnect a connected console session
    *
    * @return boolean
    */
   public function disconnect()
   {
      if ($this->socket != null) {
         fclose($this->socket);
         $this->socket = null;
         if ($this->config['debug']) {
            echo "Connection to " . $this->config['host'] . " on port " . $this->config['port'] . " closed\n";
         }
         return true;
      }

      return false;
   }

   /**
    * Login into a Bareos Director e.g. authenticate the console session
    *
    * @return boolean
    */
   private function login()
   {
      include 'version.php';

      if(isset($this->config['console_name'])) {
         $bashed_console_name = self::bash_spaces($this->config['console_name']);
         $DIR_HELLO = "Hello " . $bashed_console_name . " calling version $bareos_full_version\n";
      } else {
         $DIR_HELLO = "Hello *UserAgent* calling\n";
      }

      self::send($DIR_HELLO);
      $recv = self::receive();

      self::cram_md5_response($recv, $this->config['password']);
      $recv = self::receive();

      if(strncasecmp($recv, self::DIR_AUTH_FAILED, strlen(self::DIR_AUTH_FAILED)) == 0) {
         return false;
         //throw new \Exception("Failed to authenticate with Director\n");
      } elseif(strncasecmp($recv, self::DIR_OK_AUTH, strlen(self::DIR_OK_AUTH)) == 0) {
         return self::cram_md5_challenge($this->config['password']);
      } else {
         return false;
         //throw new \Exception("Unknown response to authentication by Director $recv\n");
      }

   }

   /**
    * Verify the CN of the certificate against a list of allowed CN names.
    *
    * @return boolean
    */
   private function tls_postconnect_verify_cn()
   {
      if ($this->socket) {
        return false;
      }

      $options = stream_context_get_options($this->socket);

      if (isset($options['ssl']) && isset($options['ssl']['peer_certificate'])) {
         $cert_data = openssl_x509_parse($options["ssl"]["peer_certificate"]);

         if ($this->config['debug']) {
            print_r($cert_data);
         }

         if (isset($cert_data['subject']['CN'])) {
            $common_names = $cert_data['subject']['CN'];
            if ($this->config['debug']) {
               echo("CommonNames: " . $common_names . "\n");
            }
         }

         if (isset($common_names)) {
            $checks = explode(',', $common_names);

            foreach($checks as $check) {
               $allowed_cns = explode(',', $this->config['allowed_cns']);
               foreach($allowed_cns as $allowed_cn) {
                  if (strcasecmp($check, $allowed_cn) == 0) {
                     return true;
                  }
               }
            }
         }
      }

      return false;
   }

   /**
    * Verify TLS names
    *
    * @param $names
    * @return boolean
    */
   private function verify_tls_name($names)
   {
      $hostname = $this->config['host'];
      $checks = explode(',', $names);

      $tmp = explode('.', $hostname);
      $rev_hostname = array_reverse($tmp);
      $ok = false;

      foreach($checks as $check) {
         $tmp = explode(':', $check);

         /*
          * Candidates must start with DNS:
          */
         if ($tmp[0] != 'DNS') {
            continue;
         }

         /*
          * and have something afterwards
          */
         if (!isset($tmp[1])) {
            continue;
         }

         $tmp = explode('.', $tmp[1]);

         /*
          * "*.com" is not a valid match
          */
         if (count($tmp) < 3) {
            continue;
         }

         $cand = array_reverse($tmp);
         $ok = true;

         foreach($cand as $i => $item) {
            if (!isset($rev_hostname[$i])) {
               $ok = false;
               break;
            }

            if ($rev_hostname[$i] == $item) {
               continue;
            }

            if ($item == '*') {
               break;
            }
         }

         if ($ok) {
            break;
         }
      }

      return $ok;
   }

   /**
    * Verify the subjectAltName or CN of the certificate against the hostname we are connecting to.
    *
    * @return boolean
    */
   private function tls_postconnect_verify_host()
   {
      if (!$this->socket) {
        return false;
      }

      $options = stream_context_get_options($this->socket);

      if (isset($options['ssl']) && isset($options['ssl']['peer_certificate'])) {
         $cert_data = openssl_x509_parse($options["ssl"]["peer_certificate"]);

         if ($this->config['debug']) {
            print_r($cert_data);
         }

         /*
          * Check subjectAltName extensions first.
          */
         if (isset($cert_data['extensions'])) {
            if (isset($cert_data['extensions']['subjectAltName'])) {
               $alt_names = $cert_data['extensions']['subjectAltName'];
               if ($this->config['debug']) {
                  echo("AltNames: " . $alt_names . "\n");
               }

               if (self::verify_tls_name($alt_names)) {
                  return true;
               }
            }
         }

         /*
          * Try verifying against the subject name.
          */
         if (isset($cert_data['subject']['CN'])) {
            $common_names = "DNS:" . $cert_data['subject']['CN'];
            if ($this->config['debug']) {
               echo("CommonNames: " . $common_names . "\n");
            }

            if (self::verify_tls_name($common_names)) {
               return true;
            }
         }
      }

      return false;
   }

   /**
    * Perform a CRAM MD5 response
    *
    * @param $recv
    * @param $password
    * @return boolean
    */
   private function cram_md5_response($recv, $password)
   {
      list($chal, $ssl) = sscanf($recv, "auth cram-md5 %s ssl=%d");

      switch($ssl) {
         case self::BNET_TLS_OK:
            $this->config['server_can_do_tls'] = true;
            break;
         case self::BNET_TLS_REQUIRED:
            $this->config['server_requires_tls'] = true;
            break;
         default:
            $this->config['server_can_do_tls'] = false;
            $this->config['server_requires_tls'] = false;
            break;
      }

      $m = hash_hmac('md5', $chal, md5($password), true);
      $msg = rtrim(base64_encode($m), "=");

      self::send($msg);

      return true;
   }

   /**
    * Perform a CRAM MD5 challenge
    *
    * @param $password
    * @return boolean
    */
   private function cram_md5_challenge($password)
   {
      $rand = rand(1000000000, 9999999999);
      $time = time();
      $clientname = "php-bsock";
      $client = "<" . $rand . "." . $time . "@" . $clientname . ">";

      if($this->config['client_requires_tls']) {
         $DIR_AUTH = sprintf("auth cram-md5 %s ssl=%d\n", $client, self::BNET_TLS_REQUIRED);
      } elseif($this->config['client_can_do_tls']) {
         $DIR_AUTH = sprintf("auth cram-md5 %s ssl=%d\n", $client, self::BNET_TLS_OK);
      } else {
         $DIR_AUTH = sprintf("auth cram-md5 %s ssl=%d\n", $client, self::BNET_TLS_NONE);
      }

      if(self::send($DIR_AUTH) == true) {
         $recv = self::receive();
         $m = hash_hmac('md5', $client, md5($password), true);

         $b64 = new BareosBase64();
         $msg = rtrim( $b64->encode($m, false), "=" );

         if (self::send(self::DIR_OK_AUTH) == true && strcmp(trim($recv), trim($msg)) == 0) {
            return true;
         } else {
            return false;
         }
      } else {
         return false;
      }

   }

   /**
    * Send a single command
    *
    * @param $cmd
    * @param $api
    * @return string
    */
   public function send_command($cmd, $api=0)
   {
      $result = "";
      $debug = "";

      switch($api) {
         case 2:
            // Enable api 2 with compact mode enabled
            self::send(".api 2 compact=yes");
            try {
               $debug = self::receive_message();
               if(!preg_match('/result/', $debug)) {
                  throw new \Exception("Error: API 2 not available on director.
                  Please upgrade to version 15.2.2 or greater and/or compile with jansson support.");
               }
            }
            catch(\Exception $e) {
               echo $e->getMessage();
               exit;
            }
            break;
         case 1:
            self::send(".api 1");
            $debug = self::receive_message();
            break;
         default:
            self::send(".api 0");
            $debug = self::receive_message();
            break;
      }

      if (isset($this->config['catalog'])) {
         if(self::send("use catalog=" . $this->config['catalog'])) {
            $debug = self::receive_message();
         }
      }

      if(self::send($cmd)) {
         $result = self::receive_message();
      }

      return $result;
   }

   /**
    * Send restore command
    *
    * @param $jobid
    * @param $client
    * @param $restoreclient
    * @param $restorejob
    * @param $where
    * @param $fileid
    * @param $dirid
    * @param $jobids
    *
    * @return string
    */
   public function restore($jobid=null, $client=null, $restoreclient=null, $restorejob=null, $where=null, $fileid=null, $dirid=null, $jobids=null, $replace=null)
   {
      $result = "";
      $debug = "";
      $rnd = rand(1000,1000000);

      if(self::send(".api 0")) {
         $debug = self::receive_message();
      }

      if(self::send(".bvfs_restore jobid=$jobids fileid=$fileid dirid=$dirid path=b2000$rnd")) {
         $debug = self::receive_message();
      }

      if(self::send('restore file=?b2000'.$rnd.' client='.$client.' restoreclient='.$restoreclient.' restorejob="'.$restorejob.'" where='.$where.' replace='.$replace.' yes')) {
         $result = self::receive_message();
      }

      if(self::send(".bvfs_cleanup path=b2000$rnd")) {
         $debug = self::receive_message();
      }

      return $result;
   }

}
?>
