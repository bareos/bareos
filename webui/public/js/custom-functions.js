/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2020 Bareos GmbH & Co. KG (http://www.bareos.org/)
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
