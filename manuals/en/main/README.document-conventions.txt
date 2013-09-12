Conventions

Domain: example.com
Hosts: ???

\fileStoragePath
\unixConfigPath

Links, internal:
\ilink{Description}{LaTex-Label}

Links, external
\elink{Description}{URL}

\newcommand{\command}[1]{\path|#1|}
\newcommand{\bcommand}[1]{\path|#1|}
\newcommand{\file}[1]{\path|#1|}
\newcommand{\user}[1]{\path|#1|}
\newcommand{\group}[1]{\path|#1|}
\newcommand{\variable}[1]{\path|#1|}
\newcommand{\parameter}[1]{\path|#1|}
\newcommand{\configdirective}[1]{\textbf{#1}}


Bareos configuration files:
\begin{bconfig}{<CAPTION>}
<configuration content 1>
<configuration content 2>
\end{bconfig}

\begin{bconfig}{}
\end{bconfig}

Bareos bconsole in- and output:
\begin{bconsole}{prompt}{command}{caption (caption is "command", if not given)}{input}
output
\end{bconsole}

\begin{bconsole}{}{}{}{}
\end{bconsole}

Unix (Linux) Command with output:
\begin{commandOut}{[caption]}{[prompt]}{command}
output
\end{commandOut}

\begin{commandOut}{}{}{}
\end{commandOut}



Index:
rules for using index:
  use singular
  first letter: upper case letters
  everything to [general] (in the longterm)

index keywords (instead of ...)

Windows (Win32)
Configuration (configure)
Command (for executabel commands)
Console!Command (for bconsole commands)
Restore (Restoring)
Catalog (Bareos Catalog, Bareos Database)
Problem (Troubleshooting)
