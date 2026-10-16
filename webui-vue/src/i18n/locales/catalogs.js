/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

import localeManifest from './locales.json'
import cn_CN from './cn_CN.json'
import cs_CZ from './cs_CZ.json'
import de_DE from './de_DE.json'
import en_EN from './en_EN.json'
import es_ES from './es_ES.json'
import fr_FR from './fr_FR.json'
import hu_HU from './hu_HU.json'
import it_IT from './it_IT.json'
import nl_BE from './nl_BE.json'
import pl_PL from './pl_PL.json'
import pt_BR from './pt_BR.json'
import ru_RU from './ru_RU.json'
import sk_SK from './sk_SK.json'
import tr_TR from './tr_TR.json'
import uk_UA from './uk_UA.json'

export const DEFAULT_WEBUI_LOCALE = localeManifest.default
export const WEBUI_LOCALES = Object.freeze(localeManifest.locales)

const CATALOGS = {
  cn_CN,
  cs_CZ,
  de_DE,
  en_EN,
  es_ES,
  fr_FR,
  hu_HU,
  it_IT,
  nl_BE,
  pl_PL,
  pt_BR,
  ru_RU,
  sk_SK,
  tr_TR,
  uk_UA,
}

function nonEmptyTranslations(catalog) {
  return Object.fromEntries(
    Object.entries(catalog).filter(([, translation]) => translation !== '')
  )
}

export const WEBUI_MESSAGES = Object.fromEntries(
  Object.entries(CATALOGS).map(([locale, catalog]) => [
    locale,
    nonEmptyTranslations(catalog),
  ])
)
