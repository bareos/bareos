/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (C) 2013-2026 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

var dt_locale = "";
var dt_textdomain = "";

function setDtLocale(val) {
   switch(val) {
      case 'cn':
      case 'cn_CN':
         dt_locale = 'cn_CN';
         break;
      case 'cs':
      case 'cs_CZ':
         dt_locale = 'cs_CZ';
         break;
      case 'en':
      case 'en_EN':
         dt_locale = 'en_EN';
         break;
      case 'es':
      case 'es_ES':
         dt_locale = 'es_ES';
         break;
      case 'fr':
      case 'fr_FR':
         dt_locale = 'fr_FR';
         break;
      case 'de':
      case 'de_DE':
         dt_locale = 'de_DE';
         break;
      case 'hu':
      case 'hu_HU':
         dt_locale = 'hu_HU';
         break;
      case 'it':
      case 'it_IT':
         dt_locale = 'it_IT';
         break;
      case 'pl':
      case 'pl_PL':
         dt_locale = 'pl_PL';
         break;
      case 'pt':
      case 'pt_BR':
         dt_locale = 'pt_BR';
         break;
      case 'ru':
      case 'ru_RU':
         dt_locale = 'ru_RU';
         break;
      case 'nl':
      case 'nl_BE':
         dt_locale = 'nl_BE';
         break;
      case 'tr':
      case 'tr_TR':
         dt_locale = 'tr_TR';
         break;
      case 'sk':
      case 'sk_SK':
         dt_locale = 'sk_SK';
         break;
      case 'uk':
      case 'uk_UA':
         dt_locale = 'uk_UA';
         break;
      default:
         dt_locale = 'en_EN';
   }
   initDTLocale();
}

function setDtTextDomain(val) {
   dt_textdomain = val;
}

function initDTLocale() {
   iJS.i18n.setlocale(dt_locale);
   iJS.i18n.bindtextdomain(dt_locale, dt_textdomain, "po");
   iJS.i18n.try_load_lang();
}

function refreshCsrfToken(onSuccess, onFailure) {
   let csrfElement = document.querySelector('meta[name="csrf-token"]');
   let refreshUrl = csrfElement ? csrfElement.getAttribute('data-refresh-url') : null;

   if(!refreshUrl) {
      onFailure("Unable to refresh the CSRF token.");
      return;
   }

   $.ajax({
      method: "GET",
      url: refreshUrl,
      dataType: "json",
      cache: false
   }).done(function(response) {
      if(!response || !response.csrf) {
         onFailure("Unable to refresh the CSRF token.");
         return;
      }
      csrfElement.setAttribute('content', response.csrf);
      onSuccess(response.csrf);
   }).fail(function() {
      onFailure("Unable to refresh the CSRF token. Please reload the page and try again.");
   });
}

function submitActionPostForm(form) {
   if(form.getAttribute('data-submitting') === 'true') {
      return false;
   }

   let confirmation = form.getAttribute('data-confirmation');
   if(confirmation && !confirm(confirmation)) {
      return false;
   }

   form.setAttribute('data-submitting', 'true');
   refreshCsrfToken(function(token) {
      form.querySelector('input[name="csrf"]').value = token;
      HTMLFormElement.prototype.submit.call(form);
   }, function(error) {
      form.removeAttribute('data-submitting');
      alert(error);
   });
   return false;
}

function escapeHtmlAttribute(value) {
   return String(value)
      .replace(/&/g, '&amp;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#039;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;');
}

function actionPostButton(url, parameters, title, icon, confirmation) {
   let csrfElement = document.querySelector('meta[name="csrf-token"]');
   let csrfToken = csrfElement ? csrfElement.getAttribute('content') : '';
   let form = '<form method="post" action="' + escapeHtmlAttribute(url) +
      '" style="display:inline" onsubmit="return submitActionPostForm(this)"';

   if(confirmation) {
      form += ' data-confirmation="' + escapeHtmlAttribute(confirmation) +
         '"';
   }
   form += '>';

   parameters.csrf = csrfToken;
   Object.keys(parameters).forEach(function(name) {
      form += '<input type="hidden" name="' + escapeHtmlAttribute(name) + '" value="' +
         escapeHtmlAttribute(parameters[name]) + '">';
   });

   form += '<button type="submit" class="btn btn-default btn-xs" data-toggle="tooltip" data-placement="top" title="' +
      escapeHtmlAttribute(title) + '" id="btn-1"><span class="glyphicon ' + escapeHtmlAttribute(icon) +
      '"></span></button></form>';
   return form;
}
