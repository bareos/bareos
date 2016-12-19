" Vimball Archiver by Charles E. Campbell, Jr., Ph.D.
UseVimball
finish
plugin/AlignPlugin.vim	[[[1
41
" AlignPlugin: tool to align multiple fields based on one or more separators
"   Author:	 Charles E. Campbell, Jr.
"   Date:    Nov 02, 2008
" GetLatestVimScripts: 294 1 :AutoInstall: Align.vim
" GetLatestVimScripts: 1066 1 :AutoInstall: cecutil.vim
" Copyright:    Copyright (C) 1999-2007 Charles E. Campbell, Jr. {{{1
"               Permission is hereby granted to use and distribute this code,
"               with or without modifications, provided that this copyright
"               notice is copied with it. Like anything else that's free,
"               Align.vim is provided *as is* and comes with no warranty
"               of any kind, either expressed or implied. By using this
"               plugin, you agree that in no event will the copyright
"               holder be liable for any damages resulting from the use
"               of this software.
"
" Romans 1:16,17a : For I am not ashamed of the gospel of Christ, for it is {{{1
" the power of God for salvation for everyone who believes; for the Jew first,
" and also for the Greek.  For in it is revealed God's righteousness from
" faith to faith.
" ---------------------------------------------------------------------
" Load Once: {{{1
if &cp || exists("g:loaded_AlignPlugin")
 finish
endif
let g:loaded_AlignPlugin = "v35"
let s:keepcpo            = &cpo
set cpo&vim

" ---------------------------------------------------------------------
" Public Interface: {{{1
com! -bang -range -nargs=* Align <line1>,<line2>call Align#Align(<bang>0,<q-args>)
com!       -range -nargs=0 AlignReplaceQuotedSpaces <line1>,<line2>call Align#AlignReplaceQuotedSpaces()
com!              -nargs=* AlignCtrl call Align#AlignCtrl(<q-args>)
com!              -nargs=0 AlignPush call Align#AlignPush()
com!              -nargs=0 AlignPop  call Align#AlignPop()

" ---------------------------------------------------------------------
"  Restore: {{{1
let &cpo= s:keepcpo
unlet s:keepcpo
" vim: ts=4 fdm=marker
plugin/AlignMapsPlugin.vim	[[[1
242
" AlignMapsPlugin:   Alignment maps based upon <Align.vim> and <AlignMaps.vim>
" Maintainer:        Dr. Charles E. Campbell, Jr. <NdrOchipS@PcampbellAfamily.Mbiz>
" Date:              Mar 03, 2009
"
" NOTE: the code herein needs vim 6.0 or later
"                       needs <Align.vim> v6 or later
"                       needs <cecutil.vim> v5 or later
" Copyright:    Copyright (C) 1999-2008 Charles E. Campbell, Jr. {{{1
"               Permission is hereby granted to use and distribute this code,
"               with or without modifications, provided that this copyright
"               notice is copied with it. Like anything else that's free,
"               AlignMaps.vim is provided *as is* and comes with no warranty
"               of any kind, either expressed or implied. By using this
"               plugin, you agree that in no event will the copyright
"               holder be liable for any damages resulting from the use
"               of this software.
"
" Usage: {{{1
" Use 'a to mark beginning of to-be-aligned region,   Alternative:  use V
" move cursor to end of region, and execute map.      (linewise visual mode) to
" The maps also set up marks 'y and 'z, and retain    mark region, execute same
" 'a at the beginning of region.                      map.  Uses 'a, 'y, and 'z.
"
" The start/end wrappers save and restore marks 'y and 'z.
"
" Although the comments indicate the maps use a leading backslash,
" actually they use <Leader> (:he mapleader), so the user can
" specify that the maps start how he or she prefers.
"
" Note: these maps all use <Align.vim>.
"
" Romans 1:20 For the invisible things of Him since the creation of the {{{1
" world are clearly seen, being perceived through the things that are
" made, even His everlasting power and divinity; that they may be
" without excuse.

" ---------------------------------------------------------------------
" Load Once: {{{1
if &cp || exists("g:loaded_AlignMapsPlugin")
 finish
endif
let s:keepcpo                = &cpo
let g:loaded_AlignMapsPlugin = "v41"
set cpo&vim

" =====================================================================
"  Maps: {{{1

" ---------------------------------------------------------------------
" WS: wrapper start map (internal)  {{{2
" Produces a blank line above and below, marks with 'y and 'z
if !hasmapto('<Plug>WrapperStart')
 map <unique> <SID>WS	<Plug>AlignMapsWrapperStart
endif
nmap <silent> <script> <Plug>AlignMapsWrapperStart	:set lz<CR>:call AlignMaps#WrapperStart(0)<CR>
vmap <silent> <script> <Plug>AlignMapsWrapperStart	:<c-u>set lz<CR>:call AlignMaps#WrapperStart(1)<CR>

" ---------------------------------------------------------------------
" WE: wrapper end (internal)   {{{2
" Removes guard lines, restores marks y and z, and restores search pattern
if !hasmapto('<Plug>WrapperEnd')
 nmap <unique> <SID>WE	<Plug>AlignMapsWrapperEnd
endif
nmap <silent> <script> <Plug>AlignMapsWrapperEnd	:call AlignMaps#WrapperEnd()<CR>:set nolz<CR>

" ---------------------------------------------------------------------
" Complex C-code alignment maps: {{{2
if !hasmapto('<Plug>AM_a?')   |map <unique> <Leader>a?		<Plug>AM_a?|endif
if !hasmapto('<Plug>AM_a,')   |map <unique> <Leader>a,		<Plug>AM_a,|endif
if !hasmapto('<Plug>AM_a<')   |map <unique> <Leader>a<		<Plug>AM_a<|endif
if !hasmapto('<Plug>AM_a=')   |map <unique> <Leader>a=		<Plug>AM_a=|endif
if !hasmapto('<Plug>AM_a(')   |map <unique> <Leader>a(		<Plug>AM_a(|endif
if !hasmapto('<Plug>AM_abox') |map <unique> <Leader>abox	<Plug>AM_abox|endif
if !hasmapto('<Plug>AM_acom') |map <unique> <Leader>acom	<Plug>AM_acom|endif
if !hasmapto('<Plug>AM_adcom')|map <unique> <Leader>adcom	<Plug>AM_adcom|endif
if !hasmapto('<Plug>AM_aocom')|map <unique> <Leader>aocom	<Plug>AM_aocom|endif
if !hasmapto('<Plug>AM_ascom')|map <unique> <Leader>ascom	<Plug>AM_ascom|endif
if !hasmapto('<Plug>AM_adec') |map <unique> <Leader>adec	<Plug>AM_adec|endif
if !hasmapto('<Plug>AM_adef') |map <unique> <Leader>adef	<Plug>AM_adef|endif
if !hasmapto('<Plug>AM_afnc') |map <unique> <Leader>afnc	<Plug>AM_afnc|endif
if !hasmapto('<Plug>AM_afnc') |map <unique> <Leader>afnc	<Plug>AM_afnc|endif
if !hasmapto('<Plug>AM_aunum')|map <unique> <Leader>aunum	<Plug>AM_aenum|endif
if !hasmapto('<Plug>AM_aenum')|map <unique> <Leader>aenum	<Plug>AM_aunum|endif
if exists("g:alignmaps_euronumber") && !exists("g:alignmaps_usanumber")
 if !hasmapto('<Plug>AM_anum')|map <unique> <Leader>anum	<Plug>AM_aenum|endif
else
 if !hasmapto('<Plug>AM_anum')|map <unique> <Leader>anum	<Plug>AM_aunum|endif
endif

map <silent> <script> <Plug>AM_a?		<SID>WS:AlignCtrl mIp1P1lC ? : : : : <CR>:'a,.Align<CR>:'a,'z-1s/\(\s\+\)? /?\1/e<CR><SID>WE
map <silent> <script> <Plug>AM_a,		<SID>WS:'y,'zs/\(\S\)\s\+/\1 /ge<CR>'yjma'zk:call AlignMaps#CharJoiner(",")<cr>:silent 'y,'zg/,/call AlignMaps#FixMultiDec()<CR>'z:exe "norm \<Plug>AM_adec"<cr><SID>WE
map <silent> <script> <Plug>AM_a<		<SID>WS:AlignCtrl mIp1P1=l << >><CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_a(       <SID>WS:AlignCtrl mIp0P1=l<CR>:'a,.Align [(,]<CR>:sil 'y+1,'z-1s/\(\s\+\),/,\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_a=		<SID>WS:AlignCtrl mIp1P1=l<CR>:AlignCtrl g :=<CR>:'a,'zAlign :\==<CR><SID>WE
map <silent> <script> <Plug>AM_abox		<SID>WS:let g:alignmaps_iws=substitute(getline("'a"),'^\(\s*\).*$','\1','e')<CR>:'a,'z-1s/^\s\+//e<CR>:'a,'z-1s/^.*$/@&@/<CR>:AlignCtrl m=p01P0w @<CR>:'a,.Align<CR>:'a,'z-1s/@/ * /<CR>:'a,'z-1s/@$/*/<CR>'aYP:s/./*/g<CR>0r/'zkYp:s/./*/g<CR>0r A/<Esc>:exe "'a-1,'z-1s/^/".g:alignmaps_iws."/e"<CR><SID>WE
map <silent> <script> <Plug>AM_acom		<SID>WS:'a,.s/\/[*/]\/\=/@&@/e<CR>:'a,.s/\*\//@&/e<CR>:'y,'zs/^\( *\) @/\1@/e<CR>'zk:call AlignMaps#StdAlign(2)<CR>:'y,'zs/^\(\s*\) @/\1/e<CR>:'y,'zs/ @//eg<CR><SID>WE
map <silent> <script> <Plug>AM_adcom	<SID>WS:'a,.v/^\s*\/[/*]/s/\/[*/]\*\=/@&@/e<CR>:'a,.v/^\s*\/[/*]/s/\*\//@&/e<CR>:'y,'zv/^\s*\/[/*]/s/^\( *\) @/\1@/e<CR>'zk:call AlignMaps#StdAlign(3)<cr>:'y,'zv/^\s*\/[/*]/s/^\(\s*\) @/\1/e<CR>:'y,'zs/ @//eg<CR><SID>WE
map <silent> <script> <Plug>AM_aocom	<SID>WS:AlignPush<CR>:AlignCtrl g /[*/]<CR>:exe "norm \<Plug>AM_acom"<cr>:AlignPop<CR><SID>WE
map <silent> <script> <Plug>AM_ascom	<SID>WS:'a,.s/\/[*/]/@&@/e<CR>:'a,.s/\*\//@&/e<CR>:silent! 'a,.g/^\s*@\/[*/]/s/@//ge<CR>:AlignCtrl v ^\s*\/[*/]<CR>:AlignCtrl g \/[*/]<CR>'zk:call AlignMaps#StdAlign(2)<cr>:'y,'zs/^\(\s*\) @/\1/e<CR>:'y,'zs/ @//eg<CR><SID>WE
map <silent> <script> <Plug>AM_adec		<SID>WS:'a,'zs/\([^ \t/(]\)\([*&]\)/\1 \2/e<CR>:'y,'zv/^\//s/\([^ \t]\)\s\+/\1 /ge<CR>:'y,'zv/^\s*[*/]/s/\([^/][*&]\)\s\+/\1/ge<CR>:'y,'zv/^\s*[*/]/s/^\(\s*\%(\K\k*\s\+\%([a-zA-Z_*(&]\)\@=\)\+\)\([*(&]*\)\s*\([a-zA-Z0-9_()]\+\)\s*\(\(\[.\{-}]\)*\)\s*\(=\)\=\s*\(.\{-}\)\=\s*;/\1@\2#@\3\4@\6@\7;@/e<CR>:'y,'zv/^\s*[*/]/s/\*\/\s*$/@*\//e<CR>:'y,'zv/^\s*[*/]/s/^\s\+\*/@@@@@* /e<CR>:'y,'zv/^\s*[*/]/s/^@@@@@\*\(.*[^*/]\)$/&@*/e<CR>'yjma'zk:AlignCtrl v ^\s*[*/#]<CR>:call AlignMaps#StdAlign(1)<cr>:'y,'zv/^\s*[*/]/s/@ //ge<CR>:'y,'zv/^\s*[*/]/s/\(\s*\);/;\1/e<CR>:'y,'zv/^#/s/# //e<CR>:'y,'zv/^\s\+[*/#]/s/\([^/*]\)\(\*\+\)\( \+\)/\1\3\2/e<CR>:'y,'zv/^\s\+[*/#]/s/\((\+\)\( \+\)\*/\2\1*/e<CR>:'y,'zv/^\s\+[*/#]/s/^\(\s\+\) \*/\1*/e<CR>:'y,'zv/^\s\+[*/#]/s/[ \t@]*$//e<CR>:'y,'zs/^[*]/ */e<CR><SID>WE
map <silent> <script> <Plug>AM_adef		<SID>WS:AlignPush<CR>:AlignCtrl v ^\s*\(\/\*\<bar>\/\/\)<CR>:'a,.v/^\s*\(\/\*\<bar>\/\/\)/s/^\(\s*\)#\(\s\)*define\s*\(\I[a-zA-Z_0-9(),]*\)\s*\(.\{-}\)\($\<Bar>\/\*\)/#\1\2define @\3@\4@\5/e<CR>:'a,.v/^\s*\(\/\*\<bar>\/\/\)/s/\($\<Bar>\*\/\)/@&/e<CR>'zk:call AlignMaps#StdAlign(1)<cr>'yjma'zk:'a,.v/^\s*\(\/\*\<bar>\/\/\)/s/ @//g<CR><SID>WE
map <silent> <script> <Plug>AM_afnc		:<c-u>set lz<CR>:silent call AlignMaps#Afnc()<CR>:set nolz<CR>
map <silent> <script> <Plug>AM_aunum	<SID>WS:'a,'zs/\%([0-9.]\)\s\+\zs\([-+.]\=\d\)/@\1/ge<CR>:'a,'zs/\(\(^\|\s\)\d\+\)\(\s\+\)@/\1@\3@/ge<CR>:'a,'zs/\.@/\.0@/ge<CR>:AlignCtrl wmp0P0r<CR>:'a,'zAlign [.@]<CR>:'a,'zs/@/ /ge<CR>:'a,'zs/\(\.\)\(\s\+\)\([0-9.,eE+]\+\)/\1\3\2/ge<CR>:'a,'zs/\([eE]\)\(\s\+\)\([0-9+\-+]\+\)/\1\3\2/ge<CR><SID>WE
map <silent> <script> <Plug>AM_aenum	<SID>WS:'a,'zs/\%([0-9.]\)\s\+\([-+]\=\d\)/\1@\2/ge<CR>:'a,'zs/\.@/\.0@/ge<CR>:AlignCtrl wmp0P0r<CR>:'a,'zAlign [,@]<CR>:'a,'zs/@/ /ge<CR>:'a,'zs/\(,\)\(\s\+\)\([-0-9.,eE+]\+\)/\1\3\2/ge<CR>:'a,'zs/\([eE]\)\(\s\+\)\([0-9+\-+]\+\)/\1\3\2/ge<CR><SID>WE

" ---------------------------------------------------------------------
" html table alignment	{{{2
if !hasmapto('<Plug>AM_Htd')|map <unique> <Leader>Htd	<Plug>AM_Htd|endif
map <silent> <script> <Plug>AM_Htd <SID>WS:'y,'zs%<[tT][rR]><[tT][dD][^>]\{-}>\<Bar></[tT][dD]><[tT][dD][^>]\{-}>\<Bar></[tT][dD]></[tT][rR]>%@&@%g<CR>'yjma'zk:AlignCtrl m=Ilp1P0 @<CR>:'a,.Align<CR>:'y,'zs/ @/@/<CR>:'y,'zs/@ <[tT][rR]>/<[tT][rR]>/ge<CR>:'y,'zs/@//ge<CR><SID>WE

" ---------------------------------------------------------------------
" character-based right-justified alignment maps {{{2
if !hasmapto('<Plug>AM_T|')|map <unique> <Leader>T|		<Plug>AM_T||endif
if !hasmapto('<Plug>AM_T#')	 |map <unique> <Leader>T#		<Plug>AM_T#|endif
if !hasmapto('<Plug>AM_T,')	 |map <unique> <Leader>T,		<Plug>AM_T,o|endif
if !hasmapto('<Plug>AM_Ts,') |map <unique> <Leader>Ts,		<Plug>AM_Ts,|endif
if !hasmapto('<Plug>AM_T:')	 |map <unique> <Leader>T:		<Plug>AM_T:|endif
if !hasmapto('<Plug>AM_T;')	 |map <unique> <Leader>T;		<Plug>AM_T;|endif
if !hasmapto('<Plug>AM_T<')	 |map <unique> <Leader>T<		<Plug>AM_T<|endif
if !hasmapto('<Plug>AM_T=')	 |map <unique> <Leader>T=		<Plug>AM_T=|endif
if !hasmapto('<Plug>AM_T?')	 |map <unique> <Leader>T?		<Plug>AM_T?|endif
if !hasmapto('<Plug>AM_T@')	 |map <unique> <Leader>T@		<Plug>AM_T@|endif
if !hasmapto('<Plug>AM_Tab') |map <unique> <Leader>Tab		<Plug>AM_Tab|endif
if !hasmapto('<Plug>AM_Tsp') |map <unique> <Leader>Tsp		<Plug>AM_Tsp|endif
if !hasmapto('<Plug>AM_T~')	 |map <unique> <Leader>T~		<Plug>AM_T~|endif

map <silent> <script> <Plug>AM_T| <SID>WS:AlignCtrl mIp0P0=r <Bar><CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_T#   <SID>WS:AlignCtrl mIp0P0=r #<CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_T,   <SID>WS:AlignCtrl mIp0P1=r ,<CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_Ts,  <SID>WS:AlignCtrl mIp0P1=r ,<CR>:'a,.Align<CR>:'a,.s/\(\s*\),/,\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_T:   <SID>WS:AlignCtrl mIp1P1=r :<CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_T;   <SID>WS:AlignCtrl mIp0P0=r ;<CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_T<   <SID>WS:AlignCtrl mIp0P0=r <<CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_T=   <SID>WS:'a,'z-1s/\s\+\([*/+\-%<Bar>&\~^]\==\)/ \1/e<CR>:'a,'z-1s@ \+\([*/+\-%<Bar>&\~^]\)=@\1=@ge<CR>:'a,'z-1s/; */;@/e<CR>:'a,'z-1s/==/\="\<Char-0x0f>\<Char-0x0f>"/ge<CR>:'a,'z-1s/!=/\x="!\<Char-0x0f>"/ge<CR>:AlignCtrl mIp1P1=r = @<CR>:AlignCtrl g =<CR>:'a,'z-1Align<CR>:'a,'z-1s/; *@/;/e<CR>:'a,'z-1s/; *$/;/e<CR>:'a,'z-1s@\([*/+\-%<Bar>&\~^]\)\( \+\)=@\2\1=@ge<CR>:'a,'z-1s/\( \+\);/;\1/ge<CR>:'a,'z-1s/\xff/=/ge<CR><SID>WE:exe "norm <Plug>acom"
map <silent> <script> <Plug>AM_T?   <SID>WS:AlignCtrl mIp0P0=r ?<CR>:'a,.Align<CR>:'y,'zs/ \( *\);/;\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_T@   <SID>WS:AlignCtrl mIp0P0=r @<CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_Tab  <SID>WS:'a,.s/^\(\t*\)\(.*\)/\=submatch(1).escape(substitute(submatch(2),'\t','@','g'),'\')/<CR>:AlignCtrl mI=r @<CR>:'a,.Align<CR>:'y+1,'z-1s/@/ /g<CR><SID>WE
map <silent> <script> <Plug>AM_Tsp  <SID>WS:'a,.s/^\(\s*\)\(.*\)/\=submatch(1).escape(substitute(submatch(2),'\s\+','@','g'),'\')/<CR>:AlignCtrl mI=r @<CR>:'a,.Align<CR>:'y+1,'z-1s/@/ /g<CR><SID>WE
map <silent> <script> <Plug>AM_T~   <SID>WS:AlignCtrl mIp0P0=r ~<CR>:'a,.Align<CR>:'y,'zs/ \( *\);/;\1/ge<CR><SID>WE

" ---------------------------------------------------------------------
" character-based left-justified alignment maps {{{2
if !hasmapto('<Plug>AM_t|')	|map <unique> <Leader>t|	<Plug>AM_t||endif
if !hasmapto('<Plug>AM_t#')		|map <unique> <Leader>t#	<Plug>AM_t#|endif
if !hasmapto('<Plug>AM_t,')		|map <unique> <Leader>t,	<Plug>AM_t,|endif
if !hasmapto('<Plug>AM_t:')		|map <unique> <Leader>t:	<Plug>AM_t:|endif
if !hasmapto('<Plug>AM_t;')		|map <unique> <Leader>t;	<Plug>AM_t;|endif
if !hasmapto('<Plug>AM_t<')		|map <unique> <Leader>t<	<Plug>AM_t<|endif
if !hasmapto('<Plug>AM_t=')		|map <unique> <Leader>t=	<Plug>AM_t=|endif
if !hasmapto('<Plug>AM_ts,')	|map <unique> <Leader>ts,	<Plug>AM_ts,|endif
if !hasmapto('<Plug>AM_ts:')	|map <unique> <Leader>ts:	<Plug>AM_ts:|endif
if !hasmapto('<Plug>AM_ts;')	|map <unique> <Leader>ts;	<Plug>AM_ts;|endif
if !hasmapto('<Plug>AM_ts<')	|map <unique> <Leader>ts<	<Plug>AM_ts<|endif
if !hasmapto('<Plug>AM_ts=')	|map <unique> <Leader>ts=	<Plug>AM_ts=|endif
if !hasmapto('<Plug>AM_w=')		|map <unique> <Leader>w=	<Plug>AM_w=|endif
if !hasmapto('<Plug>AM_t?')		|map <unique> <Leader>t?	<Plug>AM_t?|endif
if !hasmapto('<Plug>AM_t~')		|map <unique> <Leader>t~	<Plug>AM_t~|endif
if !hasmapto('<Plug>AM_t@')		|map <unique> <Leader>t@	<Plug>AM_t@|endif
if !hasmapto('<Plug>AM_m=')		|map <unique> <Leader>m=	<Plug>AM_m=|endif
if !hasmapto('<Plug>AM_tab')	|map <unique> <Leader>tab	<Plug>AM_tab|endif
if !hasmapto('<Plug>AM_tml')	|map <unique> <Leader>tml	<Plug>AM_tml|endif
if !hasmapto('<Plug>AM_tsp')	|map <unique> <Leader>tsp	<Plug>AM_tsp|endif
if !hasmapto('<Plug>AM_tsq')	|map <unique> <Leader>tsq	<Plug>AM_tsq|endif
if !hasmapto('<Plug>AM_tt')		|map <unique> <Leader>tt	<Plug>AM_tt|endif

map <silent> <script> <Plug>AM_t|		<SID>WS:AlignCtrl mIp0P0=l <Bar><CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_t#		<SID>WS:AlignCtrl mIp0P0=l #<CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_t,		<SID>WS:AlignCtrl mIp0P1=l ,<CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_t:		<SID>WS:AlignCtrl mIp1P1=l :<CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_t;		<SID>WS:AlignCtrl mIp0P1=l ;<CR>:'a,.Align<CR>:sil 'y,'zs/\( *\);/;\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_t<		<SID>WS:AlignCtrl mIp0P0=l <<CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_t=		<SID>WS:call AlignMaps#Equals()<CR><SID>WE
map <silent> <script> <Plug>AM_ts,		<SID>WS:AlignCtrl mIp0P1=l #<CR>:'a,.Align<CR>:sil 'y+1,'z-1s/\(\s*\)#/,\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_ts,		<SID>WS:AlignCtrl mIp0P1=l ,<CR>:'a,.Align<CR>:sil 'y+1,'z-1s/\(\s*\),/,\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_ts:		<SID>WS:AlignCtrl mIp1P1=l :<CR>:'a,.Align<CR>:sil 'y+1,'z-1s/\(\s*\):/:\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_ts;		<SID>WS:AlignCtrl mIp1P1=l ;<CR>:'a,.Align<CR>:sil 'y+1,'z-1s/\(\s*\);/;\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_ts<		<SID>WS:AlignCtrl mIp1P1=l <<CR>:'a,.Align<CR>:sil 'y+1,'z-1s/\(\s*\)</<\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_ts=		<SID>WS:AlignCtrl mIp1P1=l =<CR>:'a,.Align<CR>:sil 'y+1,'z-1s/\(\s*\)=/=\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_w=		<SID>WS:'a,'zg/=/s/\s\+\([*/+\-%<Bar>&\~^]\==\)/ \1/e<CR>:'a,'zg/=/s@ \+\([*/+\-%<Bar>&\~^]\)=@\1=@ge<CR>:'a,'zg/=/s/==/\="\<Char-0x0f>\<Char-0x0f>"/ge<CR>:'a,'zg/=/s/!=/\="!\<Char-0x0f>"/ge<CR>'zk:AlignCtrl mWp1P1=l =<CR>:AlignCtrl g =<CR>:'a,'z-1g/=/Align<CR>:'a,'z-1g/=/s@\([*/+\-%<Bar>&\~^!=]\)\( \+\)=@\2\1=@ge<CR>:'a,'z-1g/=/s/\( \+\);/;\1/ge<CR>:'a,'z-1v/^\s*\/[*/]/s/\/[*/]/@&@/e<CR>:'a,'z-1v/^\s*\/[*/]/s/\*\//@&/e<CR>'zk:call AlignMaps#StdAlign(1)<cr>:'y,'zs/^\(\s*\) @/\1/e<CR>:'a,'z-1g/=/s/\xff/=/ge<CR>:'y,'zg/=/s/ @//eg<CR><SID>WE
map <silent> <script> <Plug>AM_t?		<SID>WS:AlignCtrl mIp0P0=l ?<CR>:'a,.Align<CR>:.,'zs/ \( *\);/;\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_t~		<SID>WS:AlignCtrl mIp0P0=l ~<CR>:'a,.Align<CR>:'y,'zs/ \( *\);/;\1/ge<CR><SID>WE
map <silent> <script> <Plug>AM_t@		<SID>WS::call AlignMaps#StdAlign(1)<cr>:<SID>WE
map <silent> <script> <Plug>AM_m=		<SID>WS:'a,'zs/\s\+\([*/+\-%<Bar>&\~^]\==\)/ \1/e<CR>:'a,'zs@ \+\([*/+\-%<Bar>&\~^]\)=@\1=@ge<CR>:'a,'zs/==/\="\<Char-0x0f>\<Char-0x0f>"/ge<CR>:'a,'zs/!=/\="!\<Char-0x0f>"/ge<CR>'zk:AlignCtrl mIp1P1=l =<CR>:AlignCtrl g =<CR>:'a,'z-1Align<CR>:'a,'z-1s@\([*/+\-%<Bar>&\~^!=]\)\( \+\)=@\2\1=@ge<CR>:'a,'z-1s/\( \+\);/;\1/ge<CR>:'a,'z-s/%\ze[^=]/ @%@ /e<CR>'zk:call AlignMaps#StdAlign(1)<cr>:'y,'zs/^\(\s*\) @/\1/e<CR>:'a,'z-1s/\xff/=/ge<CR>:'y,'zs/ @//eg<CR><SID>WE
map <silent> <script> <Plug>AM_tab		<SID>WS:'a,.s/^\(\t*\)\(.*\)$/\=submatch(1).escape(substitute(submatch(2),'\t',"\<Char-0x0f>",'g'),'\')/<CR>:if &ts == 1<bar>exe "AlignCtrl mI=lp0P0 \<Char-0x0f>"<bar>else<bar>exe "AlignCtrl mI=l \<Char-0x0f>"<bar>endif<CR>:'a,.Align<CR>:exe "'y+1,'z-1s/\<Char-0x0f>/".((&ts == 1)? '\t' : ' ')."/g"<CR><SID>WE
map <silent> <script> <Plug>AM_tml		<SID>WS:AlignCtrl mWp1P0=l \\\@<!\\\s*$<CR>:'a,.Align<CR><SID>WE
map <silent> <script> <Plug>AM_tsp		<SID>WS:'a,.s/^\(\s*\)\(.*\)/\=submatch(1).escape(substitute(submatch(2),'\s\+','@','g'),'\')/<CR>:AlignCtrl mI=lp0P0 @<CR>:'a,.Align<CR>:'y+1,'z-1s/@/ /g<CR><SID>WE
map <silent> <script> <Plug>AM_tsq		<SID>WS:'a,.AlignReplaceQuotedSpaces<CR>:'a,.s/^\(\s*\)\(.*\)/\=submatch(1).substitute(submatch(2),'\s\+','@','g')/<CR>:AlignCtrl mIp0P0=l @<CR>:'a,.Align<CR>:'y+1,'z-1s/[%@]/ /g<CR><SID>WE
map <silent> <script> <Plug>AM_tt		<SID>WS:AlignCtrl mIp1P1=l \\\@<!& \\\\<CR>:'a,.Align<CR><SID>WE

" =====================================================================
" Menu Support: {{{1
"   ma ..move.. use menu
"   v V or ctrl-v ..move.. use menu
if has("menu") && has("gui_running") && &go =~ 'm' && !exists("s:firstmenu")
 let s:firstmenu= 1
 if !exists("g:DrChipTopLvlMenu")
  let g:DrChipTopLvlMenu= "DrChip."
 endif
 if g:DrChipTopLvlMenu != ""
  let s:mapleader = exists("g:mapleader")? g:mapleader : '\'
  let s:emapleader= escape(s:mapleader,'\ ')
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.<<\ and\ >><tab>'.s:emapleader.'a<	'.s:mapleader.'a<'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Assignment\ =<tab>'.s:emapleader.'t=	'.s:mapleader.'t='
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Assignment\ :=<tab>'.s:emapleader.'a=	'.s:mapleader.'a='
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Backslashes<tab>'.s:emapleader.'tml	'.s:mapleader.'tml'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Breakup\ Comma\ Declarations<tab>'.s:emapleader.'a,	'.s:mapleader.'a,'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.C\ Comment\ Box<tab>'.s:emapleader.'abox	'.s:mapleader.'abox'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Commas<tab>'.s:emapleader.'t,	'.s:mapleader.'t,'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Commas<tab>'.s:emapleader.'ts,	'.s:mapleader.'ts,'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Commas\ With\ Strings<tab>'.s:emapleader.'tsq	'.s:mapleader.'tsq'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Comments<tab>'.s:emapleader.'acom	'.s:mapleader.'acom'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Comments\ Only<tab>'.s:emapleader.'aocom	'.s:mapleader.'aocom'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Declaration\ Comments<tab>'.s:emapleader.'adcom	'.s:mapleader.'adcom'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Declarations<tab>'.s:emapleader.'adec	'.s:mapleader.'adec'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Definitions<tab>'.s:emapleader.'adef	'.s:mapleader.'adef'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Function\ Header<tab>'.s:emapleader.'afnc	'.s:mapleader.'afnc'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Html\ Tables<tab>'.s:emapleader.'Htd	'.s:mapleader.'Htd'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.(\.\.\.)?\.\.\.\ :\ \.\.\.<tab>'.s:emapleader.'a?	'.s:mapleader.'a?'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Numbers<tab>'.s:emapleader.'anum	'.s:mapleader.'anum'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Numbers\ (American-Style)<tab>'.s:emapleader.'aunum	<Leader>aunum	'.s:mapleader.'aunum	<Leader>aunum'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Numbers\ (Euro-Style)<tab>'.s:emapleader.'aenum	'.s:mapleader.'aenum'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Spaces\ (Left\ Justified)<tab>'.s:emapleader.'tsp	'.s:mapleader.'tsp'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Spaces\ (Right\ Justified)<tab>'.s:emapleader.'Tsp	'.s:mapleader.'Tsp'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Statements\ With\ Percent\ Style\ Comments<tab>'.s:emapleader.'m=	'.s:mapleader.'m='
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Symbol\ <<tab>'.s:emapleader.'t<	'.s:mapleader.'t<'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Symbol\ \|<tab>'.s:emapleader.'t\|	'.s:mapleader.'t|'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Symbol\ @<tab>'.s:emapleader.'t@	'.s:mapleader.'t@'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Symbol\ #<tab>'.s:emapleader.'t#	'.s:mapleader.'t#'
  exe 'menu '.g:DrChipTopLvlMenu.'AlignMaps.Tabs<tab>'.s:emapleader.'tab	'.s:mapleader.'tab'
  unlet s:mapleader
  unlet s:emapleader
 endif
endif

" =====================================================================
"  Restore: {{{1
let &cpo= s:keepcpo
unlet s:keepcpo

" ==============================================================================
"  Modelines: {{{1
" vim: ts=4 nowrap fdm=marker
plugin/cecutil.vim	[[[1
510
" cecutil.vim : save/restore window position
"               save/restore mark position
"               save/restore selected user maps
"  Author:	Charles E. Campbell, Jr.
"  Version:	18b	ASTRO-ONLY
"  Date:	Aug 27, 2008
"
"  Saving Restoring Destroying Marks: {{{1
"       call SaveMark(markname)       let savemark= SaveMark(markname)
"       call RestoreMark(markname)    call RestoreMark(savemark)
"       call DestroyMark(markname)
"       commands: SM RM DM
"
"  Saving Restoring Destroying Window Position: {{{1
"       call SaveWinPosn()        let winposn= SaveWinPosn()
"       call RestoreWinPosn()     call RestoreWinPosn(winposn)
"		\swp : save current window/buffer's position
"		\rwp : restore current window/buffer's previous position
"       commands: SWP RWP
"
"  Saving And Restoring User Maps: {{{1
"       call SaveUserMaps(mapmode,maplead,mapchx,suffix)
"       call RestoreUserMaps(suffix)
"
" GetLatestVimScripts: 1066 1 :AutoInstall: cecutil.vim
"
" You believe that God is one. You do well. The demons also {{{1
" believe, and shudder. But do you want to know, vain man, that
" faith apart from works is dead?  (James 2:19,20 WEB)

" ---------------------------------------------------------------------
" Load Once: {{{1
if &cp || exists("g:loaded_cecutil")
 finish
endif
let g:loaded_cecutil = "v18b"
let s:keepcpo        = &cpo
set cpo&vim
"DechoTabOn

" =======================
"  Public Interface: {{{1
" =======================

" ---------------------------------------------------------------------
"  Map Interface: {{{2
if !hasmapto('<Plug>SaveWinPosn')
 map <unique> <Leader>swp <Plug>SaveWinPosn
endif
if !hasmapto('<Plug>RestoreWinPosn')
 map <unique> <Leader>rwp <Plug>RestoreWinPosn
endif
nmap <silent> <Plug>SaveWinPosn		:call SaveWinPosn()<CR>
nmap <silent> <Plug>RestoreWinPosn	:call RestoreWinPosn()<CR>

" ---------------------------------------------------------------------
" Command Interface: {{{2
com! -bar -nargs=0 SWP	call SaveWinPosn()
com! -bar -nargs=0 RWP	call RestoreWinPosn()
com! -bar -nargs=1 SM	call SaveMark(<q-args>)
com! -bar -nargs=1 RM	call RestoreMark(<q-args>)
com! -bar -nargs=1 DM	call DestroyMark(<q-args>)

if v:version < 630
 let s:modifier= "sil "
else
 let s:modifier= "sil keepj "
endif

" ===============
" Functions: {{{1
" ===============

" ---------------------------------------------------------------------
" SaveWinPosn: {{{2
"    let winposn= SaveWinPosn()  will save window position in winposn variable
"    call SaveWinPosn()          will save window position in b:cecutil_winposn{b:cecutil_iwinposn}
"    let winposn= SaveWinPosn(0) will *only* save window position in winposn variable (no stacking done)
fun! SaveWinPosn(...)
"  call Dfunc("SaveWinPosn() a:0=".a:0)
  if line(".") == 1 && getline(1) == ""
"   call Dfunc("SaveWinPosn : empty buffer")
   return ""
  endif
  let so_keep   = &l:so
  let siso_keep = &siso
  let ss_keep   = &l:ss
  setlocal so=0 siso=0 ss=0

  let swline    = line(".")
  let swcol     = col(".")
  let swwline   = winline() - 1
  let swwcol    = virtcol(".") - wincol()
  let savedposn = "call GoWinbufnr(".winbufnr(0).")|silent ".swline
  let savedposn = savedposn."|".s:modifier."norm! 0z\<cr>"
  if swwline > 0
   let savedposn= savedposn.":".s:modifier."norm! ".swwline."\<c-y>\<cr>"
  endif
  if swwcol > 0
   let savedposn= savedposn.":".s:modifier."norm! 0".swwcol."zl\<cr>"
  endif
  let savedposn = savedposn.":".s:modifier."call cursor(".swline.",".swcol.")\<cr>"

  " save window position in
  " b:cecutil_winposn_{iwinposn} (stack)
  " only when SaveWinPosn() is used
  if a:0 == 0
   if !exists("b:cecutil_iwinposn")
	let b:cecutil_iwinposn= 1
   else
	let b:cecutil_iwinposn= b:cecutil_iwinposn + 1
   endif
"   call Decho("saving posn to SWP stack")
   let b:cecutil_winposn{b:cecutil_iwinposn}= savedposn
  endif

  let &l:so = so_keep
  let &siso = siso_keep
  let &l:ss = ss_keep

"  if exists("b:cecutil_iwinposn")	 " Decho
"   call Decho("b:cecutil_winpos{".b:cecutil_iwinposn."}[".b:cecutil_winposn{b:cecutil_iwinposn}."]")
"  else                      " Decho
"   call Decho("b:cecutil_iwinposn doesn't exist")
"  endif                     " Decho
"  call Dret("SaveWinPosn [".savedposn."]")
  return savedposn
endfun

" ---------------------------------------------------------------------
" RestoreWinPosn: {{{2
"      call RestoreWinPosn()
"      call RestoreWinPosn(winposn)
fun! RestoreWinPosn(...)
"  call Dfunc("RestoreWinPosn() a:0=".a:0)
"  call Decho("getline(1)<".getline(1).">")
"  call Decho("line(.)=".line("."))
  if line(".") == 1 && getline(1) == ""
"   call Dfunc("RestoreWinPosn : empty buffer")
   return ""
  endif
  let so_keep   = &l:so
  let siso_keep = &l:siso
  let ss_keep   = &l:ss
  setlocal so=0 siso=0 ss=0

  if a:0 == 0 || a:1 == ""
   " use saved window position in b:cecutil_winposn{b:cecutil_iwinposn} if it exists
   if exists("b:cecutil_iwinposn") && exists("b:cecutil_winposn{b:cecutil_iwinposn}")
"   	call Decho("using stack b:cecutil_winposn{".b:cecutil_iwinposn."}<".b:cecutil_winposn{b:cecutil_iwinposn}.">")
	try
     exe "silent! ".b:cecutil_winposn{b:cecutil_iwinposn}
	catch /^Vim\%((\a\+)\)\=:E749/
	 " ignore empty buffer error messages
	endtry
    " normally drop top-of-stack by one
    " but while new top-of-stack doesn't exist
    " drop top-of-stack index by one again
	if b:cecutil_iwinposn >= 1
	 unlet b:cecutil_winposn{b:cecutil_iwinposn}
	 let b:cecutil_iwinposn= b:cecutil_iwinposn - 1
	 while b:cecutil_iwinposn >= 1 && !exists("b:cecutil_winposn{b:cecutil_iwinposn}")
	  let b:cecutil_iwinposn= b:cecutil_iwinposn - 1
	 endwhile
	 if b:cecutil_iwinposn < 1
	  unlet b:cecutil_iwinposn
	 endif
	endif
   else
	echohl WarningMsg
	echomsg "***warning*** need to SaveWinPosn first!"
	echohl None
   endif

  else	 " handle input argument
"   call Decho("using input a:1<".a:1.">")
   " use window position passed to this function
   exe "silent ".a:1
   " remove a:1 pattern from b:cecutil_winposn{b:cecutil_iwinposn} stack
   if exists("b:cecutil_iwinposn")
    let jwinposn= b:cecutil_iwinposn
    while jwinposn >= 1                     " search for a:1 in iwinposn..1
        if exists("b:cecutil_winposn{jwinposn}")    " if it exists
         if a:1 == b:cecutil_winposn{jwinposn}      " and the pattern matches
       unlet b:cecutil_winposn{jwinposn}            " unlet it
       if jwinposn == b:cecutil_iwinposn            " if at top-of-stack
        let b:cecutil_iwinposn= b:cecutil_iwinposn - 1      " drop stacktop by one
       endif
      endif
     endif
     let jwinposn= jwinposn - 1
    endwhile
   endif
  endif

  " Seems to be something odd: vertical motions after RWP
  " cause jump to first column.  The following fixes that.
  " Note: was using wincol()>1, but with signs, a cursor
  " at column 1 yields wincol()==3.  Beeping ensued.
  if virtcol('.') > 1
   silent norm! hl
  elseif virtcol(".") < virtcol("$")
   silent norm! lh
  endif

  let &l:so   = so_keep
  let &l:siso = siso_keep
  let &l:ss   = ss_keep

"  call Dret("RestoreWinPosn")
endfun

" ---------------------------------------------------------------------
" GoWinbufnr: go to window holding given buffer (by number) {{{2
"   Prefers current window; if its buffer number doesn't match,
"   then will try from topleft to bottom right
fun! GoWinbufnr(bufnum)
"  call Dfunc("GoWinbufnr(".a:bufnum.")")
  if winbufnr(0) == a:bufnum
"   call Dret("GoWinbufnr : winbufnr(0)==a:bufnum")
   return
  endif
  winc t
  let first=1
  while winbufnr(0) != a:bufnum && (first || winnr() != 1)
	winc w
	let first= 0
   endwhile
"  call Dret("GoWinbufnr")
endfun

" ---------------------------------------------------------------------
" SaveMark: sets up a string saving a mark position. {{{2
"           For example, SaveMark("a")
"           Also sets up a global variable, g:savemark_{markname}
fun! SaveMark(markname)
"  call Dfunc("SaveMark(markname<".a:markname.">)")
  let markname= a:markname
  if strpart(markname,0,1) !~ '\a'
   let markname= strpart(markname,1,1)
  endif
"  call Decho("markname=".markname)

  let lzkeep  = &lz
  set lz

  if 1 <= line("'".markname) && line("'".markname) <= line("$")
   let winposn               = SaveWinPosn(0)
   exe s:modifier."norm! `".markname
   let savemark              = SaveWinPosn(0)
   let g:savemark_{markname} = savemark
   let savemark              = markname.savemark
   call RestoreWinPosn(winposn)
  else
   let g:savemark_{markname} = ""
   let savemark              = ""
  endif

  let &lz= lzkeep

"  call Dret("SaveMark : savemark<".savemark.">")
  return savemark
endfun

" ---------------------------------------------------------------------
" RestoreMark: {{{2
"   call RestoreMark("a")  -or- call RestoreMark(savemark)
fun! RestoreMark(markname)
"  call Dfunc("RestoreMark(markname<".a:markname.">)")

  if strlen(a:markname) <= 0
"   call Dret("RestoreMark : no such mark")
   return
  endif
  let markname= strpart(a:markname,0,1)
  if markname !~ '\a'
   " handles 'a -> a styles
   let markname= strpart(a:markname,1,1)
  endif
"  call Decho("markname=".markname." strlen(a:markname)=".strlen(a:markname))

  let lzkeep  = &lz
  set lz
  let winposn = SaveWinPosn(0)

  if strlen(a:markname) <= 2
   if exists("g:savemark_{markname}") && strlen(g:savemark_{markname}) != 0
	" use global variable g:savemark_{markname}
"	call Decho("use savemark list")
	call RestoreWinPosn(g:savemark_{markname})
	exe "norm! m".markname
   endif
  else
   " markname is a savemark command (string)
"	call Decho("use savemark command")
   let markcmd= strpart(a:markname,1)
   call RestoreWinPosn(markcmd)
   exe "norm! m".markname
  endif

  call RestoreWinPosn(winposn)
  let &lz       = lzkeep

"  call Dret("RestoreMark")
endfun

" ---------------------------------------------------------------------
" DestroyMark: {{{2
"   call DestroyMark("a")  -- destroys mark
fun! DestroyMark(markname)
"  call Dfunc("DestroyMark(markname<".a:markname.">)")

  " save options and set to standard values
  let reportkeep= &report
  let lzkeep    = &lz
  set lz report=10000

  let markname= strpart(a:markname,0,1)
  if markname !~ '\a'
   " handles 'a -> a styles
   let markname= strpart(a:markname,1,1)
  endif
"  call Decho("markname=".markname)

  let curmod  = &mod
  let winposn = SaveWinPosn(0)
  1
  let lineone = getline(".")
  exe "k".markname
  d
  put! =lineone
  let &mod    = curmod
  call RestoreWinPosn(winposn)

  " restore options to user settings
  let &report = reportkeep
  let &lz     = lzkeep

"  call Dret("DestroyMark")
endfun

" ---------------------------------------------------------------------
" QArgSplitter: to avoid \ processing by <f-args>, <q-args> is needed. {{{2
" However, <q-args> doesn't split at all, so this one returns a list
" with splits at all whitespace (only!), plus a leading length-of-list.
" The resulting list:  qarglist[0] corresponds to a:0
"                      qarglist[i] corresponds to a:{i}
fun! QArgSplitter(qarg)
"  call Dfunc("QArgSplitter(qarg<".a:qarg.">)")
  let qarglist    = split(a:qarg)
  let qarglistlen = len(qarglist)
  let qarglist    = insert(qarglist,qarglistlen)
"  call Dret("QArgSplitter ".string(qarglist))
  return qarglist
endfun

" ---------------------------------------------------------------------
" ListWinPosn: {{{2
"fun! ListWinPosn()                                                        " Decho
"  if !exists("b:cecutil_iwinposn") || b:cecutil_iwinposn == 0             " Decho
"   call Decho("nothing on SWP stack")                                     " Decho
"  else                                                                    " Decho
"   let jwinposn= b:cecutil_iwinposn                                       " Decho
"   while jwinposn >= 1                                                    " Decho
"    if exists("b:cecutil_winposn{jwinposn}")                              " Decho
"     call Decho("winposn{".jwinposn."}<".b:cecutil_winposn{jwinposn}.">") " Decho
"    else                                                                  " Decho
"     call Decho("winposn{".jwinposn."} -- doesn't exist")                 " Decho
"    endif                                                                 " Decho
"    let jwinposn= jwinposn - 1                                            " Decho
"   endwhile                                                               " Decho
"  endif                                                                   " Decho
"endfun                                                                    " Decho
"com! -nargs=0 LWP	call ListWinPosn()                                    " Decho

" ---------------------------------------------------------------------
" SaveUserMaps: this function sets up a script-variable (s:restoremap) {{{2
"          which can be used to restore user maps later with
"          call RestoreUserMaps()
"
"          mapmode - see :help maparg for details (n v o i c l "")
"                    ex. "n" = Normal
"                    The letters "b" and "u" are optional prefixes;
"                    The "u" means that the map will also be unmapped
"                    The "b" means that the map has a <buffer> qualifier
"                    ex. "un"  = Normal + unmapping
"                    ex. "bn"  = Normal + <buffer>
"                    ex. "bun" = Normal + <buffer> + unmapping
"                    ex. "ubn" = Normal + <buffer> + unmapping
"          maplead - see mapchx
"          mapchx  - "<something>" handled as a single map item.
"                    ex. "<left>"
"                  - "string" a string of single letters which are actually
"                    multiple two-letter maps (using the maplead:
"                    maplead . each_character_in_string)
"                    ex. maplead="\" and mapchx="abc" saves user mappings for
"                        \a, \b, and \c
"                    Of course, if maplead is "", then for mapchx="abc",
"                    mappings for a, b, and c are saved.
"                  - :something  handled as a single map item, w/o the ":"
"                    ex.  mapchx= ":abc" will save a mapping for "abc"
"          suffix  - a string unique to your plugin
"                    ex.  suffix= "DrawIt"
fun! SaveUserMaps(mapmode,maplead,mapchx,suffix)
"  call Dfunc("SaveUserMaps(mapmode<".a:mapmode."> maplead<".a:maplead."> mapchx<".a:mapchx."> suffix<".a:suffix.">)")

  if !exists("s:restoremap_{a:suffix}")
   " initialize restoremap_suffix to null string
   let s:restoremap_{a:suffix}= ""
  endif

  " set up dounmap: if 1, then save and unmap  (a:mapmode leads with a "u")
  "                 if 0, save only
  let mapmode  = a:mapmode
  let dounmap  = 0
  let dobuffer = ""
  while mapmode =~ '^[bu]'
   if     mapmode =~ '^u'
    let dounmap= 1
    let mapmode= strpart(a:mapmode,1)
   elseif mapmode =~ '^b'
    let dobuffer= "<buffer> "
    let mapmode= strpart(a:mapmode,1)
   endif
  endwhile
"  call Decho("dounmap=".dounmap."  dobuffer<".dobuffer.">")

  " save single map :...something...
  if strpart(a:mapchx,0,1) == ':'
"   call Decho("save single map :...something...")
   let amap= strpart(a:mapchx,1)
   if amap == "|" || amap == "\<c-v>"
    let amap= "\<c-v>".amap
   endif
   let amap                    = a:maplead.amap
   let s:restoremap_{a:suffix} = s:restoremap_{a:suffix}."|:silent! ".mapmode."unmap ".dobuffer.amap
   if maparg(amap,mapmode) != ""
    let maprhs                  = substitute(maparg(amap,mapmode),'|','<bar>','ge')
	let s:restoremap_{a:suffix} = s:restoremap_{a:suffix}."|:".mapmode."map ".dobuffer.amap." ".maprhs
   endif
   if dounmap
	exe "silent! ".mapmode."unmap ".dobuffer.amap
   endif

  " save single map <something>
  elseif strpart(a:mapchx,0,1) == '<'
"   call Decho("save single map <something>")
   let amap       = a:mapchx
   if amap == "|" || amap == "\<c-v>"
    let amap= "\<c-v>".amap
"	call Decho("amap[[".amap."]]")
   endif
   let s:restoremap_{a:suffix} = s:restoremap_{a:suffix}."|silent! ".mapmode."unmap ".dobuffer.amap
   if maparg(a:mapchx,mapmode) != ""
    let maprhs                  = substitute(maparg(amap,mapmode),'|','<bar>','ge')
	let s:restoremap_{a:suffix} = s:restoremap_{a:suffix}."|".mapmode."map ".amap." ".dobuffer.maprhs
   endif
   if dounmap
	exe "silent! ".mapmode."unmap ".dobuffer.amap
   endif

  " save multiple maps
  else
"   call Decho("save multiple maps")
   let i= 1
   while i <= strlen(a:mapchx)
    let amap= a:maplead.strpart(a:mapchx,i-1,1)
	if amap == "|" || amap == "\<c-v>"
	 let amap= "\<c-v>".amap
	endif
	let s:restoremap_{a:suffix} = s:restoremap_{a:suffix}."|silent! ".mapmode."unmap ".dobuffer.amap
    if maparg(amap,mapmode) != ""
     let maprhs                  = substitute(maparg(amap,mapmode),'|','<bar>','ge')
	 let s:restoremap_{a:suffix} = s:restoremap_{a:suffix}."|".mapmode."map ".amap." ".dobuffer.maprhs
    endif
	if dounmap
	 exe "silent! ".mapmode."unmap ".dobuffer.amap
	endif
    let i= i + 1
   endwhile
  endif
"  call Dret("SaveUserMaps : restoremap_".a:suffix.": ".s:restoremap_{a:suffix})
endfun

" ---------------------------------------------------------------------
" RestoreUserMaps: {{{2
"   Used to restore user maps saved by SaveUserMaps()
fun! RestoreUserMaps(suffix)
"  call Dfunc("RestoreUserMaps(suffix<".a:suffix.">)")
  if exists("s:restoremap_{a:suffix}")
   let s:restoremap_{a:suffix}= substitute(s:restoremap_{a:suffix},'|\s*$','','e')
   if s:restoremap_{a:suffix} != ""
"   	call Decho("exe ".s:restoremap_{a:suffix})
    exe "silent! ".s:restoremap_{a:suffix}
   endif
   unlet s:restoremap_{a:suffix}
  endif
"  call Dret("RestoreUserMaps")
endfun

" ==============
"  Restore: {{{1
" ==============
let &cpo= s:keepcpo
unlet s:keepcpo

" ================
"  Modelines: {{{1
" ================
" vim: ts=4 fdm=marker
doc/Align.txt	[[[1
1469
*align.txt*	The Alignment Tool			Mar 04, 2009

Author:    Charles E. Campbell, Jr.  <NdrOchip@ScampbellPfamily.AbizM>
           (remove NOSPAM from Campbell's email first)
Copyright: (c) 2004-2008 by Charles E. Campbell, Jr.	*Align-copyright*
           The VIM LICENSE applies to Align.vim, AlignMaps.vim, and Align.txt
           (see |copyright|) except use "Align and AlignMaps" instead of "Vim"
           NO WARRANTY, EXPRESS OR IMPLIED.  USE AT-YOUR-OWN-RISK.

==============================================================================
1. Contents					*align* *align-contents* {{{1

	1. Contents.................: |align-contents|
	2. Alignment Manual.........: |align-manual|
	3. Alignment Usage..........: |align-usage|
	   Alignment Concepts.......: |align-concepts|
	   Alignment Commands.......: |align-commands|
	   Alignment Control........: |align-control|
	     Separators.............: |alignctrl-separators|
	     Initial Whitespace.....: |alignctrl-w| |alignctrl-W| |alignctrl-I|
	     Justification..........: |alignctrl-l| |alignctrl-r| |alignctrl-c|
	     Justification Control..: |alignctrl--| |alignctrl-+| |alignctrl-:|
	     Cyclic/Sequential......: |alignctrl-=| |alignctrl-C|
	     Separator Justification: |alignctrl-<| |alignctrl->| |alignctrl-||
	     Line (de)Selection.....: |alignctrl-g| |alignctrl-v|
	     Temporary Settings.....: |alignctrl-m|
	     Padding................: |alignctrl-p| |alignctrl-P|
	     Current Options........: |alignctrl-settings| |alignctrl-|
	   Alignment................: |align-align|
	4. Alignment Maps...........: |align-maps|
	     \a,....................: |alignmap-a,|
	     \a?....................: |alignmap-a?|
	     \a<....................: |alignmap-a<|
	     \abox..................: |alignmap-abox|
	     \acom..................: |alignmap-acom|
	     \anum..................: |alignmap-anum|
	     \ascom.................: |alignmap-ascom|
	     \adec..................: |alignmap-adec|
	     \adef..................: |alignmap-adef|
	     \afnc..................: |alignmap-afnc|
	     \adcom.................: |alignmap-adcom|
	     \aocom.................: |alignmap-aocom|
	     \tsp...................: |alignmap-tsp|
	     \tsq...................: |alignmap-tsq|
	     \tt....................: |alignmap-tt|
	     \t=....................: |alignmap-t=|
	     \T=....................: |alignmap-T=|
	     \Htd...................: |alignmap-Htd|
	5. Alignment Tool History...: |align-history|

==============================================================================
2. Align Manual			*alignman* *alignmanual* *align-manual* {{{1

	Align comes as a vimball; simply typing >
		vim Align.vba.gz
		:so %
<	should put its components where they belong.  The components are: >
		.vim/plugin/AlignPlugin.vim
		.vim/plugin/AlignMapsPlugin.vim
		.vim/plugin/cecutil.vim
		.vim/autoload/Align.vim
		.vim/autoload/AlignMaps.vim
		.vim/doc/Align.txt
<	To see a user's guide, see |align-userguide|
	To see examples, see |alignctrl| and |alignmaps|
>
/=============+=========+=====================================================\
||            \ Default/                                                     ||
||  Commands   \ Value/                Explanation                           ||
||              |    |                                                       ||
++==============+====+=======================================================++
||  AlignCtrl   |    |  =Clrc-+:pPIWw [..list-of-separator-patterns..]       ||
||              |    +-------------------------------------------------------+|
||              |    |  may be called as a command or as a function:         ||
||              |    |  :AlignCtrl =lp0P0W & \\                              ||
||              |    |  :call Align#AlignCtrl('=lp0P0W','&','\\')            ||
||              |    |                                                       ||
||              |    +-------------------------------------------------------++
||   1st arg    |  = | =  all separator patterns are equivalent and are      ||
||              |    |    simultaneously active. Patterns are |regexp|.      ||
||              |    | C  cycle through separator patterns.  Patterns are    ||
||              |    |    |regexp| and are active sequentially.              ||
||              |    |                                                       ||
||              |  < | <  left justify separator   Separators are justified, ||
||              |    | >  right justify separator  too.  Separator styles    ||
||              |    | |  center separator         are cyclic.               ||
||              |    |                                                       ||
||              |  l | l  left justify   Justification styles are always     ||
||              |    | r  right justify  cyclic (ie. lrc would mean left j., ||
||              |    | c  center         then right j., then center, repeat. ||
||              |    | -  skip this separator                                ||
||              |    | +  re-use last justification method                   ||
||              |    | :  treat rest of text as a field                      ||
||              |    |                                                       ||
||              | p1 | p### pad separator on left  by # blanks               ||
||              | P1 | P### pad separator on right by # blanks               ||
||              |    |                                                       ||
||              |  I | I  preserve and apply first line's leading white      ||
||              |    |    space to all lines                                 ||
||              |    | W  preserve leading white space on every line, even   ||
||              |    |    if it varies from line to line                     ||
||              |    | w  don't preserve leading white space                 ||
||              |    |                                                       ||
||              |    | g  second argument is a selection pattern -- only     ||
||              |    |    align on lines that have a match  (inspired by     ||
||              |    |    :g/selection pattern/command)                      ||
||              |    | v  second argument is a selection pattern -- only     ||
||              |    |    align on lines that _don't_ have a match (inspired ||
||              |    |    by :v/selection pattern/command)                   ||
||              |    |                                                       ||
||              |    | m  Map support: AlignCtrl will immediately do an      ||
||              |    |    AlignPush() and the next call to Align() will do   ||
||              |    |    an AlignPop at the end.  This feature allows maps  ||
||              |    |    to preserve user settings.                         ||
||              |    |                                                       ||
||              |    | default                                               ||
||              |    |    AlignCtrl default                                  ||
||              |    |    will clear the AlignCtrl                           ||
||              |    |    stack & set the default:  AlignCtrl "Ilp1P1=" '='  ||
||              |    |                                                       ||
||              +----+-------------------------------------------------------+|
||  More args   |  More arguments are interpreted as describing separators   ||
||              +------------------------------------------------------------+|
||   No args    |  AlignCtrl will display its current settings               ||
||==============+============================================================+|
||[range]Align  |   [..list-of-separators..]                                 ||
||[range]Align! |   [AlignCtrl settings] [..list-of-separators..]            ||
||              +------------------------------------------------------------+|
||              |  Aligns text over the given range.  The range may be       ||
||              |  selected via visual mode (v, V, or ctrl-v) or via         ||
||              |  the command line.  The Align operation may be invoked     ||
||              |  as a command or as a function; as a function, the first   ||
||              |  argument is 0=separators only, 1=AlignCtrl option string  ||
||              |  followed by a list of separators.                         ||
||              |   :[range]Align                                            ||
||              |   :[range]Align [list of separators]                       ||
||              |   :[range]call Align#Align(0)                              ||
||              |   :[range]call Align#Align(0,"list","of","separators",...) ||
\=============================================================================/

==============================================================================
3. Alignment Usage	*alignusage* *align-usage* *align-userguide* {{{1


ALIGNMENT CONCEPTS			*align-concept* *align-concepts* {{{2

	The typical text to be aligned is considered to be:

		* composed of two or more fields
		* separated by one or more separator pattern(s):
		* two or more lines
>
		ws field ws separator ws field ws separator ...
		ws field ws separator ws field ws separator ...
<
	where "ws" stands for "white space" such as blanks and/or tabs,
	and "fields" are arbitrary text.  For example, consider >

		x= y= z= 3;
		xx= yy= zz= 4;
		zzz= yyy= zzz= 5;
		a= b= c= 3;
<
	Assume that it is desired to line up all the "=" signs; these,
	then, are the separators.  The fields are composed of all the
	alphameric text.  Assuming they lie on lines 1-4, one may align
	those "=" signs with: >
		:AlignCtrl l
		:1,4Align =
<	The result is: >
		x   = y   = z   = 3;
		xx  = yy  = zz  = 4;
		zzz = yyy = zzz = 5;
		a   = b   = c   = 3;

<	Note how each "=" sign is surrounded by a single space; the
	default padding is p1P1 (p1 means one space before the separator,
	and P1 means one space after it).  If you wish to change the
	padding, say to no padding, use  (see |alignctrl-p|) >
		:AlignCtrl lp0P0

<	Next, note how each field is left justified; that's what the "l"
	(a small letter "ell") does.  If right-justification of the fields
	had been desired, an "r" could've been used: >
		:AlignCtrl r
<	yielding >
		  x =   y =   z = 3;
		 xx =  yy =  zz = 4;
		zzz = yyy = zzz = 5;
		  a =   b =   c = 3;
<	There are many more options available for field justification: see
	|alignctrl-c| and |alignctrl--|.

	Separators, although commonly only one character long, are actually
	specified by regular expressions (see |regexp|), and one may left
	justify, right justify, or center them, too (see |alignctrl-<|).

	Assume that for some reason a left-right-left-right-... justification
	sequence was wished.  This wish is simply achieved with >
		:AlignCtrl lr
		:1,4Align =
<	because the justification commands are considered to be "cylic"; ie.
	lr is the same as lrlrlrlrlrlrlr...

	There's a lot more discussed under |alignctrl|; hopefully the examples
	there will help, too.


ALIGNMENT COMMANDS			*align-command* *align-commands* {{{2

        The <Align.vim> script includes two primary commands and two
	minor commands:

	  AlignCtrl : this command/function sets up alignment options
	              which persist until changed for later Align calls.
		      It controls such things as: how to specify field
		      separators, initial white space, padding about
		      separators, left/right/center justification, etc. >
			ex.  AlignCtrl wp0P1
                             Interpretation: during subsequent alignment
			     operations, preserve each line's initial
			     whitespace.  Use no padding before separators
			     but provide one padding space after separators.
<
	  Align     : this command/function operates on the range given it to
		      align text based on one or more separator patterns.  The
		      patterns may be provided via AlignCtrl or via Align
		      itself. >

			ex. :%Align ,
			    Interpretation: align all commas over the entire
			    file.
<		      The :Align! format permits alignment control commands
		      to precede the alignment patterns. >
			ex. :%Align! p2P2 =
<		      This will align all "=" in the file with two padding
		      spaces on both sides of each "=" sign.

		      NOTE ON USING PATTERNS WITH ALIGN:~
		      Align and AlignCtrl use |<q-args>| to obtain their
		      input patterns and they use an internal function to
		      split arguments at whitespace unless inside "..."s.
		      One may escape characters inside a double-quote string
		      by preceding such characters with a backslash.

	  AlignPush : this command/function pushes the current AlignCtrl
	              state onto an internal stack. >
			ex. :AlignPush
			    Interpretation: save the current AlignCtrl
			    settings, whatever they may be.  They'll
			    also remain as the current settings until
			    AlignCtrl is used to change them.
<
	  AlignPop  : this command/function pops the current AlignCtrl
	              state from an internal stack. >
			ex. :AlignPop
			    Interpretation: presumably AlignPush was
			    used (at least once) previously; this command
			    restores the AlignCtrl settings when AlignPush
			    was last used.
<	              Also see |alignctrl-m| for a way to automatically do
	              an AlignPop after an Align (primarily this is for maps).

ALIGNMENT OPTIONS			*align-option* *align-options* {{{2
    *align-utf8* *align-utf* *align-codepoint* *align-strlen* *align-multibyte*

	For those of you who are using 2-byte (or more) characters such as are
	available with utf-8, Align now provides a special option which you
	may choose based upon your needs:

	Use Built-in strlen() ~
>
			let g:Align_xstrlen= 0

<       This is the fastest method, but it doesn't handle multibyte characters
	well.  It is the default for:

	  enc=latin1
	  vim compiled without multi-byte support
	  $LANG is en_US.UTF-8 (assuming USA english)

	Number of codepoints (Latin a + combining circumflex is two codepoints)~
>
			let g:Align_xstrlen= 1              (default)
<
	Number of spacing codepoints (Latin a + combining circumflex is one~
	spacing codepoint; a hard tab is one; wide and narrow CJK are one~
	each; etc.)~
>
			let g:Align_xstrlen= 2
<
	Virtual length (counting, for instance, tabs as anything between 1 and~
	'tabstop', wide CJK as 2 rather than 1, Arabic alif as zero when~
	immediately preceded by lam, one otherwise, etc.)~
>
			let g:Align_xstrlen= 3
<
	By putting one of these settings into your <.vimrc>, Align will use an
	internal (interpreted) function to determine a string's length instead
	of the Vim's built-in |strlen()| function.  Since the function is
	interpreted, Align will run a bit slower but will handle such strings
	correctly.  The last setting (g:Align_xstrlen= 3) probably will run
	the slowest but be the most accurate.  (thanks to Tony Mechelynck for
	these)


ALIGNMENT CONTROL				*alignctrl* *align-control* {{{2

	This command doesn't do the alignment operation itself; instead, it
	controls subsequent alignment operation(s).

	The first argument to AlignCtrl is a string which may contain one or
	more alignment control settings.  Most of the settings are specified
	by single letters; the exceptions are the p# and P# commands which
	interpret a digit following the p or P as specifying padding about the
	separator.

	The typical text line is considered to be composed of two or more
	fields separated by one or more separator pattern(s): >

		ws field ws separator ws field ws separator ...
<
	where "ws" stands for "white space" such as blanks and/or tabs.


	SEPARATORS				*alignctrl-separators* {{{3

	As a result, separators may not have white space (tabs or blanks) on
	their outsides (ie.  ":  :" is fine as a separator, but " :: " is
	not).  Usually such separators are not needed, although a map has been
	provided which works around this limitation and aligns on whitespace
	(see |alignmap-tsp|).

	However, if you really need to have separators with leading or
	trailing whitespace, consider handling them by performing a substitute
	first (ie. s/  ::  /@/g), do the alignment on the temporary pattern
	(ie. @), and then perform a substitute to revert the separators back
	to their desired condition (ie. s/@/  ::  /g).

	The Align#Align() function will first convert tabs over the region into
	spaces and then apply alignment control.  Except for initial white
	space, white space surrounding the fields is ignored.  One has three
	options just for handling initial white space:


	--- 						*alignctrl-w*
	wWI 	INITIAL WHITE SPACE			*alignctrl-W* {{{3
	--- 						*alignctrl-I*
		w : ignore all selected lines' initial white space
		W : retain all selected lines' initial white space
		I : retain only the first line's initial white space and
		    re-use it for subsequent lines

	Example: Leading white space options: >
                         +---------------+-------------------+-----------------+
	                 |AlignCtrl w= :=|  AlignCtrl W= :=  | AlignCtrl I= := |
      +------------------+---------------+-------------------+-----------------+
      |     Original     |   w option    |     W option      |     I option    |
      +------------------+---------------+-------------------+-----------------+
      |   a := baaa      |a     := baaa  |   a      : = baaa |   a     := baaa |
      | caaaa := deeee   |caaaa := deeee | caaaa    : = deeee|   caaaa := deeee|
      |       ee := f    |ee    := f     |       ee : = f    |   ee    := f    |
      +------------------+---------------+-------------------+-----------------+
<
	The original has at least one leading white space on every line.
	Using Align with w eliminated each line's leading white space.
	Using Align with W preserved  each line's leading white space.
	Using Align with I applied the first line's leading white space
	                   (three spaces) to each line.


	------						*alignctrl-l*
	lrc-+:	FIELD JUSTIFICATION			*alignctrl-r* {{{3
	------						*alignctrl-c*

	With "lrc", the fields will be left-justified, right-justified, or
	centered as indicated by the justification specifiers (lrc).  The
	"lrc" options are re-used by cycling through them as needed:

		l   means llllll....
		r   means rrrrrr....
		lr  means lrlrlr....
		llr means llrllr....

     Example: Justification options: Align = >
     +------------+-------------------+-------------------+-------------------+
     |  Original  |  AlignCtrl l      | AlignCtrl r       | AlignCtrl lr      |
     +------------+-------------------+-------------------+-------------------+
     | a=bb=ccc=1 |a   = bb  = ccc = 1|  a =  bb = ccc = 1|a   =  bb = ccc = 1|
     | ccc=a=bb=2 |ccc = a   = bb  = 2|ccc =   a =  bb = 2|ccc =   a = bb  = 2|
     | dd=eee=f=3 |dd  = eee = f   = 3| dd = eee =   f = 3|dd  = eee = f   = 3|
     +------------+-------------------+-------------------+-------------------+
     | Alignment  |l     l     l     l|  r     r     r   r|l       r   l     r|
     +------------+-------------------+-------------------+-------------------+
<
		AlignCtrl l : The = separator is repeatedly re-used, as the
			      cycle only consists of one character (the "l").
			      Every time left-justification is used for fields.
		AlignCtrl r : The = separator is repeatedly re-used, as the
			      cycle only consists of one character (the "l").
			      Every time right-justification is used for fields
		AlignCtrl lr: Again, the "=" separator is repeatedly re-used,
			      but the fields are justified alternately between
			      left and right.

	Even more separator control is available.  With "-+:":

	    - : skip treating the separator as a separator.   *alignctrl--*
	    + : repeat use of the last "lrc" justification    *alignctrl-+*
	    : : treat the rest of the line as a single field  *alignctrl-:*

     Example: More justification options:  Align = >
     +------------+---------------+--------------------+---------------+
     |  Original  |  AlignCtrl -l | AlignCtrl rl+      | AlignCtrl l:  |
     +------------+---------------+--------------------+---------------+
     | a=bb=ccc=1 |a=bb   = ccc=1 |  a = bb  = ccc = 1 |a   = bb=ccc=1 |
     | ccc=a=bb=2 |ccc=a  = bb=2  |ccc = a   = bb  = 2 |ccc = a=bb=2   |
     | dd=eee=f=3 |dd=eee = f=3   | dd = eee = f   = 3 |dd  = eee=f=3  |
     +------------+---------------+--------------------+---------------+
     | Alignment  |l        l     |  r   l     l     l |l     l        |
     +------------+---------------+--------------------+---------------+
<
	In the first example in "More justification options":

	  The first "=" separator is skipped by the "-" specification,
	  and so "a=bb", "ccc=a", and "dd=eee" are considered as single fields.

	  The next "=" separator has its (left side) field left-justified.
	  Due to the cyclic nature of separator patterns, the "-l"
	  specification is equivalent to "-l-l-l ...".

	  Hence the next specification is a "skip", so "ccc=1", etc are fields.

	In the second example in "More justification options":

	  The first field is right-justified, the second field is left
	  justified, and all remaining fields repeat the last justification
	  command (ie. they are left justified, too).

	  Hence rl+ is equivalent to         rlllllllll ...
	  (whereas plain rl is equivalent to rlrlrlrlrl ... ).

	In the third example in "More justification options":

	  The text following the first separator is treated as a single field.

	Thus using the - and : operators one can apply justification to a
	single separator.

	ex. 1st separator only:    AlignCtrl l:
	    2nd separator only:    AlignCtrl -l:
	    3rd separator only:    AlignCtrl --l:
	    etc.


	---						     *alignctrl-=*
	=C	CYCLIC VS ALL-ACTIVE SEPARATORS		     *alignctrl-C* {{{3
	---

	The separators themselves may be considered as equivalent and
	simultaneously active ("=") or sequentially cycled through ("C").
	Separators are regular expressions (|regexp|) and are specified as the
	second, third, etc arguments.  When the separator patterns are
	equivalent and simultaneously active, there will be one pattern
	constructed: >

		AlignCtrl ... pat1 pat2 pat3
		\(pat1\|pat2\|pat3\)
<
	Each separator pattern is thus equivalent and simultaneously active.
	The cyclic separator AlignCtrl option stores a list of patterns, only
	one of which is active for each field at a time.

	Example: Equivalent/Simultaneously-Active vs Cyclic Separators >
 +-------------+------------------+---------------------+----------------------+
 |   Original  | AlignCtrl = = + -| AlignCtrl = =       | AlignCtrl C = + -    |
 +-------------+------------------+---------------------+----------------------+
 |a = b + c - d|a = b + c - d     |a = b + c - d        |a = b         + c - d |
 |x = y = z + 2|x = y = z + 2     |x = y         = z + 2|x = y = z     + 2     |
 |w = s - t = 0|w = s - t = 0     |w = s - t     = 0    |w = s - t = 0         |
 +-------------+------------------+---------------------+----------------------+
<
	The original is initially aligned with all operators (=+-) being
	considered as equivalent and simultaneously active field separators.
	Thus the "AlignCtrl = = + -" example shows no change.

	The second example only accepts the '=' as a field separator;
	consequently "b + c - d" is now a single field.

	The third example illustrates cyclic field separators and is analyzed
	in the following illustration: >

	field1 separator field2    separator field3 separator field4
	   a      =      b             +       c        -       d
	   x      =      y = z         +       2
	   w      =      s - t = 0
<
	The word "cyclic" is used because the patterns form a cycle of use; in
	the above case, its = + - = + - = + - = + -...

	Example: Cyclic separators >
		Label : this is some text discussing ":"s | ex. abc:def:ghi
		Label : this is some text with a ":" in it | ex. abc:def
<
	  apply AlignCtrl lWC : | |
	        (select lines)Align >
                Label : this is some text discussing ":"s  | ex. abc:def:ghi
                Label : this is some text with a ":" in it | ex. abcd:efg
<
	In the current example,
	  : is the first separator        So the first ":"s are aligned
	  | is the second separator       but subsequent ":"s are not.
	  | is the third separator        The "|"s are aligned, too.
	  : is the fourth separator       Since there aren't two bars,
	  | is the fifth separator        the subsequent potential cycles
	  | is the sixth separator        don't appear.
	 ...

	In this case it would probably have been a better idea to have used >
		AlignCtrl WCl: : |
<	as that alignment control would guarantee that no more cycling
	would be used after the vertical bar.

	Example: Cyclic separators

	    Original: >
		a| b&c | (d|e) & f-g-h
		aa| bb&cc | (dd|ee) & ff-gg-hh
		aaa| bbb&ccc | (ddd|eee) & fff-ggg-hhh
<
	    AlignCtrl C | | & - >
		a   | b&c     | (d|e)     & f   - g-h
		aa  | bb&cc   | (dd|ee)   & ff  - gg-hh
		aaa | bbb&ccc | (ddd|eee) & fff - ggg-hhh
<
	In this example,
	the first and second separators are "|",
	the third            separator  is  "&", and
	the fourth           separator  is  "-",

	(cycling)
	the fifth and sixth  separators are "|",
	the seventh          separator  is  "&", and
	the eighth           separator  is  "-", etc.

	Thus the first "&"s are (not yet) separators, and hence are treated as
	part of the field.  Ignoring white space for the moment, the AlignCtrl
	shown here means that Align will work with >

	field | field | field & field - field | field | field & field - ...
<

	---						*alignctrl-<*
	<>|	SEPARATOR JUSTIFICATION			*alignctrl->* {{{3
	---						*alignctrl-|*

	Separators may be of differing lengths as shown in the example below.
	Hence they too may be justified left, right, or centered.
	Furthermore, separator justification specifications are cyclic:

		<  means <<<<<...    justify separator(s) to the left
		>  means >>>>>...    justify separator(s) to the right
		|  means |||||...    center separator(s)

	Example: Separator Justification: Align -\+ >
				+-----------------+
				|    Original     |
				+-----------------+
				| a - bbb - c     |
				| aa -- bb -- ccc |
				| aaa --- b --- cc|
	+---------------------+-+-----------------+-+---------------------+
	|     AlignCtrl <     |     AlignCtrl >     |     AlignCtrl |     |
	+---------------------+---------------------+---------------------+
	| a   -   bbb -   c   | a     - bbb   - c   | a    -  bbb  -  c   |
	| aa  --  bb  --  ccc | aa   -- bb   -- ccc | aa  --  bb  --  ccc |
	| aaa --- b   --- cc  | aaa --- b   --- cc  | aaa --- b   --- cc  |
	+---------------------+---------------------+---------------------+
<

	---						*alignctrl-g*
	gv	SELECTIVE APPLICATION			*alignctrl-v* {{{3
	---


	These two options provide a way to select (g) or to deselect (v) lines
	based on a pattern.  Ideally :g/pat/Align  would work; unfortunately
	it results in Align#Align() being called on each line satisfying the
	pattern separately. >

		AlignCtrl g pattern
<
	Align will only consider those lines which have the given pattern. >

		AlignCtrl v pattern
<
	Align will only consider those lines without the given pattern.  As an
	example of use, consider the following example: >

				           :AlignCtrl v ^\s*/\*
	  Original          :Align =       :Align =
	+----------------+------------------+----------------+
	|one= 2;         |one     = 2;      |one   = 2;      |
	|three= 4;       |three   = 4;      |three = 4;      |
	|/* skip=this */ |/* skip = this */ |/* skip=this */ |
	|five= 6;        |five    = 6;      |five  = 6;      |
	+----------------+------------------+----------------+
<
	The first "Align =" aligned with all "="s, including that one in the
	"skip=this" comment.

	The second "Align =" had a AlignCtrl v-pattern which caused it to skip
	(ignore) the "skip=this" line when aligning.

	To remove AlignCtrl's g and v patterns, use (as appropriate) >

		AlignCtrl g
		AlignCtrl v
<
	To see what g/v patterns are currently active, just use the reporting
	capability of an unadorned call to AlignCtrl: >

		AlignCtrl
<

	---
	 m	MAP SUPPORT				*alignctrl-m* {{{3
	---

	This option primarily supports the development of maps.  The
	Align#AlignCtrl() call will first do an Align#AlignPush() (ie. retain
	current alignment control settings).  The next Align#Align() will, in
	addition to its alignment job, finish up with an Align#AlignPop().
	Thus the Align#AlignCtrl settings that follow the "m" are only
	temporarily in effect for just the next Align#Align().


	---
	p###						*alignctrl-p*
	P###	PADDING					*alignctrl-P* {{{3
	---

	These two options control pre-padding and post-padding with blanks
	about the separator.  One may pad separators with zero to nine spaces;
	the padding number(s) is/are treated as a cyclic parameter.  Thus one
	may specify padding separately for each field or re-use a padding
	pattern. >

	Example:          AlignCtrl p102P0
	+---------+----------------------------------+
	| Original| a=b=c=d=e=f=g=h=1                |
        | Align = | a =b=c  =d =e=f  =g =h=1         |
        +---------+----------------------------------+
	| prepad  |   1 0   2  1 0   2  1 0          |
        +---------+----------------------------------+
<
	This example will cause Align to:

		pre-pad the first  "=" with a single blank,
		pre-pad the second "=" with no blanks,
		pre-pad the third  "=" with two blanks,
		pre-pad the fourth "=" with a single blank,
		pre-pad the fifth  "=" with no blanks,
		pre-pad the sixth  "=" with two blanks,
	        etc.

	---------------				*alignctrl-settings*
	No option given		DISPLAY STATUS	*alignctrl-*		{{{3
	---------------				*alignctrl-no-option*

	AlignCtrl, when called with no arguments, will display the current
	alignment control settings.  A typical display is shown below: >

		AlignCtrl<=> qty=1 AlignStyle<l> Padding<1|1>
		Pat1<\(=\)>
<
	Interpreting, this means that the separator patterns are all
	equivalent; in this case, there's only one (qty=1).  Fields will be
	padded on the right with spaces (left justification), and separators
	will be padded on each side with a single space.

	To change one of these items, see:

	  AlignCtrl......|alignctrl|
	  qty............|align-concept|
	  AlignStyle.....|alignctrl--| |alignctrl-+| |alignctrl-:||alignctrl-c|
	  Padding........|alignctrl-p| |alignctrl-P|

	One may get a string which can be fed back into AlignCtrl: >

		:let alignctrl= Align#AlignCtrl()
<
	This form will put a string describing the current AlignCtrl options,
	except for the "g" and "v" patterns, into a variable.  The
	Align#AlignCtrl() function will still echo its settings, however.  One
	can feed any non-supported "option" to AlignCtrl() to prevent this,
	however: >

		:let alignctrl= Align#AlignCtrl("d")
<

ALIGNMENT						*align-align* {{{2

	Once the alignment control has been determined, the user specifies a
	range of lines for the Align command/function to do its thing.
	Alignment is often done on a line-range basis, but one may also
	restrict alignment to a visual block using ctrl-v.  For any visual
	mode, one types the colon (:) and then "Align".  One may, of course,
	specify a range of lines: >

		:[range]Align [list-of-separators]
<
	where the |:range| is the usual Vim-powered set of possibilities; the
	list of separators is the same as the AlignCtrl capability.  There is
	only one list of separators, but either AlignCtrl or Align can be used
	to specify that list.

	An alternative form of the Align command can handle both alignment
	control and the separator list: >

		:[range]Align! [alignment-control-string] [list-of-separators]
<
	The alignment control string will be applied only for this particular
	application of Align (it uses |alignctrl-m|).  The "g pattern" and
	"v pattern" alignment controls (see |alignctrl-g| and |alignctrl-v|)
	are also available via this form of the Align command.

	Align makes two passes over the text to be aligned.  The first pass
	determines how many fields there are and determines the maximum sizes
	of each field; these sizes are then stored in a vector.  The second
	pass pads the field (left/right/centered as specified) to bring its
	length up to the maximum size of the field.  Then the separator and
	its AlignCtrl-specified padding is appended.

		Pseudo-Code:~
		 During pass 1
		 | For all fields in the current line
		 || Determine current separator
		 || Examine field specified by current separator
		 || Determine length of field and save if largest thus far
		 Initialize newline based on initial whitespace option (wWI)
		 During pass 2
		 | For all fields in current line
		 || Determine current separator
		 || Extract field specified by current separator
		 || Prepend/append padding as specified by AlignCtrl
		 || (right/left/center)-justify to fit field into max-size field
		 || Append separator with AlignCtrl-specified separator padding
		 || Delete current line, install newly aligned line

	The g and v AlignCtrl patterns cause the passes not to consider lines
	for alignment, either by requiring that the g-pattern be present or
	that the v-pattern not be present.

	The whitespace on either side of a separator is ignored.


==============================================================================
4. Alignment Maps				*alignmaps* *align-maps* {{{1

	There are a number of maps using Align#AlignCtrl() and Align#Align()
	in the <AlignMapsPlugin.vim> file.  This file may also be put into the
	plugins subdirectory.  Since AlignCtrl and Align supercede textab and
	its <ttalign.vim> file, the maps either have a leading "t" (for
	"textab") or the more complicated ones an "a" (for "alignment") for
	backwards compatibility.

	The maps are shown below with a leading backslash (\).  Actually, the
	<Leader> construct is used (see |mapleader|), so the maps' leading
	kick-off character is easily customized.

	Furthermore, all AlignMapsPlugin.vim maps use the <Plug> construct (see
	|<Plug>|and |usr_41.txt|).  Hence, if one wishes to override the
	mapping entirely, one may do that, too.  As an example: >
		map <Leader>ACOM	<Plug>AM_acom
<	would have \ACOM do what \acom previously did (assuming that the
	mapleader has been left at its default value of a backslash).

	  \a,   : useful for breaking up comma-separated
	          declarations prior to \adec			|alignmap-a,|
	  \a(   : aligns ( and , (useful for prototypes)        *alignmap-a(*
	  \a?   : aligns (...)? ...:... expressions on ? and :	|alignmap-a?|
	  \a<   : aligns << and >> for c++			|alignmap-a<|
	  \a=   : aligns := assignments   			|alignmap-a=|
	  \abox : draw a C-style comment box around text lines	|alignmap-abox|
	  \acom : useful for aligning comments			|alignmap-acom|
	  \adcom: useful for aligning comments in declarations  |alignmap-adcom|
	  \anum : useful for aligning numbers 			|alignmap-anum|
	          NOTE: For the visual-mode use of \anum, <vis.vim> is needed!
		    See http://mysite.verizon.net/astronaut/vim/index.html#VIS
	  \aenum: align a European-style number			|alignmap-anum|
	  \aunum: align a USA-style number			|alignmap-anum|
	  \adec : useful for aligning declarations		|alignmap-adec|
	  \adef : useful for aligning definitions		|alignmap-adef|
	  \afnc : useful for aligning ansi-c style functions'
	          argument lists				|alignmap-afnc|
	  \adcom: a variant of \acom, restricted to comment     |alignmap-adcom|
	          containing lines only, but also only for
		  those which don't begin with a comment.
		  Good for certain declaration styles.
	  \aocom: a variant of \acom, restricted to comment     |alignmap-aocom|
	          containing lines only
	  \tab  : align a table based on tabs			*alignmap-tab*
	          (converts to spaces)
	  \tml  : useful for aligning the trailing backslashes	|alignmap-tml|
	          used to continue lines (shell programming, etc)
	  \tsp  : use Align to make a table separated by blanks	|alignmap-tsp|
	          (left justified)
	  \ts,  : like \t, but swaps whitespace on the right of *alignmap-ts,*
	          the commas to their left
	  \ts:  : like \t: but swaps whitespace on the right of *alignmap-ts:*
	          the colons to their left
	  \ts<  : like \t< but swaps whitespace on the right of *alignmap-ts<*
	          the less-than signs to their left
	  \ts=  : like \t= but swaps whitespace on the right of *alignmap-ts=*
	          the equals signs to their left
	  \Tsp  : use Align to make a table separated by blanks	|alignmap-Tsp|
	          (right justified)
	  \tsq  : use Align to make a table separated by blanks	|alignmap-tsq|
	          (left justified) -- "strings" are not split up
	  \tt   : useful for aligning LaTeX tabular tables	|alignmap-tt|
	  \Htd  : tabularizes html tables:			|alignmap-Htd|
	          <TR><TD> ...field... </TD><TD> ...field... </TD></TR>

		  *alignmap-t|* *alignmap-t#* *alignmap-t,* *alignmap-t:*
		  *alignmap-t;* *alignmap-t<* *alignmap-t?* *alignmap-t~*
		  *alignmap-m=*
	  \tx   : make a left-justified  alignment on
	          character "x" where "x" is: ,:<=@|#		|alignmap-t=|
	  \Tx   : make a right-justified alignment on
	          character "x" where "x" is: ,:<=@#		|alignmap-T=|
	  \m=   : like \t= but aligns with %... style comments

	The leading backslash is actually <leader> (see |mapleader| for how to
	customize the leader to be whatever you like).  These maps use the
	<Align.vim> package and are defined in the <AlignMaps.vim> file.
	Although the maps use AlignCtrl options, they typically use the "m"
	option which pushes the options (AlignPush).  The associated Align
	call which follows will then AlignPop the user's original options
	back.

	ALIGNMENT MAP USE WITH MARK AND MOVE~
	In the examples below, one may select the text with a "ma" at the
	first line, move to the last line, then execute the map.

	ALIGNMENT MAP USE WITH VISUAL MODE~
	Alternatively, one may select the text with the "V" visual mode
	command.

	ALIGNMENT MAP USE WITH MENUS~
	One may use the mark-and-move style (ma, move, use the menu) or
	the visual mode style (use the V visual mode, move, then select
	the alignment map with menu selection).  The alignment map menu
	items are under DrChip.AlignMaps .

	One may even change the top level menu name to whatever is wished; by
	default, its >
		let g:DrChipTopLvlMenu= "DrChip."
<	If you set the variable to the empty string (""), then no menu items
	will be produced.  Of course, one must have a vim with +menu, the gui
	must be running, and |'go'| must have the menu bar suboption (ie. m
	must be included).

	COMPLEX ALIGNMENT MAP METHOD~

	For those complex alignment maps which do alignment on constructs
	(e.g. \acom, \adec, etc), a series of substitutes is used to insert
	"@" symbols in appropriate locations.  Align#Align() is then used to
	do alignment directly on "@"s; then it is followed by further
	substitutes to do clean-up.  However, the maps \WS and \WE, used by
	every map supported by AlignMaps, protect any original embedded "@"
	symbols by first converting them to <DEL> characters, doing the
	requested job, and then converting them back. >

	    \WS  calls AlignMaps#WrapperStart()
	    \WE  calls AlignMaps#WrapperEnd()
<

	---------------------------
	Alignment Map Examples: \a,			*alignmap-a,* {{{3
	---------------------------

	Original: illustrates comma-separated declaration splitting: >
		int a,b,c;
		struct ABC_str abc,def;
<
	Becomes: >
		int a;
		int b;
		int c;
		struct ABC_str abc;
		struct ABC_str def;
<

	---------------------------
	Alignment Map Examples: \a?			*alignmap-a?* {{{3
	---------------------------

	Original: illustrates ()?: aligning >
		printf("<%s>\n",
		  (x == ABC)? "abc" :
		  (x == DEFG)? "defg" :
		  (x == HIJKL)? "hijkl" : "???");
<
	Becomes:  select "(x == ..." lines, then \a? >
		printf("<%s>\n",
		  (x == ABC)?   "abc"   :
		  (x == DEFG)?  "defg"  :
		  (x == HIJKL)? "hijkl" : "???");
<

	---------------------------
	Alignment Map Examples: \a<			*alignmap-a<* {{{3
	---------------------------

	Original: illustrating aligning of << and >> >
		cin << x;
		cin      << y;
		cout << "this is x=" << x;
		cout << "but y=" << y << "is not";
<
	Becomes:  select "(x == ..." lines, then \a< >
		cin  << x;
		cin  << y;
		cout << "this is x=" << x;
		cout << "but y="     << y  << "is not";
<

	---------------------------
	Alignment Map Examples: \a=			*alignmap-a=* {{{3
	---------------------------

	Original: illustrates how to align := assignments >
		aa:=bb:=cc:=1;
		a:=b:=c:=1;
		aaa:=bbb:=ccc:=1;
<
	Bcomes: select the three assignment lines, then \a:= >
		aa  := bb  := cc  := 1;
		a   := b   := c   := 1;
		aaa := bbb := ccc := 1;
<

	---------------------------
	Alignment Map Examples: \abox			*alignmap-abox* {{{3
	---------------------------

	Original: illustrates how to comment-box some text >
		This is some plain text
		which will
		soon be surrounded by a
		comment box.
<
	Becomes:  Select "This..box." with ctrl-v, press \abox >
		/***************************
		 * This is some plain text *
		 * which will              *
		 * soon be surrounded by a *
		 * comment box.            *
		 ***************************/
<

	---------------------------
	Alignment Map Examples: \acom			*alignmap-acom* {{{3
	---------------------------

	Original: illustrates aligning C-style comments (works for //, too) >
		if(itworks) { /* this */
			then= dothis; /* is a */
			} /* set of three comments */
<
	Becomes: Select the three lines, press \acom >
	        if(itworks) {         /* this                  */
	                then= dothis; /* is a                  */
	                }             /* set of three comments */
<
	Also see |alignmap-aocom|


	---------------------------
	Alignment Map Examples: \anum			*alignmap-anum* {{{3
	---------------------------

	Original: illustrates how to get numbers lined up >
		 -1.234 .5678 -.901e-4
		 1.234 5.678 9.01e-4
		 12.34 56.78 90.1e-4
		 123.4 567.8 901.e-4
<
	Becomes: Go to first line, ma.  Go to last line, press \anum >
		  -1.234    .5678   -.901e-4
		   1.234   5.678    9.01e-4
		  12.34   56.78    90.1e-4
		 123.4   567.8    901.e-4
<
	Original: >
		 | -1.234 .5678 -.901e-4 |
		 | 1.234 5.678 9.01e-4   |
		 | 12.34 56.78 90.1e-4   |
		 | 123.4 567.8 901.e-4   |
<
	Becomes: Select the numbers with ctrl-v (visual-block mode), >
	         press \anum
	         |  -1.234    .5678   -.901e-4 |
	         |   1.234   5.678    9.01e-4  |
	         |  12.34   56.78    90.1e-4   |
	         | 123.4   567.8    901.e-4    |
<
	Original: >
		 -1,234 ,5678 -,901e-4
		 1,234 5,678 9,01e-4
		 12,34 56,78 90,1e-4
		 123,4 567,8 901,e-4
<
	Becomes: Go to first line, ma.  Go to last line, press \anum >
		  -1,234    ,5678   -,901e-4
		   1,234   5,678    9,01e-4
		  12,34   56,78    90,1e-4
		 123,4   567,8    901,e-4
<
	In addition:
	  \aenum is provided to support European-style numbers
	  \aunum is provided to support USA-style numbers

	One may get \aenum behavior for \anum >
	  let g:alignmaps_euronumber= 1
<	or \aunum behavior for \anum if one puts >
	  let g:alignmaps_usanumber= 1
<	in one's <.vimrc>.


	---------------------------
	Alignment Map Examples: \ascom			*alignmap-ascom* {{{3
	---------------------------

	Original: >
		/* A Title */
		int x; /* this is a comment */
		int yzw; /* this is another comment*/
<
	Becomes: Select the three lines, press \ascom >
	        /* A Title */
	        int x;   /* this is a comment       */
	        int yzw; /* this is another comment */
<

	---------------------------
	Alignment Map Examples: \adec			*alignmap-adec* {{{3
	---------------------------

	Original: illustrates how to clean up C/C++ declarations >
		int     a;
		float   b;
		double *c=NULL;
		char x[5];
		struct  abc_str abc;
		struct  abc_str *pabc;
		int     a;              /* a   */
		float   b;              /* b   */
		double *c=NULL;              /* b   */
		char x[5]; /* x[5] */
		struct  abc_str abc;    /* abc */
		struct  abc_str *pabc;    /* pabc */
		static   int     a;              /* a   */
		static   float   b;              /* b   */
		static   double *c=NULL;              /* b   */
		static   char x[5]; /* x[5] */
		static   struct  abc_str abc;    /* abc */
		static   struct  abc_str *pabc;    /* pabc */
<
	Becomes: Select the declarations text, then \adec >
		int                    a;
		float                  b;
		double                *c    = NULL;
		char                   x[5];
		struct abc_str         abc;
		struct abc_str        *pabc;
		int                    a;           /* a    */
		float                  b;           /* b    */
		double                *c    = NULL; /* b    */
		char                   x[5];        /* x[5] */
		struct abc_str         abc;         /* abc  */
		struct abc_str        *pabc;        /* pabc */
		static int             a;           /* a    */
		static float           b;           /* b    */
		static double         *c    = NULL; /* b    */
		static char            x[5];        /* x[5] */
		static struct abc_str  abc;         /* abc  */
		static struct abc_str *pabc;        /* pabc */
<

	---------------------------
	Alignment Map Examples: \adef			*alignmap-adef* {{{3
	---------------------------

	Original: illustrates how to line up #def'initions >
		#define ONE 1
		#define TWO 22
		#define THREE 333
		#define FOUR 4444
<
	Becomes: Select four definition lines, apply \adef >
	#	 define ONE   1
	#	 define TWO   22
	#	 define THREE 333
	#	 define FOUR  4444
<

	---------------------------
	Alignment Map Examples: \afnc			*alignmap-afnc* {{{3
	---------------------------

	This map is an exception to the usual selection rules.
	It uses "]]" to find the function body's leading "{".
	Just put the cursor anywhere in the function arguments and
	the entire function declaration should be processed.

	Because "]]" looks for that "{" in the first column, the
	"original" and "becomes" examples are in the first column,
	too.

	Original: illustrates lining up ansi-c style function definitions >
	int f(
	  struct abc_str ***a, /* one */
	  long *b, /* two */
	  int c) /* three */
	{
	}
<
	Becomes: put cursor anywhere before the '{', press \afnc >
	int f(
	  struct abc_str ***a,	/* one   */
	  long             *b,	/* two   */
	  int               c)	/* three */
	{
	}
<

	---------------------------
	Alignment Map Examples: \adcom			*alignmap-adcom* {{{3
	---------------------------

	Original: illustrates aligning comments that don't begin
		lines (optionally after some whitespace). >
		struct {
			/* this is a test */
			int x; /* of how */
			double y; /* to use adcom */
			};
<
	Becomes: Select the inside lines of the structure,
		then press \adcom.  The comment-only
		line is ignored but the other two comments
		get aligned. >
		struct {
                        /* this is a test */
                        int x;    /* of how       */
                        double y; /* to use adcom */
			};
<

	---------------------------
	Alignment Map Examples: \aocom			*alignmap-aocom* {{{3
	---------------------------

	Original: illustrates how to align C-style comments (works for //, too)
	          but restricted only to aligning with those lines containing
		  comments.  See the difference from \acom (|alignmap-acom|). >
		if(itworks) { /* this comment */
			then= dothis;
			} /* only appears on two lines */
<
	Becomes: Select the three lines, press \aocom >
                if(itworks) { /* this comment              */
                        then= dothis;
                        }     /* only appears on two lines */
<
	Also see |alignmap-acom|


	---------------------------			*alignmap-Tsp*
	Alignment Map Examples: \tsp			*alignmap-tsp* {{{3
	---------------------------

	Normally Align can't use white spaces for field separators as such
	characters are ignored surrounding field separators.  The \tsp and
	\Tsp maps get around this limitation.

	Original: >
	 one two three four five
	 six seven eight nine ten
	 eleven twelve thirteen fourteen fifteen
<
	Becomes: Select the lines, \tsp >
	 one    two    three    four     five
	 six    seven  eight    nine     ten
	 eleven twelve thirteen fourteen fifteen
<
	Becomes: Select the lines, \Tsp >
	    one    two    three     four    five
	    six  seven    eight     nine     ten
	 eleven twelve thirteen fourteen fifteen
<

	---------------------------
	Alignment Map Examples: \tsq			*alignmap-tsq* {{{3
	---------------------------

	The \tsp map is useful for aligning tables based on white space,
	but sometimes one wants double-quoted strings to act as a single
	object in spite of embedded spaces.  The \tsq map was invented
	to support this. (thanks to Leif Wickland)

	Original: >
	 "one two" three
	 four "five six"
<
	Becomes: Select the lines, \tsq >
	 "one two" three
	 four      "five six"
<

	---------------------------
	Alignment Map Examples: \tt			*alignmap-tt* {{{3
	---------------------------

	Original: illustrates aligning a LaTex Table >
	 \begin{tabular}{||c|l|r||}
	 \hline\hline
	   one&two&three\\ \hline
	   four&five&six\\
	   seven&eight&nine\\
	 \hline\hline
	 \end{tabular}
<
	Becomes: Select the three lines inside the table >
	(ie. one..,four..,seven..) and press \tt
	 \begin{tabular}{||c|l|r||}
	 \hline\hline
	   one   & two   & three \\ \hline
	   four  & five  & six   \\
	   seven & eight & nine  \\
	 \hline\hline
	 \end{tabular}
<

	----------------------------
	Alignment Map Examples: \tml			*alignmap-tml* {{{3
	----------------------------

        Original:  illustrates aligning multi-line continuation marks >
	one \
	two three \
	four five six \
	seven \\ \
	eight \nine \
	ten \
<
        Becomes:  >
        one           \
        two three     \
        four five six \
        seven \\      \
        eight \nine   \
        ten           \
<

	---------------------------
	Alignment Map Examples: \t=			*alignmap-t=* {{{3
	---------------------------

	Original: illustrates left-justified aligning of = >
		aa=bb=cc=1;/*one*/
		a=b=c=1;/*two*/
		aaa=bbb=ccc=1;/*three*/
<
	Becomes: Select the three equations, press \t= >
		aa  = bb  = cc  = 1; /* one   */
		a   = b   = c   = 1; /* two   */
		aaa = bbb = ccc = 1; /* three */
<

	---------------------------
	Alignment Map Examples: \T=			*alignmap-T=* {{{3
	---------------------------

	Original: illustrates right-justified aligning of = >
		aa=bb=cc=1; /* one */
		a=b=c=1; /* two */
		aaa=bbb=ccc=1; /* three */
<
	Becomes: Select the three equations, press \T= >
                 aa =  bb =  cc = 1; /* one   */
                  a =   b =   c = 1; /* two   */
                aaa = bbb = ccc = 1; /* three */
<

	---------------------------
	Alignment Map Examples: \Htd			*alignmap-Htd* {{{3
	---------------------------

	Original: for aligning tables with html >
	  <TR><TD>...field one...</TD><TD>...field two...</TD></TR>
	  <TR><TD>...field three...</TD><TD>...field four...</TD></TR>
<
	Becomes: Select <TR>... lines, press \Htd >
	  <TR><TD> ...field one...   </TD><TD> ...field two...  </TD></TR>
	  <TR><TD> ...field three... </TD><TD> ...field four... </TD></TR>
<
==============================================================================
4. Alignment Tools' History				*align-history* {{{1

ALIGN HISTORY								{{{2
	35 : Nov 02, 2008 * g:loaded_AlignPlugin testing to prevent re-loading
			    installed
	     Nov 19, 2008 * new sanity check for an AlignStyle of just ":"
	     Jan 08, 2009 * save&restore of |'mod'| now done with local
			    variant
	34 : Jul 08, 2008 * using :AlignCtrl before entering any alignment
			    control commands was causing an error.
	33 : Sep 20, 2007 * s:Strlen() introduced to support various ways
			    used to represent characters and their effects
			    on string lengths.  See |align-strlen|.
			  * Align now accepts "..." -- so it can accept
			    whitespace as separators.
	32 : Aug 18, 2007 * uses |<q-args>| instead of |<f-args>| plus a
	                    custom argument splitter to allow patterns with
			    backslashes to slide in unaltered.
	31 : Aug 06, 2007 * :[range]Align! [AlignCtrl settings] pattern(s)
	                    implemented.
	30 : Feb 12, 2007 * now uses |setline()|
	29 : Jan 18, 2006 * cecutil updated to use keepjumps
	     Feb 23, 2006 * Align now converted to vim 7.0 style using
	                    auto-loading functions.
	28 : Aug 17, 2005 * report option workaround
	     Oct 24, 2005 * AlignCtrl l:  wasn't behaving as expected; fixed
	27 : Apr 15, 2005 : cpo workaround
	                    ignorecase workaround
	26 : Aug 20, 2004 : loaded_align now also indicates version number
	                    GetLatestVimScripts :AutoInstall: now supported
	25 : Jul 27, 2004 : For debugging, uses Dfunc(), Dret(), and Decho()
	24 : Mar 03, 2004 : (should've done this earlier!) visualmode(1)
	                    not supported until v6.2, now Align will avoid
			    calling it for earlier versions.  Visualmode
			    clearing won't take place then, of course.
	23 : Oct 07, 2003 : Included Leif Wickland's ReplaceQuotedSpaces()
	                    function which supports \tsq
	22 : Jan 29, 2003 : Now requires 6.1.308 or later to clear visualmode()
	21 : Jan 10, 2003 : BugFix: similar problem to #19; new code
	                    bypasses "norm! v\<Esc>" until initialization
	                    is over.
	20 : Dec 30, 2002 : BugFix: more on "unable to highlight" fixed
	19 : Nov 21, 2002 : BugFix: some terminals gave an "unable to highlight"
	                    message at startup; Hari Krishna Dara tracked it
	                    down; a silent! now included to prevent noise.
	18 : Nov 04, 2002 : BugFix: re-enabled anti-repeated-loading
	17 : Nov 04, 2002 : BugFix: forgot to have AlignPush() push s:AlignSep
	                    AlignCtrl now clears visual-block mode when used so
	                    that Align won't try to use old visual-block
	                    selection marks '< '>
	16 : Sep 18, 2002 : AlignCtrl <>| options implemented (separator
	                    justification)
	15 : Aug 22, 2002 : bug fix: AlignCtrl's ":" now acts as a modifier of
	                             the preceding alignment operator (lrc)
	14 : Aug 20, 2002 : bug fix: AlignCtrl default now keeps &ic unchanged
	                    bug fix: Align, on end-field, wasn't using correct
	                    alignop bug fix: Align, on end-field, was appending
			    padding
	13 : Aug 19, 2002 : bug fix: zero-length g/v patterns are accepted
	                    bug fix: always skip blank lines
	                    bug fix: AlignCtrl default now also clears g and v
	                             patterns
	12 : Aug 16, 2002 : moved keep_ic above zero-length pattern checks
	                    added "AlignCtrl default"
	                    fixed bug with last field getting separator spaces
	                    at end line
	11 : Jul 08, 2002 : prevent separator patterns which match zero length
	                    -+: included as additional alignment/justification
	                    styles
	10 : Jun 26, 2002 : =~# used instead of =~ (for matching case)
	                    ignorecase option handled
	 9 : Jun 25, 2002 : implemented cyclic padding

ALIGNMENT MAP HISTORY					*alignmap-history* {{{2
       v41    Nov 02, 2008   * g:loaded_AlignMapsPlugin testing to prevent
			       re-loading installed
			     * AlignMaps now use 0x0f (ctrl-p) for special
			       character substitutions (instead of 0xff).
			       Seems to avoid some problems with having to
			       use Strlen().
			     * bug fixed with \ts,
			     * new maps: \ts; \ts, \ts: \ts< \ts= \a(
       v40    Oct 21, 2008   * Modified AlignMaps so that its maps use <Plug>s
			       and <script>s.  \t@ and related maps have been
			       changed to call StdAlign() instead.  The
			       WrapperStart function now takes an argument and
			       handles being called via visual mode.  The
			       former nmaps and vmaps have thus been replaced
			       with a simple map.
	      Oct 24, 2008   * broke AlignMaps into a plugin and autoload
			       pair of scripts.
	v39   Mar 06, 2008 : * \t= only does /* ... */ aligning when in *.c
	                       *.cpp files.
	v38   Aug 18, 2007 : * \tt altered so that it works with the new
	                       use of |<q-args>| plus a custom argument
			       splitter
	v36   Sep 27, 2006 : * AlignWrapperStart() now has tests that marks
	                       y and z are not set
	      May 15, 2007   * \anum and variants improved
	v35   Sep 01, 2006 : * \t= and cousins used "`"s.  They now use \xff
	                       characters.
	                     * \acom now works with doxygen style /// comments
	                     * <char-0xff> used in \t= \T= \w= and \m= instead
	                       of backquotes.
	v34   Feb 23, 2006 : * AlignMaps now converted to vim 7.0 style using
	                       auto-loading functions.
	v33   Oct 12, 2005 : * \ts, now uses P1 in its AlignCtrl call
	v32   Jun 28, 2005 : * s:WrapperStart() changed to AlignWrapperStart()
	                       s:WrapperEnd() changed to AlignWrapperEnd()
	                       These changes let the AlignWrapper...()s to be
	                       used outside of AlignMaps.vim
	v31   Feb 01, 2005 : * \adcom included, with help
	                     * \a, now works across multiple lines with
	                       different types
	                     * AlignMaps now uses <cecutil.vim> for its mark and
	                       window-position saving and restoration
	      Mar 04, 2005   * improved \a,
	      Apr 06, 2005   * included \aenum, \aunum, and provided
	              g:alignmaps_{usa|euro]number} options
	v30   Aug 20, 2004 : * \a, : handles embedded assignments and does \adec
	                     * \acom  now can handle Doxygen-style comments
	                     * g:loaded_alignmaps now also indicates version
	                     * internal maps \WE and \WS are now re-entrant
	v29   Jul 27, 2004 : * \tml aligns trailing multi-line single
	                      backslashes (thanks to Raul Benavente!)
	v28   May 13, 2004 : * \a, had problems with leading blanks; fixed!
	v27   Mar 31, 2004 : * \T= was having problems with == and !=
	                     * Fixed more problems with \adec
	v26   Dec 09, 2003 : * \ascom now also ignores lines without comments
	                     * \tt  \& now not matched
	                     * \a< handles both << and >>
	v25   Nov 14, 2003 : * included \anum (aligns numbers with periods and
	                       commas).  \anum also supported with ctrl-v mode.
	                     * \ts, \Ts, : (aligns on commas, then swaps leading
	                       spaces with commas)
	                     * \adec ignores preprocessor lines and lines with
	                       with comments-only
	v23   Sep 10, 2003 : * Bugfix for \afnc - no longer overwrites marks y,z
	                     * fixed bug in \tsp, \tab, \Tsp, and \Tab - lines
	                       containing backslashes were having their
	                       backslashes removed.  Included Leif Wickland's
	                       patch for \tsq.
	                     * \adef now ignores lines holding comments only
	v18   Aug 22, 2003 :   \a< lines up C++'s << operators
	                       saves/restores gdefault option (sets to nogd)
	                       all b:..varname.. are now b:alignmaps_..varname..
	v17   Nov 04, 2002 :   \afnc now handles // comments correctly and
	                       commas within comments
	v16   Sep 10, 2002 :   changed : to :silent! for \adec
	v15   Aug 27, 2002 :   removed some <c-v>s
	v14   Aug 20, 2002 :   \WS, \WE mostly moved to functions, marks y and z
	                       now restored
	v11   Jul 08, 2002 :   \abox bug fix
	 v9   Jun 25, 2002 :   \abox now handles leading initial whitespace
	                   :   various bugfixes to \afnc, \T=, etc

==============================================================================
Modelines: {{{1
vim:tw=78:ts=8:ft=help:fdm=marker:
autoload/Align.vim	[[[1
1029
" Align: tool to align multiple fields based on one or more separators
"   Author:		Charles E. Campbell, Jr.
"   Date:		Mar 03, 2009
"   Version:	35
" GetLatestVimScripts: 294 1 :AutoInstall: Align.vim
" GetLatestVimScripts: 1066 1 :AutoInstall: cecutil.vim
" Copyright:    Copyright (C) 1999-2007 Charles E. Campbell, Jr. {{{1
"               Permission is hereby granted to use and distribute this code,
"               with or without modifications, provided that this copyright
"               notice is copied with it. Like anything else that's free,
"               Align.vim is provided *as is* and comes with no warranty
"               of any kind, either expressed or implied. By using this
"               plugin, you agree that in no event will the copyright
"               holder be liable for any damages resulting from the use
"               of this software.
"
" Romans 1:16,17a : For I am not ashamed of the gospel of Christ, for it is {{{1
" the power of God for salvation for everyone who believes; for the Jew first,
" and also for the Greek.  For in it is revealed God's righteousness from
" faith to faith.

" ---------------------------------------------------------------------
" Load Once: {{{1
if exists("g:loaded_Align") || &cp
 finish
endif
let g:loaded_Align = "v35"
if v:version < 700
 echohl WarningMsg
 echo "***warning*** this version of Align needs vim 7.0"
 echohl Normal
 finish
endif
let s:keepcpo= &cpo
set cpo&vim
"DechoTabOn

" ---------------------------------------------------------------------
" Debugging Support: {{{1
"if !exists("g:loaded_Decho") | runtime plugin/Decho.vim | endif

" ---------------------------------------------------------------------
" Options: {{{1
if !exists("g:Align_xstrlen")
 if &enc == "latin1" || $LANG == "en_US.UTF-8" || !has("multi_byte")
  let g:Align_xstrlen= 0
 else
  let g:Align_xstrlen= 1
 endif
endif

" ---------------------------------------------------------------------
" Align#AlignCtrl: enter alignment patterns here {{{1
"
"   Styles   =  all alignment-break patterns are equivalent
"            C  cycle through alignment-break pattern(s)
"            l  left-justified alignment
"            r  right-justified alignment
"            c  center alignment
"            -  skip separator, treat as part of field
"            :  treat rest of line as field
"            +  repeat previous [lrc] style
"            <  left justify separators
"            >  right justify separators
"            |  center separators
"
"   Builds   =  s:AlignPat  s:AlignCtrl  s:AlignPatQty
"            C  s:AlignPat  s:AlignCtrl  s:AlignPatQty
"            p  s:AlignPrePad
"            P  s:AlignPostPad
"            w  s:AlignLeadKeep
"            W  s:AlignLeadKeep
"            I  s:AlignLeadKeep
"            l  s:AlignStyle
"            r  s:AlignStyle
"            -  s:AlignStyle
"            +  s:AlignStyle
"            :  s:AlignStyle
"            c  s:AlignStyle
"            g  s:AlignGPat
"            v  s:AlignVPat
"            <  s:AlignSep
"            >  s:AlignSep
"            |  s:AlignSep
fun! Align#AlignCtrl(...)

"  call Dfunc("AlignCtrl(...) a:0=".a:0)

  " save options that will be changed
  let keep_search = @/
  let keep_ic     = &ic

  " turn ignorecase off
  set noic

  " clear visual mode so that old visual-mode selections don't
  " get applied to new invocations of Align().
  if v:version < 602
   if !exists("s:Align_gavemsg")
	let s:Align_gavemsg= 1
    echomsg "Align needs at least Vim version 6.2 to clear visual-mode selection"
   endif
  elseif exists("s:dovisclear")
"   call Decho("clearing visual mode a:0=".a:0." a:1<".a:1.">")
   let clearvmode= visualmode(1)
  endif

  " set up a list akin to an argument list
  if a:0 > 0
   let A= s:QArgSplitter(a:1)
  else
   let A=[0]
  endif

  if A[0] > 0
   let style = A[1]

   " Check for bad separator patterns (zero-length matches)
   " (but zero-length patterns for g/v is ok)
   if style !~# '[gv]'
    let ipat= 2
    while ipat <= A[0]
     if "" =~ A[ipat]
      echoerr "AlignCtrl: separator<".A[ipat]."> matches zero-length string"
	  let &ic= keep_ic
"      call Dret("AlignCtrl")
      return
     endif
     let ipat= ipat + 1
    endwhile
   endif
  endif

"  call Decho("AlignCtrl() A[0]=".A[0])
  if !exists("s:AlignStyle")
   let s:AlignStyle= "l"
  endif
  if !exists("s:AlignPrePad")
   let s:AlignPrePad= 0
  endif
  if !exists("s:AlignPostPad")
   let s:AlignPostPad= 0
  endif
  if !exists("s:AlignLeadKeep")
   let s:AlignLeadKeep= 'w'
  endif

  if A[0] == 0
   " ----------------------
   " List current selection
   " ----------------------
   if !exists("s:AlignPatQty")
	let s:AlignPatQty= 0
   endif
   echo "AlignCtrl<".s:AlignCtrl."> qty=".s:AlignPatQty." AlignStyle<".s:AlignStyle."> Padding<".s:AlignPrePad."|".s:AlignPostPad."> LeadingWS=".s:AlignLeadKeep." AlignSep=".s:AlignSep
"   call Decho("AlignCtrl<".s:AlignCtrl."> qty=".s:AlignPatQty." AlignStyle<".s:AlignStyle."> Padding<".s:AlignPrePad."|".s:AlignPostPad."> LeadingWS=".s:AlignLeadKeep." AlignSep=".s:AlignSep)
   if      exists("s:AlignGPat") && !exists("s:AlignVPat")
	echo "AlignGPat<".s:AlignGPat.">"
   elseif !exists("s:AlignGPat") &&  exists("s:AlignVPat")
	echo "AlignVPat<".s:AlignVPat.">"
   elseif exists("s:AlignGPat") &&  exists("s:AlignVPat")
	echo "AlignGPat<".s:AlignGPat."> AlignVPat<".s:AlignVPat.">"
   endif
   let ipat= 1
   while ipat <= s:AlignPatQty
	echo "Pat".ipat."<".s:AlignPat_{ipat}.">"
"	call Decho("Pat".ipat."<".s:AlignPat_{ipat}.">")
	let ipat= ipat + 1
   endwhile

  else
   " ----------------------------------
   " Process alignment control settings
   " ----------------------------------
"   call Decho("process the alignctrl settings")
"   call Decho("style<".style.">")

   if style ==? "default"
     " Default:  preserve initial leading whitespace, left-justified,
     "           alignment on '=', one space padding on both sides
	 if exists("s:AlignCtrlStackQty")
	  " clear AlignCtrl stack
      while s:AlignCtrlStackQty > 0
	   call Align#AlignPop()
	  endwhile
	  unlet s:AlignCtrlStackQty
	 endif
	 " Set AlignCtrl to its default value
     call Align#AlignCtrl("Ilp1P1=<",'=')
	 call Align#AlignCtrl("g")
	 call Align#AlignCtrl("v")
	 let s:dovisclear = 1
	 let &ic          = keep_ic
	 let @/           = keep_search
"     call Dret("AlignCtrl")
	 return
   endif

   if style =~# 'm'
	" map support: Do an AlignPush now and the next call to Align()
	"              will do an AlignPop at exit
"	call Decho("style case m: do AlignPush")
	call Align#AlignPush()
	let s:DoAlignPop= 1
   endif

   " = : record a list of alignment patterns that are equivalent
   if style =~# "="
"	call Decho("style case =: record list of equiv alignment patterns")
    let s:AlignCtrl  = '='
	if A[0] >= 2
     let s:AlignPatQty= 1
     let s:AlignPat_1 = A[2]
     let ipat         = 3
     while ipat <= A[0]
      let s:AlignPat_1 = s:AlignPat_1.'\|'.A[ipat]
      let ipat         = ipat + 1
     endwhile
     let s:AlignPat_1= '\('.s:AlignPat_1.'\)'
"     call Decho("AlignCtrl<".s:AlignCtrl."> AlignPat<".s:AlignPat_1.">")
	endif

    "c : cycle through alignment pattern(s)
   elseif style =~# 'C'
"	call Decho("style case C: cycle through alignment pattern(s)")
    let s:AlignCtrl  = 'C'
	if A[0] >= 2
     let s:AlignPatQty= A[0] - 1
     let ipat         = 1
     while ipat < A[0]
      let s:AlignPat_{ipat}= A[ipat+1]
"     call Decho("AlignCtrl<".s:AlignCtrl."> AlignQty=".s:AlignPatQty." AlignPat_".ipat."<".s:AlignPat_{ipat}.">")
      let ipat= ipat + 1
     endwhile
	endif
   endif

   if style =~# 'p'
    let s:AlignPrePad= substitute(style,'^.*p\(\d\+\).*$','\1','')
"	call Decho("style case p".s:AlignPrePad.": pre-separator padding")
    if s:AlignPrePad == ""
     echoerr "AlignCtrl: 'p' needs to be followed by a numeric argument'
     let @/ = keep_search
	 let &ic= keep_ic
"     call Dret("AlignCtrl")
     return
	endif
   endif

   if style =~# 'P'
    let s:AlignPostPad= substitute(style,'^.*P\(\d\+\).*$','\1','')
"	call Decho("style case P".s:AlignPostPad.": post-separator padding")
    if s:AlignPostPad == ""
     echoerr "AlignCtrl: 'P' needs to be followed by a numeric argument'
     let @/ = keep_search
	 let &ic= keep_ic
"     call Dret("AlignCtrl")
     return
	endif
   endif

   if     style =~# 'w'
"	call Decho("style case w: ignore leading whitespace")
	let s:AlignLeadKeep= 'w'
   elseif style =~# 'W'
"	call Decho("style case w: keep leading whitespace")
	let s:AlignLeadKeep= 'W'
   elseif style =~# 'I'
"	call Decho("style case w: retain initial leading whitespace")
	let s:AlignLeadKeep= 'I'
   endif

   if style =~# 'g'
	" first list item is a "g" selector pattern
"	call Decho("style case g: global selector pattern")
	if A[0] < 2
	 if exists("s:AlignGPat")
	  unlet s:AlignGPat
"	  call Decho("unlet s:AlignGPat")
	 endif
	else
	 let s:AlignGPat= A[2]
"	 call Decho("s:AlignGPat<".s:AlignGPat.">")
	endif
   elseif style =~# 'v'
	" first list item is a "v" selector pattern
"	call Decho("style case v: global selector anti-pattern")
	if A[0] < 2
	 if exists("s:AlignVPat")
	  unlet s:AlignVPat
"	  call Decho("unlet s:AlignVPat")
	 endif
	else
	 let s:AlignVPat= A[2]
"	 call Decho("s:AlignVPat<".s:AlignVPat.">")
	endif
   endif

    "[-lrc+:] : set up s:AlignStyle
   if style =~# '[-lrc+:]'
"	call Decho("style case [-lrc+:]: field justification")
    let s:AlignStyle= substitute(style,'[^-lrc:+]','','g')
"    call Decho("AlignStyle<".s:AlignStyle.">")
   endif

   "[<>|] : set up s:AlignSep
   if style =~# '[<>|]'
"	call Decho("style case [-lrc+:]: separator justification")
	let s:AlignSep= substitute(style,'[^<>|]','','g')
"	call Decho("AlignSep ".s:AlignSep)
   endif
  endif

  " sanity
  if !exists("s:AlignCtrl")
   let s:AlignCtrl= '='
  endif

  " restore search and options
  let @/ = keep_search
  let &ic= keep_ic

"  call Dret("AlignCtrl ".s:AlignCtrl.'p'.s:AlignPrePad.'P'.s:AlignPostPad.s:AlignLeadKeep.s:AlignStyle)
  return s:AlignCtrl.'p'.s:AlignPrePad.'P'.s:AlignPostPad.s:AlignLeadKeep.s:AlignStyle
endfun

" ---------------------------------------------------------------------
" s:MakeSpace: returns a string with spacecnt blanks {{{1
fun! s:MakeSpace(spacecnt)
"  call Dfunc("MakeSpace(spacecnt=".a:spacecnt.")")
  let str      = ""
  let spacecnt = a:spacecnt
  while spacecnt > 0
   let str      = str . " "
   let spacecnt = spacecnt - 1
  endwhile
"  call Dret("MakeSpace <".str.">")
  return str
endfun

" ---------------------------------------------------------------------
" Align#Align: align selected text based on alignment pattern(s) {{{1
fun! Align#Align(hasctrl,...) range
"  call Dfunc("Align#Align(hasctrl=".a:hasctrl.",...) a:0=".a:0)

  " sanity checks
  if string(a:hasctrl) != "0" && string(a:hasctrl) != "1"
   echohl Error|echo 'usage: Align#Align(hasctrl<'.a:hasctrl.'> (should be 0 or 1),"separator(s)"  (you have '.a:0.') )'|echohl None
"   call Dret("Align#Align")
   return
  endif
  if exists("s:AlignStyle") && s:AlignStyle == ":"
   echohl Error |echo '(Align#Align) your AlignStyle is ":", which implies "do-no-alignment"!'|echohl None
"   call Dret("Align#Align")
   return
  endif

  " set up a list akin to an argument list
  if a:0 > 0
   let A= s:QArgSplitter(a:1)
  else
   let A=[0]
  endif

  " if :Align! was used, then the first argument is (should be!) an AlignCtrl string
  " Note that any alignment control set this way will be temporary.
  let hasctrl= a:hasctrl
"  call Decho("hasctrl=".hasctrl)
  if a:hasctrl && A[0] >= 1
"   call Decho("Align! : using A[1]<".A[1]."> for AlignCtrl")
   if A[1] =~ '[gv]'
	let hasctrl= hasctrl + 1
	call Align#AlignCtrl('m')
    call Align#AlignCtrl(A[1],A[2])
"    call Decho("Align! : also using A[2]<".A[2]."> for AlignCtrl")
   elseif A[1] !~ 'm'
    call Align#AlignCtrl(A[1]."m")
   else
    call Align#AlignCtrl(A[1])
   endif
  endif

  " Check for bad separator patterns (zero-length matches)
  let ipat= 1 + hasctrl
  while ipat <= A[0]
   if "" =~ A[ipat]
	echoerr "Align: separator<".A[ipat]."> matches zero-length string"
"    call Dret("Align#Align")
	return
   endif
   let ipat= ipat + 1
  endwhile

  " record current search pattern for subsequent restoration
  let keep_search= @/
  let keep_ic    = &ic
  let keep_report= &report
  set noic report=10000

  if A[0] > hasctrl
  " Align will accept a list of separator regexps
"   call Decho("A[0]=".A[0].": accepting list of separator regexp")

   if s:AlignCtrl =~# "="
	"= : consider all separators to be equivalent
"    call Decho("AlignCtrl: record list of equivalent alignment patterns")
    let s:AlignCtrl  = '='
    let s:AlignPat_1 = A[1 + hasctrl]
    let s:AlignPatQty= 1
    let ipat         = 2 + hasctrl
    while ipat <= A[0]
     let s:AlignPat_1 = s:AlignPat_1.'\|'.A[ipat]
     let ipat         = ipat + 1
    endwhile
    let s:AlignPat_1= '\('.s:AlignPat_1.'\)'
"    call Decho("AlignCtrl<".s:AlignCtrl."> AlignPat<".s:AlignPat_1.">")

   elseif s:AlignCtrl =~# 'C'
    "c : cycle through alignment pattern(s)
"    call Decho("AlignCtrl: cycle through alignment pattern(s)")
    let s:AlignCtrl  = 'C'
    let s:AlignPatQty= A[0] - hasctrl
    let ipat         = 1
    while ipat <= s:AlignPatQty
     let s:AlignPat_{ipat}= A[(ipat + hasctrl)]
"     call Decho("AlignCtrl<".s:AlignCtrl."> AlignQty=".s:AlignPatQty." AlignPat_".ipat."<".s:AlignPat_{ipat}.">")
     let ipat= ipat + 1
    endwhile
   endif
  endif

  " Initialize so that begline<endline and begcol<endcol.
  " Ragged right: check if the column associated with '< or '>
  "               is greater than the line's string length -> ragged right.
  " Have to be careful about visualmode() -- it returns the last visual
  " mode used whether or not it was used currently.
  let begcol   = virtcol("'<")-1
  let endcol   = virtcol("'>")-1
  if begcol > endcol
   let begcol  = virtcol("'>")-1
   let endcol  = virtcol("'<")-1
  endif
"  call Decho("begcol=".begcol." endcol=".endcol)
  let begline  = a:firstline
  let endline  = a:lastline
  if begline > endline
   let begline = a:lastline
   let endline = a:firstline
  endif
"  call Decho("begline=".begline." endline=".endline)
  let fieldcnt = 0
  if (begline == line("'>") && endline == line("'<")) || (begline == line("'<") && endline == line("'>"))
   let vmode= visualmode()
"   call Decho("vmode=".vmode)
   if vmode == "\<c-v>"
	if exists("g:Align_xstrlen") && g:Align_xstrlen
     let ragged   = ( col("'>") > s:Strlen(getline("'>")) || col("'<") > s:Strlen(getline("'<")) )
	else
     let ragged   = ( col("'>") > strlen(getline("'>")) || col("'<") > strlen(getline("'<")) )
	endif
   else
	let ragged= 1
   endif
  else
   let ragged= 1
  endif
  if ragged
   let begcol= 0
  endif
"  call Decho("lines[".begline.",".endline."] col[".begcol.",".endcol."] ragged=".ragged." AlignCtrl<".s:AlignCtrl.">")

  " Keep user options
  let etkeep   = &l:et
  let pastekeep= &l:paste
  setlocal et paste

  " convert selected range of lines to use spaces instead of tabs
  " but if first line's initial white spaces are to be retained
  " then use 'em
  if begcol <= 0 && s:AlignLeadKeep == 'I'
   " retain first leading whitespace for all subsequent lines
   let bgntxt= substitute(getline(begline),'^\(\s*\).\{-}$','\1','')
"   call Decho("retaining 1st leading whitespace: bgntxt<".bgntxt.">")
   set noet
  endif
  exe begline.",".endline."ret"

  " Execute two passes
  " First  pass: collect alignment data (max field sizes)
  " Second pass: perform alignment
  let pass= 1
  while pass <= 2
"   call Decho(" ")
"   call Decho("---- Pass ".pass.": ----")

   let line= begline
   while line <= endline
    " Process each line
    let txt = getline(line)
"    call Decho(" ")
"    call Decho("Pass".pass.": Line ".line." <".txt.">")

    " AlignGPat support: allows a selector pattern (akin to g/selector/cmd )
    if exists("s:AlignGPat")
"	 call Decho("Pass".pass.": AlignGPat<".s:AlignGPat.">")
	 if match(txt,s:AlignGPat) == -1
"	  call Decho("Pass".pass.": skipping")
	  let line= line + 1
	  continue
	 endif
    endif

    " AlignVPat support: allows a selector pattern (akin to v/selector/cmd )
    if exists("s:AlignVPat")
"	 call Decho("Pass".pass.": AlignVPat<".s:AlignVPat.">")
	 if match(txt,s:AlignVPat) != -1
"	  call Decho("Pass".pass.": skipping")
	  let line= line + 1
	  continue
	 endif
    endif

	" Always skip blank lines
	if match(txt,'^\s*$') != -1
"	  call Decho("Pass".pass.": skipping")
	 let line= line + 1
	 continue
	endif

    " Extract visual-block selected text (init bgntxt, endtxt)
	if exists("g:Align_xstrlen") && g:Align_xstrlen
     let txtlen= s:Strlen(txt)
	else
     let txtlen= strlen(txt)
	endif
    if begcol > 0
	 " Record text to left of selected area
     let bgntxt= strpart(txt,0,begcol)
"	  call Decho("Pass".pass.": record text to left: bgntxt<".bgntxt.">")
    elseif s:AlignLeadKeep == 'W'
	 let bgntxt= substitute(txt,'^\(\s*\).\{-}$','\1','')
"	  call Decho("Pass".pass.": retaining all leading ws: bgntxt<".bgntxt.">")
    elseif s:AlignLeadKeep == 'w' || !exists("bgntxt")
	 " No beginning text
	 let bgntxt= ""
"	  call Decho("Pass".pass.": no beginning text")
    endif
    if ragged
	 let endtxt= ""
    else
     " Elide any text lying outside selected columnar region
     let endtxt= strpart(txt,endcol+1,txtlen-endcol)
     let txt   = strpart(txt,begcol,endcol-begcol+1)
    endif
"    call Decho(" ")
"    call Decho("Pass".pass.": bgntxt<".bgntxt.">")
"    call Decho("Pass".pass.":    txt<". txt  .">")
"    call Decho("Pass".pass.": endtxt<".endtxt.">")
	if !exists("s:AlignPat_{1}")
	 echohl Error|echo "no separators specified!"|echohl None
"     call Dret("Align#Align")
	 return
	endif

    " Initialize for both passes
    let seppat      = s:AlignPat_{1}
    let ifield      = 1
    let ipat        = 1
    let bgnfield    = 0
    let endfield    = 0
    let alignstyle  = s:AlignStyle
    let doend       = 1
	let newtxt      = ""
    let alignprepad = s:AlignPrePad
    let alignpostpad= s:AlignPostPad
	let alignsep    = s:AlignSep
	let alignophold = " "
	let alignop     = "l"
"	call Decho("Pass".pass.": initial alignstyle<".alignstyle."> seppat<".seppat.">")

    " Process each field on the line
    while doend > 0

	  " C-style: cycle through pattern(s)
     if s:AlignCtrl == 'C' && doend == 1
	  let seppat   = s:AlignPat_{ipat}
"	  call Decho("Pass".pass.": processing field: AlignCtrl=".s:AlignCtrl." ipat=".ipat." seppat<".seppat.">")
	  let ipat     = ipat + 1
	  if ipat > s:AlignPatQty
	   let ipat = 1
	  endif
     endif

	 " cyclic alignment/justification operator handling
	 let alignophold  = alignop
	 let alignop      = strpart(alignstyle,0,1)
	 if alignop == '+' || doend == 2
	  let alignop= alignophold
	 else
	  let alignstyle   = strpart(alignstyle,1).strpart(alignstyle,0,1)
	  let alignopnxt   = strpart(alignstyle,0,1)
	  if alignop == ':'
	   let seppat  = '$'
	   let doend   = 2
"	   call Decho("Pass".pass.": alignop<:> case: setting seppat<$> doend==2")
	  endif
	 endif

	 " cylic separator alignment specification handling
	 let alignsepop= strpart(alignsep,0,1)
	 let alignsep  = strpart(alignsep,1).alignsepop

	 " mark end-of-field and the subsequent end-of-separator.
	 " Extend field if alignop is '-'
     let endfield = match(txt,seppat,bgnfield)
	 let sepfield = matchend(txt,seppat,bgnfield)
     let skipfield= sepfield
"	 call Decho("Pass".pass.": endfield=match(txt<".txt.">,seppat<".seppat.">,bgnfield=".bgnfield.")=".endfield)
	 while alignop == '-' && endfield != -1
	  let endfield  = match(txt,seppat,skipfield)
	  let sepfield  = matchend(txt,seppat,skipfield)
	  let skipfield = sepfield
	  let alignop   = strpart(alignstyle,0,1)
	  let alignstyle= strpart(alignstyle,1).strpart(alignstyle,0,1)
"	  call Decho("Pass".pass.": extend field: endfield<".strpart(txt,bgnfield,endfield-bgnfield)."> alignop<".alignop."> alignstyle<".alignstyle.">")
	 endwhile
	 let seplen= sepfield - endfield
"	 call Decho("Pass".pass.": seplen=[sepfield=".sepfield."] - [endfield=".endfield."]=".seplen)

	 if endfield != -1
	  if pass == 1
	   " ---------------------------------------------------------------------
	   " Pass 1: Update FieldSize to max
"	   call Decho("Pass".pass.": before lead/trail remove: field<".strpart(txt,bgnfield,endfield-bgnfield).">")
	   let field      = substitute(strpart(txt,bgnfield,endfield-bgnfield),'^\s*\(.\{-}\)\s*$','\1','')
       if s:AlignLeadKeep == 'W'
	    let field = bgntxt.field
	    let bgntxt= ""
	   endif
	   if exists("g:Align_xstrlen") && g:Align_xstrlen
	    let fieldlen   = s:Strlen(field)
	   else
	    let fieldlen   = strlen(field)
	   endif
	   let sFieldSize = "FieldSize_".ifield
	   if !exists(sFieldSize)
	    let FieldSize_{ifield}= fieldlen
"	    call Decho("Pass".pass.":  set FieldSize_{".ifield."}=".FieldSize_{ifield}." <".field.">")
	   elseif fieldlen > FieldSize_{ifield}
	    let FieldSize_{ifield}= fieldlen
"	    call Decho("Pass".pass.": oset FieldSize_{".ifield."}=".FieldSize_{ifield}." <".field.">")
	   endif
	   let sSepSize= "SepSize_".ifield
	   if !exists(sSepSize)
		let SepSize_{ifield}= seplen
"	    call Decho(" set SepSize_{".ifield."}=".SepSize_{ifield}." <".field.">")
	   elseif seplen > SepSize_{ifield}
		let SepSize_{ifield}= seplen
"	    call Decho("Pass".pass.": oset SepSize_{".ifield."}=".SepSize_{ifield}." <".field.">")
	   endif

	  else
	   " ---------------------------------------------------------------------
	   " Pass 2: Perform Alignment
	   let prepad       = strpart(alignprepad,0,1)
	   let postpad      = strpart(alignpostpad,0,1)
	   let alignprepad  = strpart(alignprepad,1).strpart(alignprepad,0,1)
	   let alignpostpad = strpart(alignpostpad,1).strpart(alignpostpad,0,1)
	   let field        = substitute(strpart(txt,bgnfield,endfield-bgnfield),'^\s*\(.\{-}\)\s*$','\1','')
       if s:AlignLeadKeep == 'W'
	    let field = bgntxt.field
	    let bgntxt= ""
	   endif
	   if doend == 2
		let prepad = 0
		let postpad= 0
	   endif
	   if exists("g:Align_xstrlen") && g:Align_xstrlen
	    let fieldlen   = s:Strlen(field)
	   else
	    let fieldlen   = strlen(field)
	   endif
	   let sep        = s:MakeSpace(prepad).strpart(txt,endfield,sepfield-endfield).s:MakeSpace(postpad)
	   if seplen < SepSize_{ifield}
		if alignsepop == "<"
		 " left-justify separators
		 let sep       = sep.s:MakeSpace(SepSize_{ifield}-seplen)
		elseif alignsepop == ">"
		 " right-justify separators
		 let sep       = s:MakeSpace(SepSize_{ifield}-seplen).sep
		else
		 " center-justify separators
		 let sepleft   = (SepSize_{ifield} - seplen)/2
		 let sepright  = SepSize_{ifield} - seplen - sepleft
		 let sep       = s:MakeSpace(sepleft).sep.s:MakeSpace(sepright)
		endif
	   endif
	   let spaces     = FieldSize_{ifield} - fieldlen
"	   call Decho("Pass".pass.": Field #".ifield."<".field."> spaces=".spaces." be[".bgnfield.",".endfield."] pad=".prepad.','.postpad." FS_".ifield."<".FieldSize_{ifield}."> sep<".sep."> ragged=".ragged." doend=".doend." alignop<".alignop.">")

	    " Perform alignment according to alignment style justification
	   if spaces > 0
	    if     alignop == 'c'
		 " center the field
	     let spaceleft = spaces/2
	     let spaceright= FieldSize_{ifield} - spaceleft - fieldlen
	     let newtxt    = newtxt.s:MakeSpace(spaceleft).field.s:MakeSpace(spaceright).sep
	    elseif alignop == 'r'
		 " right justify the field
	     let newtxt= newtxt.s:MakeSpace(spaces).field.sep
	    elseif ragged && doend == 2
		 " left justify rightmost field (no trailing blanks needed)
	     let newtxt= newtxt.field
		else
		 " left justfiy the field
	     let newtxt= newtxt.field.s:MakeSpace(spaces).sep
	    endif
	   elseif ragged && doend == 2
		" field at maximum field size and no trailing blanks needed
	    let newtxt= newtxt.field
	   else
		" field is at maximum field size already
	    let newtxt= newtxt.field.sep
	   endif
"	   call Decho("Pass".pass.": newtxt<".newtxt.">")
	  endif	" pass 1/2

	  " bgnfield indexes to end of separator at right of current field
	  " Update field counter
	  let bgnfield= sepfield
      let ifield  = ifield + 1
	  if doend == 2
	   let doend= 0
	  endif
	   " handle end-of-text as end-of-field
	 elseif doend == 1
	  let seppat  = '$'
	  let doend   = 2
	 else
	  let doend   = 0
	 endif		" endfield != -1
    endwhile	" doend loop (as well as regularly separated fields)

	if pass == 2
	 " Write altered line to buffer
"     call Decho("Pass".pass.": bgntxt<".bgntxt."> line=".line)
"     call Decho("Pass".pass.": newtxt<".newtxt.">")
"     call Decho("Pass".pass.": endtxt<".endtxt.">")
	 call setline(line,bgntxt.newtxt.endtxt)
	endif

    let line = line + 1
   endwhile	" line loop

   let pass= pass + 1
  endwhile	" pass loop
"  call Decho("end of two pass loop")

  " Restore user options
  let &l:et    = etkeep
  let &l:paste = pastekeep

  if exists("s:DoAlignPop")
   " AlignCtrl Map support
   call Align#AlignPop()
   unlet s:DoAlignPop
  endif

  " restore current search pattern
  let @/      = keep_search
  let &ic     = keep_ic
  let &report = keep_report

"  call Dret("Align#Align")
  return
endfun

" ---------------------------------------------------------------------
" Align#AlignPush: this command/function pushes an alignment control string onto a stack {{{1
fun! Align#AlignPush()
"  call Dfunc("AlignPush()")

  " initialize the stack
  if !exists("s:AlignCtrlStackQty")
   let s:AlignCtrlStackQty= 1
  else
   let s:AlignCtrlStackQty= s:AlignCtrlStackQty + 1
  endif

  " construct an AlignCtrlStack entry
  if !exists("s:AlignSep")
   let s:AlignSep= ''
  endif
  let s:AlignCtrlStack_{s:AlignCtrlStackQty}= s:AlignCtrl.'p'.s:AlignPrePad.'P'.s:AlignPostPad.s:AlignLeadKeep.s:AlignStyle.s:AlignSep
"  call Decho("AlignPush: AlignCtrlStack_".s:AlignCtrlStackQty."<".s:AlignCtrlStack_{s:AlignCtrlStackQty}.">")

  " push [GV] patterns onto their own stack
  if exists("s:AlignGPat")
   let s:AlignGPat_{s:AlignCtrlStackQty}= s:AlignGPat
  else
   let s:AlignGPat_{s:AlignCtrlStackQty}=  ""
  endif
  if exists("s:AlignVPat")
   let s:AlignVPat_{s:AlignCtrlStackQty}= s:AlignVPat
  else
   let s:AlignVPat_{s:AlignCtrlStackQty}=  ""
  endif

"  call Dret("AlignPush")
endfun

" ---------------------------------------------------------------------
" Align#AlignPop: this command/function pops an alignment pattern from a stack {{{1
"           and into the AlignCtrl variables.
fun! Align#AlignPop()
"  call Dfunc("Align#AlignPop()")

  " sanity checks
  if !exists("s:AlignCtrlStackQty")
   echoerr "AlignPush needs to be used prior to AlignPop"
"   call Dret("Align#AlignPop <> : AlignPush needs to have been called first")
   return ""
  endif
  if s:AlignCtrlStackQty <= 0
   unlet s:AlignCtrlStackQty
   echoerr "AlignPush needs to be used prior to AlignPop"
"   call Dret("Align#AlignPop <> : AlignPop needs to have been called first")
   return ""
  endif

  " pop top of AlignCtrlStack and pass value to AlignCtrl
  let retval=s:AlignCtrlStack_{s:AlignCtrlStackQty}
  unlet s:AlignCtrlStack_{s:AlignCtrlStackQty}
  call Align#AlignCtrl(retval)

  " pop G pattern stack
  if s:AlignGPat_{s:AlignCtrlStackQty} != ""
   call Align#AlignCtrl('g',s:AlignGPat_{s:AlignCtrlStackQty})
  else
   call Align#AlignCtrl('g')
  endif
  unlet s:AlignGPat_{s:AlignCtrlStackQty}

  " pop V pattern stack
  if s:AlignVPat_{s:AlignCtrlStackQty} != ""
   call Align#AlignCtrl('v',s:AlignVPat_{s:AlignCtrlStackQty})
  else
   call Align#AlignCtrl('v')
  endif

  unlet s:AlignVPat_{s:AlignCtrlStackQty}
  let s:AlignCtrlStackQty= s:AlignCtrlStackQty - 1

"  call Dret("Align#AlignPop <".retval."> : AlignCtrlStackQty=".s:AlignCtrlStackQty)
  return retval
endfun

" ---------------------------------------------------------------------
" Align#AlignReplaceQuotedSpaces: {{{1
fun! Align#AlignReplaceQuotedSpaces()
"  call Dfunc("AlignReplaceQuotedSpaces()")

  let l:line          = getline(line("."))
  if exists("g:Align_xstrlen") && g:Align_xstrlen
   let l:linelen      = s:Strlen(l:line)
  else
   let l:linelen      = strlen(l:line)
  endif
  let l:startingPos   = 0
  let l:startQuotePos = 0
  let l:endQuotePos   = 0
  let l:spacePos      = 0
  let l:quoteRe       = '\\\@<!"'

"  "call Decho("in replace spaces.  line=" . line('.'))
  while (1)
    let l:startQuotePos = match(l:line, l:quoteRe, l:startingPos)
    if (l:startQuotePos < 0)
"      "call Decho("No more quotes to the end of line")
      break
    endif
    let l:endQuotePos = match(l:line, l:quoteRe, l:startQuotePos + 1)
    if (l:endQuotePos < 0)
"      "call Decho("Mismatched quotes")
      break
    endif
    let l:spaceReplaceRe = '^.\{' . (l:startQuotePos + 1) . '}.\{-}\zs\s\ze.*.\{' . (linelen - l:endQuotePos) . '}$'
"    "call Decho('spaceReplaceRe="' . l:spaceReplaceRe . '"')
    let l:newStr = substitute(l:line, l:spaceReplaceRe, '%', '')
    while (l:newStr != l:line)
"      "call Decho('newstr="' . l:newStr . '"')
      let l:line = l:newStr
      let l:newStr = substitute(l:line, l:spaceReplaceRe, '%', '')
    endwhile
    let l:startingPos = l:endQuotePos + 1
  endwhile
  call setline(line('.'), l:line)

"  call Dret("AlignReplaceQuotedSpaces")
endfun

" ---------------------------------------------------------------------
" s:QArgSplitter: to avoid \ processing by <f-args>, <q-args> is needed. {{{1
" However, <q-args> doesn't split at all, so this function returns a list
" of arguments which has been:
"   * split at whitespace
"   * unless inside "..."s.  One may escape characters with a backslash inside double quotes.
" along with a leading length-of-list.
"
"   Examples:   %Align "\""   will align on "s
"               %Align " "    will align on spaces
"
" The resulting list:  qarglist[0] corresponds to a:0
"                      qarglist[i] corresponds to a:{i}
fun! s:QArgSplitter(qarg)
"  call Dfunc("s:QArgSplitter(qarg<".a:qarg.">)")

  if a:qarg =~ '".*"'
   " handle "..." args, which may include whitespace
   let qarglist = []
   let args     = a:qarg
"   call Decho("handle quoted arguments: args<".args.">")
   while args != ""
	let iarg   = 0
	let arglen = strlen(args)
"	call Decho("args[".iarg."]<".args[iarg]."> arglen=".arglen)
	" find index to first not-escaped '"'
	while args[iarg] != '"' && iarg < arglen
	 if args[iarg] == '\'
	  let args= strpart(args,1)
	 endif
	 let iarg= iarg + 1
	endwhile
"	call Decho("args<".args."> iarg=".iarg." arglen=".arglen)

	if iarg > 0
	 " handle left of quote or remaining section
"	 call Decho("handle left of quote or remaining section")
	 if args[iarg] == '"'
	  let qarglist= qarglist + split(strpart(args,0,iarg-1))
	 else
	  let qarglist= qarglist + split(strpart(args,0,iarg))
	 endif
	 let args    = strpart(args,iarg)
	 let arglen  = strlen(args)

	elseif iarg < arglen && args[0] == '"'
	 " handle "quoted" section
"	 call Decho("handle quoted section")
	 let iarg= 1
	 while args[iarg] != '"' && iarg < arglen
	  if args[iarg] == '\'
	   let args= strpart(args,1)
	  endif
	  let iarg= iarg + 1
	 endwhile
"	 call Decho("args<".args."> iarg=".iarg." arglen=".arglen)
	 if args[iarg] == '"'
	  call add(qarglist,strpart(args,1,iarg-1))
	  let args= strpart(args,iarg+1)
	 else
	  let qarglist = qarglist + split(args)
	  let args     = ""
	 endif
	endif
"	call Decho("qarglist".string(qarglist)." iarg=".iarg." args<".args.">")
   endwhile

  else
   " split at all whitespace
   let qarglist= split(a:qarg)
  endif

  let qarglistlen= len(qarglist)
  let qarglist   = insert(qarglist,qarglistlen)
"  call Dret("s:QArgSplitter ".string(qarglist))
  return qarglist
endfun

" ---------------------------------------------------------------------
" s:Strlen: this function returns the length of a string, even if its {{{1
"           using two-byte etc characters.
"           Currently, its only used if g:Align_xstrlen is set to a
"           nonzero value.  Solution from Nicolai Weibull, vim docs
"           (:help strlen()), Tony Mechelynck, and my own invention.
fun! s:Strlen(x)
"  call Dfunc("s:Strlen(x<".a:x.">")
  if g:Align_xstrlen == 1
   " number of codepoints (Latin a + combining circumflex is two codepoints)
   " (comment from TM, solution from NW)
   let ret= strlen(substitute(a:x,'.','c','g'))

  elseif g:Align_xstrlen == 2
   " number of spacing codepoints (Latin a + combining circumflex is one spacing
   " codepoint; a hard tab is one; wide and narrow CJK are one each; etc.)
   " (comment from TM, solution from TM)
   let ret=strlen(substitute(a:x, '.\Z', 'x', 'g'))

  elseif g:Align_xstrlen == 3
   " virtual length (counting, for instance, tabs as anything between 1 and
   " 'tabstop', wide CJK as 2 rather than 1, Arabic alif as zero when immediately
   " preceded by lam, one otherwise, etc.)
   " (comment from TM, solution from me)
   let modkeep= &l:mod
   exe "norm! o\<esc>"
   call setline(line("."),a:x)
   let ret= virtcol("$") - 1
   d
   let &l:mod= modkeep

  else
   " at least give a decent default
   ret= strlen(a:x)
  endif
"  call Dret("s:Strlen ".ret)
  return ret
endfun

" ---------------------------------------------------------------------
" Set up default values: {{{1
"call Decho("-- Begin AlignCtrl Initialization --")
call Align#AlignCtrl("default")
"call Decho("-- End AlignCtrl Initialization --")

" ---------------------------------------------------------------------
"  Restore: {{{1
let &cpo= s:keepcpo
unlet s:keepcpo
" vim: ts=4 fdm=marker
autoload/AlignMaps.vim	[[[1
330
" AlignMaps.vim : support functions for AlignMaps
"   Author: Charles E. Campbell, Jr.
"   Date:   Mar 03, 2009
" Version:           41
" ---------------------------------------------------------------------
"  Load Once: {{{1
if &cp || exists("g:loaded_AlignMaps")
 finish
endif
let g:loaded_AlignMaps= "v41"
let s:keepcpo         = &cpo
set cpo&vim

" =====================================================================
" Functions: {{{1

" ---------------------------------------------------------------------
" AlignMaps#WrapperStart: {{{2
fun! AlignMaps#WrapperStart(vis) range
"  call Dfunc("AlignMaps#WrapperStart(vis=".a:vis.")")

  if a:vis
   norm! '<ma'>
  endif

  if line("'y") == 0 || line("'z") == 0 || !exists("s:alignmaps_wrapcnt") || s:alignmaps_wrapcnt <= 0
"   call Decho("wrapper initialization")
   let s:alignmaps_wrapcnt    = 1
   let s:alignmaps_keepgd     = &gdefault
   let s:alignmaps_keepsearch = @/
   let s:alignmaps_keepch     = &ch
   let s:alignmaps_keepmy     = SaveMark("'y")
   let s:alignmaps_keepmz     = SaveMark("'z")
   let s:alignmaps_posn       = SaveWinPosn(0)
   " set up fencepost blank lines
   put =''
   norm! mz'a
   put! =''
   ky
   let s:alignmaps_zline      = line("'z")
   exe "'y,'zs/@/\177/ge"
  else
"   call Decho("embedded wrapper")
   let s:alignmaps_wrapcnt    = s:alignmaps_wrapcnt + 1
   norm! 'yjma'zk
  endif

  " change some settings to align-standard values
  set nogd
  set ch=2
  AlignPush
  norm! 'zk
"  call Dret("AlignMaps#WrapperStart : alignmaps_wrapcnt=".s:alignmaps_wrapcnt." my=".line("'y")." mz=".line("'z"))
endfun

" ---------------------------------------------------------------------
" AlignMaps#WrapperEnd:	{{{2
fun! AlignMaps#WrapperEnd() range
"  call Dfunc("AlignMaps#WrapperEnd() alignmaps_wrapcnt=".s:alignmaps_wrapcnt." my=".line("'y")." mz=".line("'z"))

  " remove trailing white space introduced by whatever in the modification zone
  'y,'zs/ \+$//e

  " restore AlignCtrl settings
  AlignPop

  let s:alignmaps_wrapcnt= s:alignmaps_wrapcnt - 1
  if s:alignmaps_wrapcnt <= 0
   " initial wrapper ending
   exe "'y,'zs/\177/@/ge"

   " if the 'z line hasn't moved, then go ahead and restore window position
   let zstationary= s:alignmaps_zline == line("'z")

   " remove fencepost blank lines.
   " restore 'a
   norm! 'yjmakdd'zdd

   " restore original 'y, 'z, and window positioning
   call RestoreMark(s:alignmaps_keepmy)
   call RestoreMark(s:alignmaps_keepmz)
   if zstationary > 0
    call RestoreWinPosn(s:alignmaps_posn)
"    call Decho("restored window positioning")
   endif

   " restoration of options
   let &gd= s:alignmaps_keepgd
   let &ch= s:alignmaps_keepch
   let @/ = s:alignmaps_keepsearch

   " remove script variables
   unlet s:alignmaps_keepch
   unlet s:alignmaps_keepsearch
   unlet s:alignmaps_keepmy
   unlet s:alignmaps_keepmz
   unlet s:alignmaps_keepgd
   unlet s:alignmaps_posn
  endif

"  call Dret("AlignMaps#WrapperEnd : alignmaps_wrapcnt=".s:alignmaps_wrapcnt." my=".line("'y")." mz=".line("'z"))
endfun

" ---------------------------------------------------------------------
" AlignMaps#StdAlign: some semi-standard align calls {{{2
fun! AlignMaps#StdAlign(mode) range
"  call Dfunc("AlignMaps#StdAlign(mode=".a:mode.")")
  if     a:mode == 1
   " align on @
"   call Decho("align on @")
   AlignCtrl mIp1P1=l @
   'a,.Align
  elseif a:mode == 2
   " align on @, retaining all initial white space on each line
"   call Decho("align on @, retaining all initial white space on each line")
   AlignCtrl mWp1P1=l @
   'a,.Align
  elseif a:mode == 3
   " like mode 2, but ignore /* */-style comments
"   call Decho("like mode 2, but ignore /* */-style comments")
   AlignCtrl v ^\s*/[/*]
   AlignCtrl mWp1P1=l @
   'a,.Align
  else
   echoerr "(AlignMaps) AlignMaps#StdAlign doesn't support mode#".a:mode
  endif
"  call Dret("AlignMaps#StdAlign")
endfun

" ---------------------------------------------------------------------
" AlignMaps#CharJoiner: joins lines which end in the given character (spaces {{{2
"             at end are ignored)
fun! AlignMaps#CharJoiner(chr)
"  call Dfunc("AlignMaps#CharJoiner(chr=".a:chr.")")
  let aline = line("'a")
  let rep   = line(".") - aline
  while rep > 0
	norm! 'a
	while match(getline(aline),a:chr . "\s*$") != -1 && rep >= 0
	  " while = at end-of-line, delete it and join with next
	  norm! 'a$
	  j!
	  let rep = rep - 1
	endwhile
	" update rep(eat) count
	let rep = rep - 1
	if rep <= 0
	  " terminate loop if at end-of-block
	  break
	endif
	" prepare for next line
	norm! jma
	let aline = line("'a")
  endwhile
"  call Dret("AlignMaps#CharJoiner")
endfun

" ---------------------------------------------------------------------
" AlignMaps#Equals: supports \t= and \T= {{{2
fun! AlignMaps#Equals() range
"  call Dfunc("AlignMaps#Equals()")
  'a,'zs/\s\+\([*/+\-%|&\~^]\==\)/ \1/e
  'a,'zs@ \+\([*/+\-%|&\~^]\)=@\1=@ge
  'a,'zs/==/\="\<Char-0x0f>\<Char-0x0f>"/ge
  'a,'zs/\([!<>:]\)=/\=submatch(1)."\<Char-0x0f>"/ge
  norm g'zk
  AlignCtrl mIp1P1=l =
  AlignCtrl g =
  'a,'z-1Align
  'a,'z-1s@\([*/+\-%|&\~^!=]\)\( \+\)=@\2\1=@ge
  'a,'z-1s/\( \+\);/;\1/ge
  if &ft == "c" || &ft == "cpp"
"   call Decho("exception for ".&ft)
   'a,'z-1v/^\s*\/[*/]/s/\/[*/]/@&@/e
   'a,'z-1v/^\s*\/[*/]/s/\*\//@&/e
   if exists("g:mapleader")
    exe "norm 'zk"
    call AlignMaps#StdAlign(1)
   else
    exe "norm 'zk"
    call AlignMaps#StdAlign(1)
   endif
   'y,'zs/^\(\s*\) @/\1/e
  endif
  'a,'z-1s/\%x0f/=/ge
  'y,'zs/ @//eg
"  call Dret("AlignMaps#Equals")
endfun

" ---------------------------------------------------------------------
" AlignMaps#Afnc: useful for splitting one-line function beginnings {{{2
"            into one line per argument format
fun! AlignMaps#Afnc()
"  call Dfunc("AlignMaps#Afnc()")

  " keep display quiet
  let chkeep = &ch
  let gdkeep = &gd
  let vekeep = &ve
  set ch=2 nogd ve=

  " will use marks y,z ; save current values
  let mykeep = SaveMark("'y")
  let mzkeep = SaveMark("'z")

  " Find beginning of function -- be careful to skip over comments
  let cmmntid  = synIDtrans(hlID("Comment"))
  let stringid = synIDtrans(hlID("String"))
  exe "norm! ]]"
  while search(")","bW") != 0
"   call Decho("line=".line(".")." col=".col("."))
   let parenid= synIDtrans(synID(line("."),col("."),1))
   if parenid != cmmntid && parenid != stringid
	break
   endif
  endwhile
  norm! %my
  s/(\s*\(\S\)/(\r  \1/e
  exe "norm! `y%"
  s/)\s*\(\/[*/]\)/)\r\1/e
  exe "norm! `y%mz"
  'y,'zs/\s\+$//e
  'y,'zs/^\s\+//e
  'y+1,'zs/^/  /

  " insert newline after every comma only one parenthesis deep
  sil! exe "norm! `y\<right>h"
  let parens   = 1
  let cmmnt    = 0
  let cmmntline= -1
  while parens >= 1
"   call Decho("parens=".parens." @a=".@a)
   exe 'norm! ma "ay`a '
   if @a == "("
    let parens= parens + 1
   elseif @a == ")"
    let parens= parens - 1

   " comment bypass:  /* ... */  or //...
   elseif cmmnt == 0 && @a == '/'
    let cmmnt= 1
   elseif cmmnt == 1
	if @a == '/'
	 let cmmnt    = 2   " //...
	 let cmmntline= line(".")
	elseif @a == '*'
	 let cmmnt= 3   " /*...
	else
	 let cmmnt= 0
	endif
   elseif cmmnt == 2 && line(".") != cmmntline
	let cmmnt    = 0
	let cmmntline= -1
   elseif cmmnt == 3 && @a == '*'
	let cmmnt= 4
   elseif cmmnt == 4
	if @a == '/'
	 let cmmnt= 0   " ...*/
	elseif @a != '*'
	 let cmmnt= 3
	endif

   elseif @a == "," && parens == 1 && cmmnt == 0
	exe "norm! i\<CR>\<Esc>"
   endif
  endwhile
  norm! `y%mz%
  sil! 'y,'zg/^\s*$/d

  " perform substitutes to mark fields for Align
  sil! 'y+1,'zv/^\//s/^\s\+\(\S\)/  \1/e
  sil! 'y+1,'zv/^\//s/\(\S\)\s\+/\1 /eg
  sil! 'y+1,'zv/^\//s/\* \+/*/ge
  sil! 'y+1,'zv/^\//s/\w\zs\s*\*/ */ge
  "                                                 func
  "                    ws  <- declaration   ->    <-ptr  ->   <-var->    <-[array][]    ->   <-glop->      <-end->
  sil! 'y+1,'zv/^\//s/^\s*\(\(\K\k*\s*\)\+\)\s\+\([(*]*\)\s*\(\K\k*\)\s*\(\(\[.\{-}]\)*\)\s*\(.\{-}\)\=\s*\([,)]\)\s*$/  \1@#\3@\4\5@\7\8/e
  sil! 'y+1,'z+1g/^\s*\/[*/]/norm! kJ
  sil! 'y+1,'z+1s%/[*/]%@&@%ge
  sil! 'y+1,'z+1s%*/%@&%ge
  AlignCtrl mIp0P0=l @
  sil! 'y+1,'zAlign
  sil! 'y,'zs%@\(/[*/]\)@%\t\1 %e
  sil! 'y,'zs%@\*/% */%e
  sil! 'y,'zs/@\([,)]\)/\1/
  sil! 'y,'zs/@/ /
  AlignCtrl mIlrp0P0= # @
  sil! 'y+1,'zAlign
  sil! 'y+1,'zs/#/ /
  sil! 'y+1,'zs/@//
  sil! 'y+1,'zs/\(\s\+\)\([,)]\)/\2\1/e

  " Restore
  call RestoreMark(mykeep)
  call RestoreMark(mzkeep)
  let &ch= chkeep
  let &gd= gdkeep
  let &ve= vekeep

"  call Dret("AlignMaps#Afnc")
endfun

" ---------------------------------------------------------------------
"  AlignMaps#FixMultiDec: converts a   type arg,arg,arg;   line to multiple lines {{{2
fun! AlignMaps#FixMultiDec()
"  call Dfunc("AlignMaps#FixMultiDec()")

  " save register x
  let xkeep   = @x
  let curline = getline(".")
"  call Decho("curline<".curline.">")

  " Get the type.  I'm assuming one type per line (ie.  int x; double y;   on one line will not be handled properly)
  let @x=substitute(curline,'^\(\s*[a-zA-Z_ \t][a-zA-Z0-9_ \t]*\)\s\+[(*]*\h.*$','\1','')
"  call Decho("@x<".@x.">")

  " transform line
  exe 's/,/;\r'.@x.' /ge'

  "restore register x
  let @x= xkeep

"  call Dret("AlignMaps#FixMultiDec : my=".line("'y")." mz=".line("'z"))
endfun

" ---------------------------------------------------------------------
"  Restore: {{{1
let &cpo= s:keepcpo
unlet s:keepcpo
" vim: ts=4 fdm=marker
