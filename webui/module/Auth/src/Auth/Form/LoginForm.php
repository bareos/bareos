<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2016 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Auth\Form;

use Zend\Form\Form;

class LoginForm extends Form
{

   protected $config;
   protected $directors;
   protected $dird;
   protected $availableLocales;
   protected $locale;

   public function __construct($config=null, $dird=null)
   {

      $this->config = $config;
      $this->dird = $dird;
      $this->directors = $this->getDirectors();
      $this->availableLocales = $this->getAvailableLocales();
      $this->locale = $this->determineLanguage();

      parent::__construct('login');

      if(count($this->directors) == 1) {
         $this->add(array(
                  'name' => 'director',
                  'type' => 'select',
                  'options' => array(
                     'label' => 'Director',
                     'empty_option' => 'Please choose a director',
                     'value_options' => $this->directors,
                  ),
                  'attributes' => array(
                     'id' => 'director',
                     'value' => key($this->directors)
                  )
               )
         );
      }
      else {
         if(isset($this->dird)) {
            $this->add(array(
                     'name' => 'director',
                     'type' => 'select',
                     'options' => array(
                        'label' => 'Director',
                        'empty_option' => 'Please choose a director',
                        'value_options' => $this->directors,
                     ),
                     'attributes' => array(
                        'id' => 'director',
                        'value' => $this->dird
                     )
                  )
            );
         }
         else {
            $this->add(array(
                     'name' => 'director',
                     'type' => 'select',
                     'options' => array(
                        'label' => 'Director',
                        'empty_option' => 'Please choose a director',
                        'value_options' => $this->directors,
                     ),
                     'attributes' => array(
                        'id' => 'director',
                     )
                  )
            );
         }
      }

      if(isset($this->locale)) {
         $this->add(array(
            'name' => 'locale',
            'type' => 'select',
            'options' => array(
               'label' => 'Language',
               'empty_option' => 'Please choose a language',
               'value_options' => $this->availableLocales
            ),
            'attributes' => array(
               'id' => 'locale',
               'value' => key($this->locale)
            )
         ));
      }
      else {
         $this->add(array(
            'name' => 'locale',
            'type' => 'select',
            'options' => array(
               'label' => 'Language',
               'empty_option' => 'Please choose a language',
               'value_options' => $this->availableLocales
            ),
            'attributes' => array(
               'id' => 'locale',
            )
         ));
      }

      $this->add(array(
               'name' => 'consolename',
               'type' => 'text',
               'options' => array(
                  'label' => 'Username',
               ),
               'attributes' => array(
                  'placeholder' => 'Username',
               ),
            )
      );

      $this->add(array(
               'name' => 'password',
               'type' => 'password',
               'options' => array(
                  'label' => 'Password',
               ),
               'attributes' => array(
                  'placeholder' => 'Password',
               ),
            )
      );

      $this->add(array(
         'name' => 'bareos_updates',
         'type' => 'Zend\Form\Element\Hidden',
         'attributes' => array(
               'value' => '',
               'id' => 'bareos_updates'
            ),
         )
      );

      $this->add(array(
         'name' => 'submit',
         'type' => 'submit',
         'attributes' => array(
            'value' => 'Login',
            'id' => 'submit',
         ),
      )
      );

   }

   private function determineLanguage($default = "en_EN") {

      $l = array();

      if(!isset($_SERVER["HTTP_ACCEPT_LANGUAGE"]) || empty($_SERVER["HTTP_ACCEPT_LANGUAGE"])) {
         $l['en_EN'] = 'English';
         return $l;
      }

      $accepted = preg_split("{,\s*}", $_SERVER["HTTP_ACCEPT_LANGUAGE"]);
      $language = $default;
      $quality = 0;

      if(is_array($accepted) && (count($accepted) > 0)) {
         foreach($accepted as $key => $value) {
            if(!preg_match("{^([a-z]{1,8}(?:-[a-z]{1,8})*)(?:;\s*q=(0(?:\.[0-9]{1,3})?|1(?:\.0{1,3})?))?$}i", $value, $matches)) {
                continue;
            }

            $code = explode("-", $matches[1]);

            if(isset($matches[2])) {
                $priority = floatval($matches[2]);
            } else {
                $priority = 1.0;
            }

            while(count($code) > 0) {
                if($priority > $quality) {
                    $language = implode("_", $code);
                    $quality = $priority;
                    break;
                }
                break;
            }
         }
      }

      switch($language) {
         case 'cn':
            $l['cn_CN'] = 'Chinese';
            break;
         case 'cn_CN':
            $l['cn_CN'] = 'Chinese';
            break;
         case 'cs':
            $l['cs_CZ'] = 'Czech';
            break;
         case 'cs_CZ':
            $l['cs_CZ'] = 'Czech';
            break;
         case 'en_EN':
            $l['en_EN'] = 'English';
            break;
         case 'en_GB':
            $l['en_EN'] = 'English';
            break;
         case 'en_US':
            $l['en_EN'] = 'English';
            break;
         case 'en':
            $l['en_EN'] = 'English';
            break;
         case 'fr':
            $l['fr_FR'] = 'French';
            break;
         case 'fr_FR':
            $l['fr_FR'] = 'French';
            break;
         case 'de_DE':
            $l['de_DE'] = 'German';
            break;
         case 'de':
            $l['de_DE'] = 'German';
            break;
         case 'hu_HU':
            $l['hu_HU'] = 'Hungarian';
            break;
         case 'hu':
            $l['hu_HU'] = 'Hungarian';
            break;
         case 'it':
            $l['it_IT'] = 'Italian';
            break;
         case 'it_IT':
            $l['it_IT'] = 'Italian';
            break;
         case 'pl':
            $l['pl_PL'] = 'Polish';
            break;
         case 'pl_PL':
            $l['pl_PL'] = 'Polish';
            break;
         case 'pt':
            $l['pt_BR'] = 'Portuguese (BR)';
            break;
         case 'pt_BR':
            $l['pt_BR'] = 'Portuguese (BR)';
            break;
         case 'ru_RU':
            $l['ru_RU'] = 'Russian';
            break;
         case 'ru':
            $l['ru_RU'] = 'Russian';
            break;
         case 'es':
            $l['es_ES'] = 'Spanish';
            break;
         case 'es_ES':
            $l['es_ES'] = 'Spanish';
            break;
		   case 'nl_BE':
            $l['nl_BE'] = 'Dutch';
            break;
         case 'tr':
            $l['tr'] = 'Turkish';
            break;
         case 'tr_TR':
            $l['tr_TR'] = 'Turkish';
            break;
		   case 'sk':
            $l['sk'] = 'Slovak';
            break;
         case 'sk_SK':
            $l['sk_SK'] = 'Slovak';
            break;
         default:
            $l['en_EN'] = 'English';
      }

      return $l;
   }

   private function getAvailableLocales()
   {
      $locales = array();

      $locales['cn_CN'] = "Chinese";
      $locales['cs_CZ'] = "Czech";
      $locales['nl_BE'] = "Dutch/Belgium";
      $locales['en_EN'] = "English";
      $locales['fr_FR'] = "French";
      $locales['de_DE'] = "German";
      $locales['hu_HU'] = "Hungarian";
      $locales['it_IT'] = "Italian";
      $locales['pl_PL'] = "Polish";
      $locales['pt_BR'] = "Portuguese (BR)";
      $locales['ru_RU'] = "Russian";
      $locales['sk_SK'] = "Slovak";
      $locales['es_ES'] = "Spanish";
      $locales['tr_TR'] = "Turkish";

      return $locales;
   }

   public function getDirectors()
   {
      $selectData = array();

      foreach($this->config as $dird) {
         $selectData[key($this->config)] = key($this->config);
         next($this->config);
      }

      return $selectData;
   }

}
