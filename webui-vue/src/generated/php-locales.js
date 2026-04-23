/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
 */

// One-time locale migration from
// webui/module/Auth/src/Auth/Form/LoginForm.php.

export const DEFAULT_WEBUI_LOCALE = 'en_EN'

export const PHP_WEBUI_LOCALES = [
  { value: 'cn_CN', label: 'Chinese' },
  { value: 'cs_CZ', label: 'Czech' },
  { value: 'nl_BE', label: 'Dutch/Belgium' },
  { value: 'en_EN', label: 'English' },
  { value: 'fr_FR', label: 'French' },
  { value: 'de_DE', label: 'German' },
  { value: 'hu_HU', label: 'Hungarian' },
  { value: 'it_IT', label: 'Italian' },
  { value: 'pl_PL', label: 'Polish' },
  { value: 'pt_BR', label: 'Portuguese (BR)' },
  { value: 'ru_RU', label: 'Russian' },
  { value: 'sk_SK', label: 'Slovak' },
  { value: 'es_ES', label: 'Spanish' },
  { value: 'tr_TR', label: 'Turkish' },
  { value: 'uk_UA', label: 'Ukrainian' },
]
