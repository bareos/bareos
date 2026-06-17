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

import { createI18n } from 'vue-i18n'
import { DEFAULT_WEBUI_LOCALE } from '../generated/webui-locales.js'
import { WEBUI_MESSAGES } from '../generated/webui-messages.js'
import { normalizeWebUiLocale } from '../utils/locales.js'

const MESSAGE_OVERRIDES = {
  cn_CN: {
    'Log in to Bareos': '登录 Bareos',
    Username: '用户名',
    Password: '密码',
    Directors: 'Director 列表',
    Login: '登录',
    'Retry remaining directors': '重试其余 Director',
    'Skip failed directors': '跳过失败的 Director',
    'Reuse current credentials': '重用当前凭据',
    'Retry the remaining directors or skip them.':
      '重试其余 Director 或跳过它们。',
    'Retry the remaining directors.': '重试其余 Director。',
    'The entered credentials will be tried on all configured directors.':
      '将在所有已配置的 Director 上尝试输入的凭据。',
    'Login failure': '登录失败',
    'Successfully logged in': '登录成功',
    'Not yet logged in': '尚未登录',
  },
  cs_CZ: {
    'Log in to Bareos': 'Přihlásit se do Bareos',
    Username: 'Uživatelské jméno',
    Password: 'Heslo',
    Directors: 'Ředitelé',
    Login: 'Přihlásit se',
    'Retry remaining directors': 'Zkusit zbývající ředitele',
    'Skip failed directors': 'Přeskočit neúspěšné ředitele',
    'Reuse current credentials': 'Znovu použít aktuální přihlašovací údaje',
    'Retry the remaining directors or skip them.':
      'Zkuste zbývající ředitele nebo je přeskočte.',
    'Retry the remaining directors.': 'Zkuste zbývající ředitele.',
    'The entered credentials will be tried on all configured directors.':
      'Zadané přihlašovací údaje budou vyzkoušeny na všech nakonfigurovaných ředitelích.',
    'Login failure': 'Přihlášení selhalo',
    'Successfully logged in': 'Úspěšně přihlášeno',
    'Not yet logged in': 'Zatím nepřihlášeno',
  },
  nl_BE: {
    'Log in to Bareos': 'Aanmelden bij Bareos',
    Username: 'Gebruikersnaam',
    Password: 'Wachtwoord',
    Directors: 'Directoren',
    Login: 'Aanmelden',
    'Retry remaining directors': 'Opnieuw proberen met resterende directoren',
    'Skip failed directors': 'Mislukte directoren overslaan',
    'Reuse current credentials': 'Huidige inloggegevens opnieuw gebruiken',
    'Retry the remaining directors or skip them.':
      'Probeer de resterende directoren opnieuw of sla ze over.',
    'Retry the remaining directors.': 'Probeer de resterende directoren opnieuw.',
    'The entered credentials will be tried on all configured directors.':
      'De ingevoerde inloggegevens worden op alle geconfigureerde directoren geprobeerd.',
    'Login failure': 'Aanmelden mislukt',
    'Successfully logged in': 'Succesvol aangemeld',
    'Not yet logged in': 'Nog niet aangemeld',
  },
  de_DE: {
    'Log in to Bareos': 'Bei Bareos anmelden',
    Login: 'Anmelden',
    Username: 'Benutzername',
    Password: 'Passwort',
    Directors: 'Direktoren',
    'Retry remaining directors': 'Verbleibende Direktoren erneut versuchen',
    'Skip failed directors': 'Fehlgeschlagene Direktoren überspringen',
    'Reuse current credentials': 'Aktuelle Zugangsdaten wiederverwenden',
    'Retry the remaining directors or skip them.':
      'Verbleibende Direktoren erneut versuchen oder überspringen.',
    'Retry the remaining directors.':
      'Verbleibende Direktoren erneut versuchen.',
    'The entered credentials will be tried on all configured directors.':
      'Die eingegebenen Zugangsdaten werden bei allen konfigurierten Direktoren ausprobiert.',
    'Login failure': 'Anmeldung fehlgeschlagen',
    'Successfully logged in': 'Erfolgreich angemeldet',
    'Not yet logged in': 'Noch nicht angemeldet',
    'Could not load the configured directors.':
      'Die konfigurierten Direktoren konnten nicht geladen werden.',
    'Could not log in to any configured director. Retry the remaining directors.':
      'An keinem konfigurierten Direktor konnte eine Anmeldung erfolgen. Verbleibende Direktoren erneut versuchen.',
    'Authentication failed': 'Authentifizierung fehlgeschlagen',
    'Could not connect to director. Is the proxy running?':
      'Verbindung zum Direktor fehlgeschlagen. Läuft der Proxy?',
    'WebSocket connection failed': 'WebSocket-Verbindung fehlgeschlagen',
    'WebSocket connection failed. Check proxy configuration or firewall.':
      'WebSocket-Verbindung fehlgeschlagen. Proxy-Konfiguration oder Firewall prüfen.',
    'Director list request failed: {message}':
      'Anfrage der Direktorenliste fehlgeschlagen: {message}',
  },
  fr_FR: {
    'Log in to Bareos': 'Se connecter à Bareos',
    Username: "Nom d'utilisateur",
    Password: 'Mot de passe',
    Directors: 'Directeurs',
    Login: 'Connexion',
    'Retry remaining directors': 'Réessayer les directeurs restants',
    'Skip failed directors': 'Ignorer les directeurs en échec',
    'Reuse current credentials': 'Réutiliser les identifiants actuels',
    'Retry the remaining directors or skip them.':
      'Réessayez les directeurs restants ou ignorez-les.',
    'Retry the remaining directors.': 'Réessayez les directeurs restants.',
    'The entered credentials will be tried on all configured directors.':
      'Les identifiants saisis seront essayés sur tous les directeurs configurés.',
    'Login failure': 'Échec de la connexion',
    'Successfully logged in': 'Connecté avec succès',
    'Not yet logged in': 'Pas encore connecté',
  },
  hu_HU: {
    'Log in to Bareos': 'Bejelentkezés a Bareosba',
    Username: 'Felhasználónév',
    Password: 'Jelszó',
    Directors: 'Direktorok',
    Login: 'Bejelentkezés',
    'Retry remaining directors': 'Fennmaradó direktorok újrapróbálása',
    'Skip failed directors': 'Sikertelen direktorok kihagyása',
    'Reuse current credentials': 'Jelenlegi hitelesítő adatok újrahasználata',
    'Retry the remaining directors or skip them.':
      'Próbálja újra a fennmaradó direktorokat, vagy hagyja ki őket.',
    'Retry the remaining directors.': 'Próbálja újra a fennmaradó direktorokat.',
    'The entered credentials will be tried on all configured directors.':
      'A megadott hitelesítő adatok minden konfigurált direktoron kipróbálásra kerülnek.',
    'Login failure': 'Bejelentkezés sikertelen',
    'Successfully logged in': 'Sikeres bejelentkezés',
    'Not yet logged in': 'Még nincs bejelentkezve',
  },
  it_IT: {
    'Log in to Bareos': 'Accedi a Bareos',
    Username: 'Nome utente',
    Password: 'Password',
    Directors: 'Direttori',
    Login: 'Accedi',
    'Retry remaining directors': 'Riprova i direttori rimanenti',
    'Skip failed directors': 'Salta i direttori non riusciti',
    'Reuse current credentials': 'Riutilizza le credenziali correnti',
    'Retry the remaining directors or skip them.':
      'Riprova i direttori rimanenti o saltali.',
    'Retry the remaining directors.': 'Riprova i direttori rimanenti.',
    'The entered credentials will be tried on all configured directors.':
      'Le credenziali inserite verranno provate su tutti i direttori configurati.',
    'Login failure': 'Accesso non riuscito',
    'Successfully logged in': 'Accesso effettuato con successo',
    'Not yet logged in': 'Non ancora connesso',
  },
  pl_PL: {
    'Log in to Bareos': 'Zaloguj się do Bareos',
    Username: 'Nazwa użytkownika',
    Password: 'Hasło',
    Directors: 'Dyrektorzy',
    Login: 'Zaloguj',
    'Retry remaining directors': 'Ponów dla pozostałych dyrektorów',
    'Skip failed directors': 'Pomiń nieudanych dyrektorów',
    'Reuse current credentials': 'Użyj ponownie bieżących poświadczeń',
    'Retry the remaining directors or skip them.':
      'Ponów dla pozostałych dyrektorów lub ich pomiń.',
    'Retry the remaining directors.': 'Ponów dla pozostałych dyrektorów.',
    'The entered credentials will be tried on all configured directors.':
      'Wprowadzone poświadczenia zostaną wypróbowane na wszystkich skonfigurowanych dyrektorach.',
    'Login failure': 'Logowanie nie powiodło się',
    'Successfully logged in': 'Zalogowano pomyślnie',
    'Not yet logged in': 'Jeszcze nie zalogowano',
  },
  pt_BR: {
    'Log in to Bareos': 'Entrar no Bareos',
    Username: 'Nome de usuário',
    Password: 'Senha',
    Directors: 'Diretores',
    Login: 'Entrar',
    'Retry remaining directors': 'Tentar novamente os diretores restantes',
    'Skip failed directors': 'Ignorar diretores com falha',
    'Reuse current credentials': 'Reutilizar credenciais atuais',
    'Retry the remaining directors or skip them.':
      'Tente novamente os diretores restantes ou ignore-os.',
    'Retry the remaining directors.': 'Tente novamente os diretores restantes.',
    'The entered credentials will be tried on all configured directors.':
      'As credenciais informadas serão testadas em todos os diretores configurados.',
    'Login failure': 'Falha no login',
    'Successfully logged in': 'Login realizado com sucesso',
    'Not yet logged in': 'Ainda não conectado',
  },
  ru_RU: {
    'Log in to Bareos': 'Войти в Bareos',
    Username: 'Имя пользователя',
    Password: 'Пароль',
    Directors: 'Директоры',
    Login: 'Войти',
    'Retry remaining directors': 'Повторить для оставшихся директоров',
    'Skip failed directors': 'Пропустить директоры с ошибкой',
    'Reuse current credentials': 'Повторно использовать текущие учетные данные',
    'Retry the remaining directors or skip them.':
      'Повторите для оставшихся директоров или пропустите их.',
    'Retry the remaining directors.': 'Повторите для оставшихся директоров.',
    'The entered credentials will be tried on all configured directors.':
      'Введенные учетные данные будут проверены на всех настроенных директорах.',
    'Login failure': 'Ошибка входа',
    'Successfully logged in': 'Вход выполнен успешно',
    'Not yet logged in': 'Вход еще не выполнен',
  },
  sk_SK: {
    'Log in to Bareos': 'Prihlásiť sa do Bareos',
    Username: 'Používateľské meno',
    Password: 'Heslo',
    Directors: 'Riaditelia',
    Login: 'Prihlásiť sa',
    'Retry remaining directors': 'Znova skúsiť zostávajúcich riaditeľov',
    'Skip failed directors': 'Preskočiť neúspešných riaditeľov',
    'Reuse current credentials': 'Znova použiť aktuálne prihlasovacie údaje',
    'Retry the remaining directors or skip them.':
      'Skúste znova zostávajúcich riaditeľov alebo ich preskočte.',
    'Retry the remaining directors.': 'Skúste znova zostávajúcich riaditeľov.',
    'The entered credentials will be tried on all configured directors.':
      'Zadané prihlasovacie údaje sa použijú na všetkých nakonfigurovaných riaditeľoch.',
    'Login failure': 'Prihlásenie zlyhalo',
    'Successfully logged in': 'Úspešne prihlásený',
    'Not yet logged in': 'Zatiaľ neprihlásený',
  },
  es_ES: {
    'Log in to Bareos': 'Iniciar sesión en Bareos',
    Username: 'Nombre de usuario',
    Password: 'Contraseña',
    Directors: 'Directores',
    Login: 'Iniciar sesión',
    'Retry remaining directors': 'Reintentar directores restantes',
    'Skip failed directors': 'Omitir directores fallidos',
    'Reuse current credentials': 'Reutilizar credenciales actuales',
    'Retry the remaining directors or skip them.':
      'Reintenta los directores restantes o sáltalos.',
    'Retry the remaining directors.': 'Reintenta los directores restantes.',
    'The entered credentials will be tried on all configured directors.':
      'Las credenciales introducidas se probarán en todos los directores configurados.',
    'Login failure': 'Error de inicio de sesión',
    'Successfully logged in': 'Inicio de sesión correcto',
    'Not yet logged in': 'Aún no has iniciado sesión',
  },
  tr_TR: {
    'Log in to Bareos': "Bareos'ta oturum aç",
    Username: 'Kullanıcı adı',
    Password: 'Parola',
    Directors: 'Direktörler',
    Login: 'Oturum aç',
    'Retry remaining directors': 'Kalan direktörleri yeniden dene',
    'Skip failed directors': 'Başarısız direktörleri atla',
    'Reuse current credentials': 'Mevcut kimlik bilgilerini yeniden kullan',
    'Retry the remaining directors or skip them.':
      'Kalan direktörleri yeniden deneyin veya atlayın.',
    'Retry the remaining directors.': 'Kalan direktörleri yeniden deneyin.',
    'The entered credentials will be tried on all configured directors.':
      'Girilen kimlik bilgileri yapılandırılmış tüm direktörlerde denenecek.',
    'Login failure': 'Oturum açma başarısız',
    'Successfully logged in': 'Başarıyla oturum açıldı',
    'Not yet logged in': 'Henüz oturum açılmadı',
  },
  uk_UA: {
    'Log in to Bareos': 'Увійти до Bareos',
    Username: "Ім'я користувача",
    Password: 'Пароль',
    Directors: 'Директори',
    Login: 'Увійти',
    'Retry remaining directors': 'Повторити для решти директорів',
    'Skip failed directors': 'Пропустити директори з помилкою',
    'Reuse current credentials': 'Повторно використати поточні облікові дані',
    'Retry the remaining directors or skip them.':
      'Повторіть для решти директорів або пропустіть їх.',
    'Retry the remaining directors.': 'Повторіть для решти директорів.',
    'The entered credentials will be tried on all configured directors.':
      'Введені облікові дані буде перевірено на всіх налаштованих директорах.',
    'Login failure': 'Помилка входу',
    'Successfully logged in': 'Успішний вхід',
    'Not yet logged in': 'Ще не виконано вхід',
  },
}

const EFFECTIVE_MESSAGES = Object.fromEntries(
  Object.entries(WEBUI_MESSAGES).map(([locale, messages]) => [
    locale,
    {
      ...messages,
      ...(MESSAGE_OVERRIDES[locale] ?? {}),
    },
  ])
)

function interpolate(message, values = {}) {
  return Object.entries(values).reduce(
    (text, [key, value]) => text.replaceAll(`{${key}}`, String(value)),
    message
  )
}

export const i18n = createI18n({
  legacy: false,
  locale: DEFAULT_WEBUI_LOCALE,
  fallbackLocale: DEFAULT_WEBUI_LOCALE,
  messages: EFFECTIVE_MESSAGES,
  missingWarn: false,
  fallbackWarn: false,
})

export function setI18nLocale(locale) {
  i18n.global.locale.value = normalizeWebUiLocale(locale)
}

export function translate(locale, msgid, values) {
  const effectiveLocale = normalizeWebUiLocale(locale)
  const catalog = EFFECTIVE_MESSAGES[effectiveLocale]
    ?? EFFECTIVE_MESSAGES[DEFAULT_WEBUI_LOCALE]
    ?? {}
  const fallbackCatalog = EFFECTIVE_MESSAGES[DEFAULT_WEBUI_LOCALE] ?? {}
  const message = catalog[msgid] ?? fallbackCatalog[msgid] ?? msgid

  return interpolate(message, values)
}
