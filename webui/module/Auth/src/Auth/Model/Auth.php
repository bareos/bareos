<?php

namespace Auth\Model;

use Zend\InputFilter\InputFilter;
use Zend\InputFilter\InputFilterAwareInterface;
use Zend\InputFilter\InputFilterInterface;

class Auth implements InputFilterAwareInterface
{

   public $director;
   public $consolename;
   public $password;

   protected $inputFilter;

   public function setInputFilter(InputFilterInterface $inputFilter)
   {
      throw new \Exception("setInputFilter() not used");
   }

   public function getInputFilter()
   {
      if (!$this->inputFilter) {

         $inputFilter = new InputFilter();

         $inputFilter->add(array(
            'name' => 'director',
            'required' => true,
            'filters' => array(
               array('name' => 'StripTags'),
               array('name' => 'StringTrim'),
            ),
            'validators' => array(),
         ));

         $inputFilter->add(array(
            'name' => 'consolename',
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
                     'max' => 64,
                  ),
               ),
            ),
         ));

         $inputFilter->add(array(
            'name' => 'password',
      'required' => true,
      'filters' => array(),
      'validators' => array(
          array(
         'name' => 'StringLength',
         'options' => array(
             'encoding' => 'UTF-8',
             'min' => 1,
             'max' => 64,
         ),
          ),
      ),
         ));

         $this->inputFilter = $inputFilter;

      }

      return $this->inputFilter;

   }

}
