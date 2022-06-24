# Julian Bittner Task

### Aufgabenstellung
1. Compile Bareos and run some or all systemtests.
2. Explain how you would improve the behavior of setdebug.
3. Find the functions in the code-base that handles this functionality. Where is it called? What other functions does it call?
4. Propose a way to implement the behaviour you described in (2.)
5. Implement and test.
6. Did you find (or can you think of) any other bugs? Test and describe what you found out.
7. Create a pull request with your changes on GitHub.

***

## 2. Aufgabe
Entweder verstehe ich etwas nicht, oder die Problemstellung selbst ist ziemlich unsinnig.

--

Halten wir erstmal fest:

1. Ohne weitere angeben ruft `setdebug` einem *event-handler* auf.
    - Der *event-handler* ermöglicht keine Eingabe für den trace Parameter.
    - Der *event-handler* setzt den trace Parameter auch nicht selbstständig auf 1.

2. Es ist möglich `setdebug` mit einem *full command* in der Form `setdebug level=100 dir trace=1` aufzurufen
    - hier ist es möglich trace auf 1 zu setzen und die funktion so zu aktivieren.
    - es ist aber auch möglich trace manuell auf 0 zu setzen

3. In der Problemstellung wird der debug Modus ohne trace als nutzlos bezeichnet.

    Zitat: „ ... it asks for a debug level and a daemon, but does not enable tracing, which makes it useless, as without trace=1 no file will be written."

--

Hier kommen mir nun zwei fragen in den Sinn:

 - Wenn der Debug-Modus ohne trace Funktion nutzlos ist, wiso ist der trace Modus dann nicht grundsätzlich per defalt aktiv?
 - und wiso besteht die Möglichkeit diesen abzuschalten?

--

Mein Lösungsansatz sieht also vor den trace modus beim aufruf durch den *event-handler* per default einzuschalten.

***

## 3. Aufgabe
Ich führte eine Keyword suche mit dem begriff `setdebug` über die gesammte Code-Base durch und wurde schnell in der Datei `core/src/dird/ua_cmds.cc` in Zeile 1402 fündig.

***

## 4. Aufgabe
Nun werden Sie mir hoffentlich nachsehen, dass ich mich nicht komplett in Ihre Codebase einarbeite um den *event-handler* um eine weitere Abfrage zu erweitern. Das würde den Rahmen einer einfachen `pre interview task` sprengen.

So wie sich mir die Funktion `SetdebugCmd` darstellt, prüft diese erst ob mit dem `setdebug` Befehl argumente übergeben wurden:

 - Ist dies der Fall, übernimmt sie diese und returnt anschließend true.
 - Ist dies nicht der Fall, landet die Funktion in der *event-handler* Sktion.
<br>

Um das Problem des *event-handler* zu beheben, würde es also genügen das `trace_flag` vor dessen Sektion einfach mit einer 1 zu beschreiben.
<br>

***

## 5. Aufgabe
Da bei jedem Aufruf von `SetdebugCmd`, bei nicht angebe des trace Arguments, in Zeile 1428 das `trace_flag` mit -1 überschrieben wird, um *no changes* zu signalisieren, habe ich hier einfach das Minus entfernt. So wird nicht länger *no changes* signalisiert sondern das `trace_flag`, wenn nichts weiter angegeben wird, auf 1 gesetzt.

Der Test Zeigt, dass die trace Funktion (unsinnigerweise) nachwievor von einem *full command* abgeschaltet werden kann, während es beim Aufruf über den *event-handler* per defalt aktiv ist.

***

## 6. Aufgabe
Auf diese Frage möchte ich in Drei Schritten eingehen.

- Mit Kritik an der Aufgabenstellung
- mit Beantwortung der eigentlichen Frage und
- mit Kritik an der Codebase

--

### Kritik an der Aufgabe
Die Aufgabenstellung sowie die *instructions* verraten rein gar nichts über die Software selbst. Es wird ganz klar vorrausgesetzt, dass man sich im Vorfeld längere Zeit mit Ihrem Produkt auseinander gesetzt hat. Ich habe über eine Stunde mit dem `Introduction and Tutorial` Teil Ihrer Webseite verbracht und verstehe dennoch nicht wirklich an welchem Stück Code ich da gearbeitet habe oder wie die Test-Software im Deteil funktionieren.

--

### Gefundene Bugs und Verbesserungsvorschläge
Zunächst einmal, um wirklich eine Fehlersuche durchzuführen, Bugs zu finden oder Designfehler festzustellen, müsste ich verstehen wie die Test-Software im Deteil funktioniert. Ich kann schlicht nicht beurteilen ob sich die Software falsch verhält oder nicht.

Dennoch geht für mich aus der Problemstellung hervor, dass das trace_flag gar nicht manipuliert werden sollte. Es sollte permanent auf 1 stehen, wodurch es eigentlich obsolet wird.

Das `trace_flag` wird bis zur Funktion `SetTrace` als Integer durchgeschleift, um dann durch eine dreiztufige Fehlerbehandlung in einen Boolean Type umgewandelt zu werden. Scheinbar wird dies nur getan, um den den Wert -1 als fehlercode verwenden zu können. Der Fehlercode -1 wird jedoch nur verwendet um *no changes* zu signalisieren. Ich denke das diese Verwendung des Fehlercoes unsinnig und nicht nötig ist. Bei einem besseren Design mit einem Boolean Typ würde die gesammte fehlerbehandlung des trace_flag weckfallen.

--

### Kritik an der Codebase
Auf meiner suche nach der `SetdebugCmd` Funktion bin ich auf einigen Code gestoßen der mir Bauchschmerzen bereitet.

- Die Codebase die ich bei meiner kleinen *review* gesehen habe ist wirklich miserabel Dokumentiert. Es wird, wenn überhaupt nur hier und da mal ein Kommentar geschrieben, in den meisten Fällen ist der Code jedoch völlig unkommentiert. Die Compilation Units haben nichteinmal einen Hader der zumindest grob beschreibt was diese eigentlich tun. Als Robert Cecil Martin 2008 sein Buch veröffentlichte, war es nicht seine Absicht die Programmierer komplett vom Kommentieren ihres Codes abzubringen. Clean Code beschreibt viel mehr den sorgsamen und sorgfältigen Umgang mit Kommentaren.

- Der Code in der Codebase spricht nicht wirklich führ sich selbst. Die Bezeichner sind teilweise so unglücklich oder kryptisch gewählt, dass man ohne Kommentare wirklich in die Tiefe gehen muss, um nachzuvollziehen was die Funktion oder die Klasse eigentlich macht (das ist ein rekursives Problem!). Die Codebase die ich gesehen habe, spricht starken "Dialekt".

- Im *Header-File* `core/src/include/baconfig.h` wird das Makro `N_(s)` definiert. Es tut rein garnichts, wird aber überall verwendet.

- An einigen Stellen sind Makros zu finden die man eher in C Code erwarten würde, mit dem C++17 standard sind diese jedoch obsolet.

- `typedef` an Stellen wo man `using` nutzen sollte.

- Im *Header-File* `core/src/include/baconfig.h` Zeile 112 wird ein Makro definiert dessen Bezeichner einfach nur aus einem Unterstrich besteht. Warum dies sehr schlechter *code style* ist, darauf muss ich an dieser Stelle nicht eingehen.

- die Quell-Code Datei `dir_cmd.cc` befindet sich zwei mal in der Codebase! im Verzeichnis `core/src/stored` und `core/src/tests`. Sie scheinen sogar teilweise den selben Code zu enthalten. Es wirkt so als ob sie sich in zwei verschiedenen Stadien der Entwicklung befinden. Mir fällt wirklich überhaupt kein sinvoller Grund ein wesshalb man soetwas machen sollte. Das führt lediglich zu Problemen.

Wenn dies nur ein paar Ausnahmen wären, könnte man vielleicht sagen: „ok, alles halb so schlimm". Doch dies ist nur das Resultat einer Halben Stunde review, länger habe ich nähmlich nicht mit der Suche verbracht.