<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2014 Bareos GmbH & Co. KG
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

class BareosBase64
{

      private $base64_digits = array(
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
            'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
            'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
            'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/');

   private $encoded = "";

   /**
    * base64 encoding
    * @param $arg
    * @param $compatible (true=standard / false=non-standard)
    * @return string
    */
   public function encode($arg, $compatible=false)
   {
      if (strlen($arg) <= 0) {
         return false;
      } else {
         if ($compatible) {
            $this->encoded = base64_encode($arg);
         } else {
            $this->encoded = self::to_bareos_base64($arg);
         }
      }
      return $this->encoded;
   }

   /**
    * Computes the 2's complement
    */
   private function twos_comp($val, $bits)
   {
      if ( ($val & (1 << ($bits - 1) )) != 0 )
         $val = $val - (1 << $bits);
      return $val;
   }

      /**
    * Bareos base64 encoding
    * @param $arg
    */
   private function to_bareos_base64($arg)
   {
      $reg = 0;
      $rem = 0;
      $save = 0;
      $mask = 0;
      $len = strlen($arg);
      $buffer = "";

      for ( $i = 0; $i < $len; ) {
         if ($rem < 6) {
            $reg <<= 8;
            $t = ord( $arg[$i++] );
            if ($t > 127) {
               $t = $this->twos_comp($t, 8);
            }
            $reg |= $t;
            $rem += 8;
         }
         $save = $reg;
         $reg >>= ($rem - 6);
         $tmp = $reg & 0x3F;
         $buffer .= $this->base64_digits[$tmp];
         $reg = $save;
         $rem -= 6;
      }

      $mask = (1 << $rem) - 1;
      $buffer .= $this->base64_digits[$reg & $mask];

      return $buffer;
   }

}

?>
