This file is not up to date. 
Please refer to bareos.sty regarding defined macros that should be used.

Conventions

Domain: example.com
Hosts:
  * bareos.example.com (if only one system, or one system for Director and Storage Daemon (and webui)
  * bareos-dir.example.com
  * bareos-sd1.example.com, bareos-sd2.example.com, ...
  * client1.example.com, client2.example.com, ... (Clients, where Bareos is running on).

\fileStoragePath
\unixConfigPath

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

Catalog              (Bareos Catalog, Bareos Database)
Configuration        (configure)
Command              (for executabel commands)
Console!Command      (for bconsole commands)
Console!Restore      (for bconsole restore commands)
Debug
Problem              (Troubleshooting)
Restore              (Restoring)
Support
Tape
Windows              (Win32)


Using \index:
\index[index]{first!second!third}

There are 4 indexes: general, dir, sd, fd, console, monitor.
The default is general. Maybe the other indexes will get integrated into general.
It is possible to define a hierarchy of index words, separated by "!".
