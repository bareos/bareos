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

namespace Storage\Model;

use Zend\InputFilter\InputFilter;
use Zend\InputFilter\InputFilterAwareInterface;
use Zend\InputFilter\InputFilterInterface;

class Storage implements InputFilterAwareInterface
{

   protected $storage = null;
   protected $pool = null;
   protected $drive = null;

   protected $inputFilter = null;

   public function setInputFilter(InputFilterInterface $inputFilter)
   {
      throw new \Exception("setInputFiler() not used");
   }

   public function getInputFilter()
   {
      if(!$this->inputFilter) {
         $inputFilter = new InputFilter();

         $inputFilter->add(array(
            'name' => 'pool',
            'required' => true,
            'filters' => array(
               array('name' => 'StripTags'),
               array('name' => 'StringTrim'),
            ),
            'validators' => array(
               array(
                  'name' => 'StringLength',
                  'options' => array(
                     'encoding' => 'UTF-8',
                     'min' => 1,
                     'max' => 64
                  )
               )
            )
         ));

         $inputFilter->add(array(
            'name' => 'drive',
            'required' => true,
            'filters' => array(
               array('name' => 'StripTags'),
               array('name' => 'StringTrim'),
            ),
            'validators' => array(
               array(
                  'name' => 'StringLength',
                  'options' => array(
                     'encoding' => 'UTF-8',
                     'min' => 1,
                     'max' => 64
                  )
               )
            )
         ));

         $inputFilter->add(array(
            'name' => 'storage',
            'required' => true,
            'filters' => array(
               array('name' => 'StripTags'),
               array('name' => 'StringTrim'),
            ),
            'validators' => array(
               array(
                  'name' => 'StringLength',
                  'options' => array(
                     'encoding' => 'UTF-8'
                  )
               )
            )
         ));

         $this->inputFilter = $inputFilter;
      }
      return $inputFilter;
   }
}
