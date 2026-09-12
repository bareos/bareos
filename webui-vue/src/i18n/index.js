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
    'Jobs Past 24 h': '过去24小时的作业',
    'Recent Jobs': '最近的作业',
    'Pool Storage (Bytes)': '存储池空间(字节)',
    'Pool Storage (Volumes)': '存储池空间(卷)',
    'Trouble View': '问题视图',
    'Database Table Sizes': '数据库表大小',
    'Summary counts of jobs started in the last 24 hours by status.':
      '按状态汇总过去24小时内启动的作业数量。',
    'Table showing the most recent job run per job name.':
      '按作业名称显示最近一次运行的表格。',
    'Live list of currently running jobs with progress.':
      '当前正在运行的作业及其进度的实时列表。',
    'Cumulative job, file, and byte totals across all directors.':
      '所有 Director 的作业、文件和字节累计总数。',
    'Doughnut chart showing bytes stored across pools.':
      '显示各存储池已存储字节数的环形图。',
    'Doughnut chart showing volume count across pools.':
      '显示各存储池卷数量的环形图。',
    'Error and warning lines from job logs in the last 24 hours.':
      '过去24小时作业日志中的错误和警告行。',
    'Database size and per-table size breakdown.':
      '数据库大小及按表划分的大小明细。',
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
    'Jobs Past 24 h': 'Joby za posledních 24 h',
    'Recent Jobs': 'Nedávné joby',
    'Pool Storage (Bytes)': 'Úložiště poolu (bajty)',
    'Pool Storage (Volumes)': 'Úložiště poolu (svazky)',
    'Trouble View': 'Přehled problémů',
    'Database Table Sizes': 'Velikosti tabulek databáze',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Souhrnný počet jobů spuštěných za posledních 24 hodin podle stavu.',
    'Table showing the most recent job run per job name.':
      'Tabulka zobrazující poslední spuštění pro každý název jobu.',
    'Live list of currently running jobs with progress.':
      'Živý seznam aktuálně běžících jobů s průběhem.',
    'Cumulative job, file, and byte totals across all directors.':
      'Kumulativní součty jobů, souborů a bajtů napříč všemi direktory.',
    'Doughnut chart showing bytes stored across pools.':
      'Prstencový graf zobrazující bajty uložené v jednotlivých poolech.',
    'Doughnut chart showing volume count across pools.':
      'Prstencový graf zobrazující počet svazků v jednotlivých poolech.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Řádky chyb a upozornění z protokolů jobů za posledních 24 hodin.',
    'Database size and per-table size breakdown.':
      'Velikost databáze a rozpis podle tabulek.',
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
    'Jobs Past 24 h': 'Jobs afgelopen 24 u',
    'Recent Jobs': 'Recente jobs',
    'Pool Storage (Bytes)': 'Poolopslag (bytes)',
    'Pool Storage (Volumes)': 'Poolopslag (volumes)',
    'Trouble View': 'Probleemoverzicht',
    'Database Table Sizes': 'Databasetabelgroottes',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Samenvattend aantal jobs gestart in de afgelopen 24 uur, per status.',
    'Table showing the most recent job run per job name.':
      'Tabel met de meest recente uitvoering per jobnaam.',
    'Live list of currently running jobs with progress.':
      'Live lijst van momenteel actieve jobs met voortgang.',
    'Cumulative job, file, and byte totals across all directors.':
      'Cumulatieve job-, bestands- en bytetotalen over alle directors.',
    'Doughnut chart showing bytes stored across pools.':
      'Ringdiagram met opgeslagen bytes per pool.',
    'Doughnut chart showing volume count across pools.':
      'Ringdiagram met het aantal volumes per pool.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Fout- en waarschuwingsregels uit joblogs van de afgelopen 24 uur.',
    'Database size and per-table size breakdown.':
      'Databasegrootte en uitsplitsing per tabel.',
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
    'Jobs Past 24 h': 'Jobs (letzte 24 Stunden)',
    'Recent Jobs': 'Letzte Jobs',
    'Pool Storage (Bytes)': 'Pool-Speicher (Bytes)',
    'Pool Storage (Volumes)': 'Pool-Speicher (Volumes)',
    'Trouble View': 'Fehleransicht',
    'Database Table Sizes': 'Datenbank-Tabellengrößen',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Zusammenfassung der in den letzten 24 Stunden gestarteten Jobs nach Status.',
    'Table showing the most recent job run per job name.':
      'Tabelle mit dem letzten Joblauf pro Jobname.',
    'Live list of currently running jobs with progress.':
      'Live-Liste der aktuell laufenden Jobs mit Fortschritt.',
    'Cumulative job, file, and byte totals across all directors.':
      'Kumulierte Job-, Datei- und Byte-Summen über alle Direktoren.',
    'Doughnut chart showing bytes stored across pools.':
      'Ringdiagramm mit gespeicherten Bytes je Pool.',
    'Doughnut chart showing volume count across pools.':
      'Ringdiagramm mit der Anzahl der Volumes je Pool.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Fehler- und Warnzeilen aus Joblogs der letzten 24 Stunden.',
    'Database size and per-table size breakdown.':
      'Datenbankgröße und Größenaufschlüsselung je Tabelle.',
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
    'Jobs Past 24 h': 'Jobs des dernières 24 h',
    'Recent Jobs': 'Jobs récents',
    'Pool Storage (Bytes)': 'Stockage des pools (octets)',
    'Pool Storage (Volumes)': 'Stockage des pools (volumes)',
    'Trouble View': 'Vue des incidents',
    'Database Table Sizes': 'Tailles des tables de la base de données',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Nombre de jobs démarrés au cours des dernières 24 heures, par statut.',
    'Table showing the most recent job run per job name.':
      'Tableau montrant la dernière exécution de chaque job.',
    'Live list of currently running jobs with progress.':
      'Liste en direct des jobs actuellement en cours avec leur progression.',
    'Cumulative job, file, and byte totals across all directors.':
      'Totaux cumulés des jobs, fichiers et octets sur tous les directeurs.',
    'Doughnut chart showing bytes stored across pools.':
      'Graphique en anneau montrant les octets stockés par pool.',
    'Doughnut chart showing volume count across pools.':
      'Graphique en anneau montrant le nombre de volumes par pool.',
    'Error and warning lines from job logs in the last 24 hours.':
      "Lignes d'erreur et d'avertissement des journaux de jobs des dernières 24 heures.",
    'Database size and per-table size breakdown.':
      'Taille de la base de données et répartition par table.',
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
    'Jobs Past 24 h': 'Feladatok az elmúlt 24 órában',
    'Recent Jobs': 'Legutóbbi feladatok',
    'Pool Storage (Bytes)': 'Poolterület (bájt)',
    'Pool Storage (Volumes)': 'Poolterület (kötetek)',
    'Trouble View': 'Problémanézet',
    'Database Table Sizes': 'Adatbázistábla-méretek',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Az elmúlt 24 órában elindított feladatok összesített száma állapot szerint.',
    'Table showing the most recent job run per job name.':
      'Táblázat, amely feladatnevenként a legutóbbi futtatást mutatja.',
    'Live list of currently running jobs with progress.':
      'Élő lista az aktuálisan futó feladatokról a folyamat állapotával.',
    'Cumulative job, file, and byte totals across all directors.':
      'Kumulált feladat-, fájl- és bájtösszegek az összes direktoron.',
    'Doughnut chart showing bytes stored across pools.':
      'Gyűrűdiagram, amely poolonként a tárolt bájtokat mutatja.',
    'Doughnut chart showing volume count across pools.':
      'Gyűrűdiagram, amely poolonként a kötetek számát mutatja.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Hiba- és figyelmeztető sorok a feladatnaplókból az elmúlt 24 órából.',
    'Database size and per-table size breakdown.':
      'Adatbázisméret és táblánkénti méretbontás.',
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
    'Jobs Past 24 h': 'Job delle ultime 24 ore',
    'Recent Jobs': 'Job recenti',
    'Pool Storage (Bytes)': 'Archiviazione pool (byte)',
    'Pool Storage (Volumes)': 'Archiviazione pool (volumi)',
    'Trouble View': 'Vista problemi',
    'Database Table Sizes': 'Dimensioni tabelle del database',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Conteggio riepilogativo dei job avviati nelle ultime 24 ore per stato.',
    'Table showing the most recent job run per job name.':
      "Tabella con l'ultima esecuzione di ogni job.",
    'Live list of currently running jobs with progress.':
      'Elenco in tempo reale dei job attualmente in esecuzione con avanzamento.',
    'Cumulative job, file, and byte totals across all directors.':
      'Totali cumulativi di job, file e byte su tutti i direttori.',
    'Doughnut chart showing bytes stored across pools.':
      'Grafico a ciambella con i byte archiviati per pool.',
    'Doughnut chart showing volume count across pools.':
      'Grafico a ciambella con il numero di volumi per pool.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Righe di errore e avviso dai log dei job nelle ultime 24 ore.',
    'Database size and per-table size breakdown.':
      'Dimensione del database e ripartizione per tabella.',
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
    'Jobs Past 24 h': 'Zadania z ostatnich 24 h',
    'Recent Jobs': 'Ostatnie zadania',
    'Pool Storage (Bytes)': 'Magazyn puli (bajty)',
    'Pool Storage (Volumes)': 'Magazyn puli (woluminy)',
    'Trouble View': 'Widok problemów',
    'Database Table Sizes': 'Rozmiary tabel bazy danych',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Podsumowanie liczby zadań uruchomionych w ostatnich 24 godzinach według statusu.',
    'Table showing the most recent job run per job name.':
      'Tabela pokazująca ostatnie uruchomienie dla każdej nazwy zadania.',
    'Live list of currently running jobs with progress.':
      'Lista na żywo aktualnie uruchomionych zadań wraz z postępem.',
    'Cumulative job, file, and byte totals across all directors.':
      'Skumulowane sumy zadań, plików i bajtów we wszystkich dyrektorach.',
    'Doughnut chart showing bytes stored across pools.':
      'Wykres pierścieniowy pokazujący liczbę bajtów przechowywanych w poszczególnych pulach.',
    'Doughnut chart showing volume count across pools.':
      'Wykres pierścieniowy pokazujący liczbę woluminów w poszczególnych pulach.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Wiersze błędów i ostrzeżeń z dzienników zadań z ostatnich 24 godzin.',
    'Database size and per-table size breakdown.':
      'Rozmiar bazy danych i podział według tabel.',
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
    'Jobs Past 24 h': 'Jobs nas últimas 24 h',
    'Recent Jobs': 'Jobs recentes',
    'Pool Storage (Bytes)': 'Armazenamento de pools (bytes)',
    'Pool Storage (Volumes)': 'Armazenamento de pools (volumes)',
    'Trouble View': 'Visão de problemas',
    'Database Table Sizes': 'Tamanhos das tabelas do banco de dados',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Contagem resumida dos jobs iniciados nas últimas 24 horas por status.',
    'Table showing the most recent job run per job name.':
      'Tabela mostrando a execução mais recente de cada job.',
    'Live list of currently running jobs with progress.':
      'Lista ao vivo dos jobs em execução no momento, com progresso.',
    'Cumulative job, file, and byte totals across all directors.':
      'Totais acumulados de jobs, arquivos e bytes em todos os diretores.',
    'Doughnut chart showing bytes stored across pools.':
      'Gráfico de rosca mostrando os bytes armazenados por pool.',
    'Doughnut chart showing volume count across pools.':
      'Gráfico de rosca mostrando a quantidade de volumes por pool.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Linhas de erro e aviso dos logs de jobs nas últimas 24 horas.',
    'Database size and per-table size breakdown.':
      'Tamanho do banco de dados e detalhamento por tabela.',
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
    'Jobs Past 24 h': 'Задания за последние 24 ч',
    'Recent Jobs': 'Недавние задания',
    'Pool Storage (Bytes)': 'Хранилище пулов (байты)',
    'Pool Storage (Volumes)': 'Хранилище пулов (тома)',
    'Trouble View': 'Обзор проблем',
    'Database Table Sizes': 'Размеры таблиц базы данных',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Сводное количество заданий, запущенных за последние 24 часа, по статусу.',
    'Table showing the most recent job run per job name.':
      'Таблица с последним запуском для каждого имени задания.',
    'Live list of currently running jobs with progress.':
      'Живой список текущих выполняющихся заданий с прогрессом.',
    'Cumulative job, file, and byte totals across all directors.':
      'Суммарные показатели заданий, файлов и байтов по всем директорам.',
    'Doughnut chart showing bytes stored across pools.':
      'Кольцевая диаграмма с объёмом байтов, хранящихся в пулах.',
    'Doughnut chart showing volume count across pools.':
      'Кольцевая диаграмма с количеством томов в пулах.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Строки ошибок и предупреждений из журналов заданий за последние 24 часа.',
    'Database size and per-table size breakdown.':
      'Размер базы данных и разбивка по таблицам.',
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
    'Jobs Past 24 h': 'Joby za posledných 24 h',
    'Recent Jobs': 'Nedávne joby',
    'Pool Storage (Bytes)': 'Úložisko poolu (bajty)',
    'Pool Storage (Volumes)': 'Úložisko poolu (zväzky)',
    'Trouble View': 'Prehľad problémov',
    'Database Table Sizes': 'Veľkosti tabuliek databázy',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Súhrnný počet jobov spustených za posledných 24 hodín podľa stavu.',
    'Table showing the most recent job run per job name.':
      'Tabuľka zobrazujúca posledné spustenie pre každý názov jobu.',
    'Live list of currently running jobs with progress.':
      'Živý zoznam aktuálne bežiacich jobov s priebehom.',
    'Cumulative job, file, and byte totals across all directors.':
      'Kumulatívne súčty jobov, súborov a bajtov naprieč všetkými riaditeľmi.',
    'Doughnut chart showing bytes stored across pools.':
      'Prstencový graf zobrazujúci bajty uložené v jednotlivých pooloch.',
    'Doughnut chart showing volume count across pools.':
      'Prstencový graf zobrazujúci počet zväzkov v jednotlivých pooloch.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Riadky chýb a upozornení z protokolov jobov za posledných 24 hodín.',
    'Database size and per-table size breakdown.':
      'Veľkosť databázy a rozpis podľa tabuliek.',
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
    'Jobs Past 24 h': 'Jobs en las últimas 24 h',
    'Recent Jobs': 'Jobs recientes',
    'Pool Storage (Bytes)': 'Almacenamiento de pools (bytes)',
    'Pool Storage (Volumes)': 'Almacenamiento de pools (volúmenes)',
    'Trouble View': 'Vista de incidencias',
    'Database Table Sizes': 'Tamaños de tablas de la base de datos',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Recuento resumido de jobs iniciados en las últimas 24 horas por estado.',
    'Table showing the most recent job run per job name.':
      'Tabla que muestra la ejecución más reciente de cada job.',
    'Live list of currently running jobs with progress.':
      'Lista en vivo de los jobs actualmente en ejecución con su progreso.',
    'Cumulative job, file, and byte totals across all directors.':
      'Totales acumulados de jobs, archivos y bytes en todos los directores.',
    'Doughnut chart showing bytes stored across pools.':
      'Gráfico de anillos con los bytes almacenados por pool.',
    'Doughnut chart showing volume count across pools.':
      'Gráfico de anillos con el número de volúmenes por pool.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Líneas de error y advertencia de los registros de jobs de las últimas 24 horas.',
    'Database size and per-table size breakdown.':
      'Tamaño de la base de datos y desglose por tabla.',
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
    'Jobs Past 24 h': 'Son 24 saatteki işler',
    'Recent Jobs': 'Son işler',
    'Pool Storage (Bytes)': 'Havuz depolama (bayt)',
    'Pool Storage (Volumes)': 'Havuz depolama (birimler)',
    'Trouble View': 'Sorun görünümü',
    'Database Table Sizes': 'Veritabanı tablo boyutları',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Son 24 saatte başlatılan işlerin duruma göre özet sayıları.',
    'Table showing the most recent job run per job name.':
      'Her iş adı için en son çalıştırmayı gösteren tablo.',
    'Live list of currently running jobs with progress.':
      'Şu anda çalışan işlerin ilerlemesiyle birlikte canlı listesi.',
    'Cumulative job, file, and byte totals across all directors.':
      'Tüm direktörler genelinde birikimli iş, dosya ve bayt toplamları.',
    'Doughnut chart showing bytes stored across pools.':
      'Havuzlar genelinde depolanan baytları gösteren halka grafik.',
    'Doughnut chart showing volume count across pools.':
      'Havuzlar genelinde birim sayısını gösteren halka grafik.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Son 24 saatteki iş günlüklerinden hata ve uyarı satırları.',
    'Database size and per-table size breakdown.':
      'Veritabanı boyutu ve tablo bazında dağılım.',
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
    'Jobs Past 24 h': 'Завдання за останні 24 год',
    'Recent Jobs': 'Останні завдання',
    'Pool Storage (Bytes)': 'Сховище пулів (байти)',
    'Pool Storage (Volumes)': 'Сховище пулів (томи)',
    'Trouble View': 'Огляд проблем',
    'Database Table Sizes': 'Розміри таблиць бази даних',
    'Summary counts of jobs started in the last 24 hours by status.':
      'Зведена кількість завдань, запущених за останні 24 години, за станом.',
    'Table showing the most recent job run per job name.':
      'Таблиця з останнім запуском для кожної назви завдання.',
    'Live list of currently running jobs with progress.':
      'Живий список поточних завдань, що виконуються, з прогресом.',
    'Cumulative job, file, and byte totals across all directors.':
      'Сукупні підсумки завдань, файлів і байтів за всіма директорами.',
    'Doughnut chart showing bytes stored across pools.':
      'Кільцева діаграма з обсягом байтів, збережених у пулах.',
    'Doughnut chart showing volume count across pools.':
      'Кільцева діаграма з кількістю томів у пулах.',
    'Error and warning lines from job logs in the last 24 hours.':
      'Рядки помилок і попереджень з журналів завдань за останні 24 години.',
    'Database size and per-table size breakdown.':
      'Розмір бази даних і розподіл за таблицями.',
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
