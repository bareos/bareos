" SQLUtilities:   Variety of tools for writing SQL
"   Author:	      David Fishburn <dfishburn dot vim at gmail dot com>
"   Date:	      Nov 23, 2002
"   Last Changed: 2010 Aug 14
"   Version:	  4.0.0
"   Script:	      http://www.vim.org/script.php?script_id=492
"   License:      GPL (http://www.gnu.org/licenses/gpl.html)
"
"   Dependencies:
"        Align.vim - Version 15 (as a minimum)
"                  - Author: Charles E. Campbell, Jr.
"                  - http://www.vim.org/script.php?script_id=294
"   Documentation:
"        :h SQLUtilities.txt
"

" Prevent duplicate loading
if exists("g:loaded_sqlutilities") || &cp
    finish
endif
let g:loaded_sqlutilities = 400

if !exists('g:sqlutil_align_where')
    let g:sqlutil_align_where = 1
endif

if !exists('g:sqlutil_align_comma')
    let g:sqlutil_align_comma = 0
endif

if !exists('g:sqlutil_wrap_expressions')
    let g:sqlutil_wrap_expressions = 0
endif

if !exists('g:sqlutil_align_first_word')
    let g:sqlutil_align_first_word = 1
endif

if !exists('g:sqlutil_align_keyword_right')
    let g:sqlutil_align_keyword_right = 1
endif

if !exists('g:sqlutil_cmd_terminator')
    let g:sqlutil_cmd_terminator = ';'
endif

if !exists('g:sqlutil_stmt_keywords')
    let g:sqlutil_stmt_keywords = 'select,insert,update,delete,with,merge'
endif

if !exists('g:sqlutil_keyword_case')
    " This controls whether keywords should be made
    " upper or lower case.
    " The default is to leave them in current case.
    let g:sqlutil_keyword_case = ''
endif

if !exists('g:sqlutil_use_tbl_alias')
    " If this is set to 1, when you run SQLU_CreateColumnList
    " and you do not specify a 3rd parameter, you will be
    " prompted for the alias name to append to the column list.
    "
    " The default for the alias are the initials for the table:
    "     some_thing_with_under_bars -> stwub
    "     somethingwithout           -> s
    "
    " The default is no asking
    "   d - use by default
    "   a - ask (prompt)
    "   n - no
    let g:sqlutil_use_tbl_alias = 'a'
endif

if !exists('g:sqlutil_col_list_terminators')
    " You can override which keywords will determine
    " when a column list finishes:
    "        CREATE TABLE customer (
    "        	id	INT DEFAULT AUTOINCREMENT,
    "        	last_modified TIMESTAMP NULL,
    "        	first_name     	VARCHAR(30) NOT NULL,
    "        	last_name	VARCHAR(60) NOT NULL,
    "        	balance	        NUMERIC(10,2),
    "        	PRIMARY KEY( id )
    "        );
    " So in the above example, when "primary" is reached, we
    " know the column list is complete.
    "     PRIMARY KEY
    "     foreign keys
    "     indicies
    "     check contraints
    "     table contraints
    "     foreign keys
    "
    let g:sqlutil_col_list_terminators =
                \ 'primary\s\+key.*(' .
                \ ',references' .
                \ ',match' .
                \ ',unique' .
                \ ',check' .
                \ ',constraint' .
                \ ',\%(not\s\+null\s\+\)\?foreign'
endif
" Determines which menu items will be recreated
let s:sqlutil_menus_created = 0

" Public Interface:
command! -range=% -nargs=* SQLUFormatStmts <line1>,<line2>
            \ call SQLUtilities#SQLU_FormatStmts(<q-args>)
command! -range -nargs=* SQLUFormatter <line1>,<line2>
            \ call SQLUtilities#SQLU_Formatter(<q-args>)
command!        -nargs=* SQLUCreateColumnList
            \ call SQLU_CreateColumnList(<f-args>)
command!        -nargs=* SQLUGetColumnDef
            \ call SQLU_GetColumnDef(<f-args>)
command!        -nargs=* SQLUGetColumnDataType
            \ call SQLU_GetColumnDef(expand("<cword>"), 1)
command!        -nargs=* SQLUCreateProcedure
            \ call SQLU_CreateProcedure(<f-args>)
command!        -nargs=* SQLUToggleValue
            \ call SQLU_ToggleValue(<f-args>)

if !exists("g:sqlutil_load_default_maps")
    let g:sqlutil_load_default_maps = 1
endif

if(g:sqlutil_load_default_maps == 1)
    if !hasmapto('<Plug>SQLUFormatStmts')
        nmap <unique> <Leader>sfr <Plug>SQLUFormatStmts
        vmap <unique> <Leader>sfr <Plug>SQLUFormatStmts
    endif
    if !hasmapto('<Plug>SQLUFormatter')
        nmap <unique> <Leader>sfs <Plug>SQLUFormatter
        vmap <unique> <Leader>sfs <Plug>SQLUFormatter
        nmap <unique> <Leader>sf <Plug>SQLUFormatter
        vmap <unique> <Leader>sf <Plug>SQLUFormatter
    endif
    if !hasmapto('<Plug>SQLUCreateColumnList')
        nmap <unique> <Leader>scl <Plug>SQLUCreateColumnList
    endif
    if !hasmapto('<Plug>SQLUGetColumnDef')
        nmap <unique> <Leader>scd <Plug>SQLUGetColumnDef
    endif
    if !hasmapto('<Plug>SQLUGetColumnDataType')
        nmap <unique> <Leader>scdt <Plug>SQLUGetColumnDataType
    endif
    if !hasmapto('<Plug>SQLUCreateProcedure')
        nmap <unique> <Leader>scp <Plug>SQLUCreateProcedure
    endif
endif

if exists("g:loaded_sqlutilities_global_maps")
    vunmap <unique> <script> <Plug>SQLUFormatStmts
    nunmap <unique> <script> <Plug>SQLUFormatStmts
    vunmap <unique> <script> <Plug>SQLUFormatter
    nunmap <unique> <script> <Plug>SQLUFormatter
    nunmap <unique> <script> <Plug>SQLUCreateColumnList
    nunmap <unique> <script> <Plug>SQLUGetColumnDef
    nunmap <unique> <script> <Plug>SQLUGetColumnDataType
    nunmap <unique> <script> <Plug>SQLUCreateProcedure
endif

" Global Maps:
vmap <unique> <script> <Plug>SQLUFormatStmts       :SQLUFormatStmts v<CR>
nmap <unique> <script> <Plug>SQLUFormatStmts       :SQLUFormatStmts n<CR>
vmap <unique> <script> <Plug>SQLUFormatter         :SQLUFormatter v<CR>
nmap <unique> <script> <Plug>SQLUFormatter         :SQLUFormatter n<CR>
nmap <unique> <script> <Plug>SQLUCreateColumnList  :SQLUCreateColumnList<CR>
nmap <unique> <script> <Plug>SQLUGetColumnDef      :SQLUGetColumnDef<CR>
nmap <unique> <script> <Plug>SQLUGetColumnDataType :SQLUGetColumnDataType<CR>
nmap <unique> <script> <Plug>SQLUCreateProcedure   :SQLUCreateProcedure<CR>
let g:loaded_sqlutilities_global_maps = 1

if !exists('g:sqlutil_default_menu_mode')
    let g:sqlutil_default_menu_mode = 3
endif

function! SQLU_Menu()
    if has("menu") && g:sqlutil_default_menu_mode != 0
        if g:sqlutil_default_menu_mode == 1
            let menuRoot     = 'SQLUtil'
            let menuPriority = exists("g:sqlutil_menu_priority") ? g:sqlutil_menu_priority : ''
        elseif g:sqlutil_default_menu_mode == 2
            let menuRoot     = '&SQLUtil'
            let menuPriority = exists("g:sqlutil_menu_priority") ? g:sqlutil_menu_priority : ''
        elseif g:sqlutil_default_menu_mode == 3
            let menuRoot     = exists("g:sqlutil_menu_root") ? g:sqlutil_menu_root : '&Plugin.&SQLUtil'
            let menuPriority = exists("g:sqlutil_menu_priority") ? g:sqlutil_menu_priority : ''
        else
            let menuRoot     = '&Plugin.&SQLUtil'
            let menuPriority = exists("g:sqlutil_menu_priority") ? g:sqlutil_menu_priority : ''
        endif

        let leader = '\'
        if exists('g:mapleader')
            let leader = g:mapleader
        endif
        let leader = escape(leader, '\')

        if s:sqlutil_menus_created == 0
            exec 'vnoremenu <script> '.menuPriority.' '.menuRoot.'.Format\ Range\ Stmts<TAB>'.leader.'sfr :SQLUFormatStmts<CR>'
            exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Format\ Range\ Stmts<TAB>'.leader.'sfr :SQLUFormatStmts<CR>'
            exec 'vnoremenu <script> '.menuPriority.' '.menuRoot.'.Format\ Statement<TAB>'.leader.'sfs :SQLUFormatter<CR>'
            exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Format\ Statement<TAB>'.leader.'sfs :SQLUFormatter<CR>'
            exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Create\ Procedure<TAB>'.leader.'scp :SQLUCreateProcedure<CR>'
            exec 'inoremenu <script> '.menuPriority.' '.menuRoot.'.Create\ Procedure<TAB>'.leader.'scp
                        \ <C-O>:SQLUCreateProcedure<CR>'
            exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Create\ Column\ List<TAB>'.leader.'sl
                        \ :SQLUCreateColumnList<CR>'
            exec 'inoremenu <script> '.menuPriority.' '.menuRoot.'.Create\ Column\ List<TAB>'.leader.'sl
                        \ <C-O>:SQLUCreateColumnList<CR>'
            exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Column\ Definition<TAB>'.leader.'scd
                        \ :SQLUGetColumnDef<CR>'
            exec 'inoremenu <script> '.menuPriority.' '.menuRoot.'.Column\ Definition<TAB>'.leader.'scd
                        \ <C-O>:SQLUGetColumnDef<CR>'
            exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Column\ Datatype<TAB>'.leader.'scdt
                        \ :SQLUGetColumnDataType<CR>'
            exec 'inoremenu <script> '.menuPriority.' '.menuRoot.'.Column\ Datatype<TAB>'.leader.'scdt
                        \ <C-O>:SQLUGetColumnDataType<CR>'

            let s:sqlutil_menus_created = 1
        endif
        silent! exec 'aunmenu  <script> '.menuPriority.' '.menuRoot.'.Toggle\ Align\ Where'
        exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Toggle\ Align\ Where'.
                    \ (g:sqlutil_align_where==1?'<TAB>(on) ':'<TAB>(off) ').
                    \ ':SQLUToggleValue g:sqlutil_align_where<CR>'
        silent! exec 'aunmenu '.menuPriority.' '.menuRoot.'.Toggle\ Align\ Comma'
        exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Toggle\ Align\ Comma'.
                    \ (g:sqlutil_align_comma==1?'<TAB>(on) ':'<TAB>(off) ').
                    \ ':SQLUToggleValue g:sqlutil_align_comma<CR>'
        silent! exec 'aunmenu '.menuPriority.' '.menuRoot.'.Toggle\ Align\ First\ Word'
        exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Toggle\ Align\ First\ Word'.
                    \ (g:sqlutil_align_first_word==1?'<TAB>(on) ':'<TAB>(off) ').
                    \ ':SQLUToggleValue g:sqlutil_align_first_word<CR>'
        silent! exec 'aunmenu '.menuPriority.' '.menuRoot.'.Toggle\ Right\ Align\ Keywords'
        exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Toggle\ Right\ Align\ Keywords'.
                    \ (g:sqlutil_align_keyword_right=='1'?'<TAB>(on) ':'<TAB>(off) ').
                    \ ':SQLUToggleValue g:sqlutil_align_keyword_right<CR>'
        silent! exec 'aunmenu '.menuPriority.' '.menuRoot.'.Uppercase\ Keywords'
        exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Uppercase\ Keywords'.
                    \ (g:sqlutil_keyword_case=='\U'?'<TAB>(on) ':' ').
                    \ ':SQLUToggleValue g:sqlutil_keyword_case \U<CR>'
        silent! exec 'aunmenu '.menuPriority.' '.menuRoot.'.Lowercase\ Keywords'
        exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Lowercase\ Keywords'.
                    \ (g:sqlutil_keyword_case=='\L'?'<TAB>(on) ':' ').
                    \ ':SQLUToggleValue g:sqlutil_keyword_case \L<CR>'
        silent! exec 'aunmenu '.menuPriority.' '.menuRoot.'.Default\ Case\ Keywords'
        exec 'noremenu  <script> '.menuPriority.' '.menuRoot.'.Default\ Case\ Keywords'.
                    \ (g:sqlutil_keyword_case==''?'<TAB>(on) ':' ').
                    \ ':SQLUToggleValue g:sqlutil_keyword_case default<CR>'
    endif
endfunction

" Puts a command separate list of columns given a table name
" Will search through the file looking for the create table command
" It assumes that each column is on a separate line
" It places the column list in unnamed buffer
function! SQLU_CreateColumnList(...)
    if(a:0 > 1)
        call SQLUtilities#SQLU_CreateColumnList(a:1, a:2)
    elseif(a:0 > 0)
        call SQLUtilities#SQLU_CreateColumnList(a:1)
    else
        call SQLUtilities#SQLU_CreateColumnList()
    endif
endfunction


" Strip the datatype from a column definition line
function! SQLU_GetColumnDatatype( line, need_type )
    return SQLUtilities#SQLU_GetColumnDatatype(a:line, a:need_type)
endfunction


" Puts a comma separated list of columns given a table name
" Will search through the file looking for the create table command
" It assumes that each column is on a separate line
" It places the column list in unnamed buffer
function! SQLU_GetColumnDef( ... )
    if(a:0 > 1)
        return SQLUtilities#SQLU_GetColumnDef(a:1, a:2)
    elseif(a:0 > 0)
        return SQLUtilities#SQLU_GetColumnDef(a:1)
    else
        return SQLUtilities#SQLU_GetColumnDef()
    endif
endfunction



" Creates a procedure defintion into the unnamed buffer for the
" table that the cursor is currently under.
function! SQLU_CreateProcedure(...)
    if(a:0 > 0)
        return SQLUtilities#SQLU_CreateProcedure(a:1)
    else
        return SQLUtilities#SQLU_CreateProcedure()
    endif
endfunction



" Compares two strings, and will remove all names from the first
" parameter, if the same name exists in the second column name.
" The 2 parameters take comma separated lists
function! SQLU_RemoveMatchingColumns( full_col_list, dup_col_list )
    return SQLUtilities#SQLU_RemoveMatchingColumns( a:full_col_list, a:dup_col_list )
endfunction



" Toggles the value of some configuration parameters.
" Mainly used by the menu.
function! SQLU_ToggleValue( ... )
    if (a:0 == 0)
        echohl WarningMsg
        echomsg "SQLUToggle value requires at least 1 parameter"
        echohl None
    elseif (a:0 == 1)
        if exists('{a:1}')
            let {a:1} = (({a:1} == 0)?1:0)
        endif
    else
        if exists('{a:1}')
            " Use defaults as the default for this function
            if a:2 == 'default'
                let {a:1} = ''
            else
                let {a:1} = a:2
            endif
        endif
    endif
    call SQLU_Menu()
endfunction

call SQLU_Menu()
" vim:fdm=marker:nowrap:ts=4:expandtab:ff=unix:
