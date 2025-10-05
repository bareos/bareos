<?php

/**
 * @see       https://github.com/laminas/laminas-form for the canonical source repository
 * @copyright https://github.com/laminas/laminas-form/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-form/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Form\View;

use Laminas\ServiceManager\ConfigInterface;
use Laminas\ServiceManager\ServiceManager;

/**
 * Service manager configuration for form view helpers
 */
class HelperConfig implements ConfigInterface
{
    /**
     * Pre-aliased view helpers
     *
     * @var array
     */
    protected $invokables = [
        'form'                    => 'Laminas\Form\View\Helper\Form',
        'formbutton'              => 'Laminas\Form\View\Helper\FormButton',
        'formcaptcha'             => 'Laminas\Form\View\Helper\FormCaptcha',
        'captchadumb'             => 'Laminas\Form\View\Helper\Captcha\Dumb',
        'formcaptchadumb'         => 'Laminas\Form\View\Helper\Captcha\Dumb',
        'captchafiglet'           => 'Laminas\Form\View\Helper\Captcha\Figlet',
        'formcaptchafiglet'       => 'Laminas\Form\View\Helper\Captcha\Figlet',
        'captchaimage'            => 'Laminas\Form\View\Helper\Captcha\Image',
        'formcaptchaimage'        => 'Laminas\Form\View\Helper\Captcha\Image',
        'captcharecaptcha'        => 'Laminas\Form\View\Helper\Captcha\ReCaptcha',
        'formcaptcharecaptcha'    => 'Laminas\Form\View\Helper\Captcha\ReCaptcha',
        'formcheckbox'            => 'Laminas\Form\View\Helper\FormCheckbox',
        'formcollection'          => 'Laminas\Form\View\Helper\FormCollection',
        'formcolor'               => 'Laminas\Form\View\Helper\FormColor',
        'formdate'                => 'Laminas\Form\View\Helper\FormDate',
        'formdatetime'            => 'Laminas\Form\View\Helper\FormDateTime',
        'formdatetimelocal'       => 'Laminas\Form\View\Helper\FormDateTimeLocal',
        'formdatetimeselect'      => 'Laminas\Form\View\Helper\FormDateTimeSelect',
        'formdateselect'          => 'Laminas\Form\View\Helper\FormDateSelect',
        'formelement'             => 'Laminas\Form\View\Helper\FormElement',
        'formelementerrors'       => 'Laminas\Form\View\Helper\FormElementErrors',
        'formemail'               => 'Laminas\Form\View\Helper\FormEmail',
        'formfile'                => 'Laminas\Form\View\Helper\FormFile',
        'formfileapcprogress'     => 'Laminas\Form\View\Helper\File\FormFileApcProgress',
        'formfilesessionprogress' => 'Laminas\Form\View\Helper\File\FormFileSessionProgress',
        'formfileuploadprogress'  => 'Laminas\Form\View\Helper\File\FormFileUploadProgress',
        'formhidden'              => 'Laminas\Form\View\Helper\FormHidden',
        'formimage'               => 'Laminas\Form\View\Helper\FormImage',
        'forminput'               => 'Laminas\Form\View\Helper\FormInput',
        'formlabel'               => 'Laminas\Form\View\Helper\FormLabel',
        'formmonth'               => 'Laminas\Form\View\Helper\FormMonth',
        'formmonthselect'         => 'Laminas\Form\View\Helper\FormMonthSelect',
        'formmulticheckbox'       => 'Laminas\Form\View\Helper\FormMultiCheckbox',
        'formnumber'              => 'Laminas\Form\View\Helper\FormNumber',
        'formpassword'            => 'Laminas\Form\View\Helper\FormPassword',
        'formradio'               => 'Laminas\Form\View\Helper\FormRadio',
        'formrange'               => 'Laminas\Form\View\Helper\FormRange',
        'formreset'               => 'Laminas\Form\View\Helper\FormReset',
        'formrow'                 => 'Laminas\Form\View\Helper\FormRow',
        'formsearch'              => 'Laminas\Form\View\Helper\FormSearch',
        'formselect'              => 'Laminas\Form\View\Helper\FormSelect',
        'formsubmit'              => 'Laminas\Form\View\Helper\FormSubmit',
        'formtel'                 => 'Laminas\Form\View\Helper\FormTel',
        'formtext'                => 'Laminas\Form\View\Helper\FormText',
        'formtextarea'            => 'Laminas\Form\View\Helper\FormTextarea',
        'formtime'                => 'Laminas\Form\View\Helper\FormTime',
        'formurl'                 => 'Laminas\Form\View\Helper\FormUrl',
        'formweek'                => 'Laminas\Form\View\Helper\FormWeek',
    ];

    /**
     * Configure the provided service manager instance with the configuration
     * in this class.
     *
     * Adds the invokables defined in this class to the SM managing helpers.
     *
     * @param  ServiceManager $serviceManager
     * @return void
     */
    public function configureServiceManager(ServiceManager $serviceManager)
    {
        foreach ($this->invokables as $name => $service) {
            $serviceManager->setInvokableClass($name, $service);
        }
    }
}
