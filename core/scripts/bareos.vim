" Vim syntax file
" Put this file to your $HOME/.vim/syntax/ and use :syntax on
" Language:         Bareos
" Maintainer:       Eric Bollengier <eric@eb.homelinux.org>
" URL:
" Latest Revision:  2007-02-11


if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif


" comments
syn region      BacComment  display oneline start="#" end="$" keepend contains=BacTodo
syn region	BacComment2 start="/\*"  end="\*/"

syn region      BacInclude start=/^@/ end="$"

syntax region xCond start=/\w+\s*{/ms=e+1 end=/}/me=s-1
syntax keyword BacName Name
syn case ignore

syn keyword  LevelElt  contained Full Incremental Differential

" todo
syn keyword     BacTodo       contained TODO FIXME XXX NOTE
syn region 	BacString     start=/"/ skip=/\\"/ end=/"/

" Specifique Client {
syn region    BacClient display start=/Client {/ end="^}"  contains=BacString,BacComment,BacC1,BacC2,BacC3,BacC4
syn match     BacC1     contained /File\s*Retention/
syn match     BacC2     contained /Maximum\s*Concurrent\s*Jobs/
syn match     BacC3     contained /Job\s*Retention/
syn keyword   BacC4     contained Name Password Address Catalog AutoPrune FDPort

" FileSet {
syn region    BacFileSet display start="FileSet {" end="^}" contains=BacString,BacComment,BacName,BacFSInc,BacFSExc,BacFS2
syn region    BacFSInc   contained display start="Include {" end="}" contains=BacString,BacComment,BacFSOpt,BacFS1
syn region    BacFSExc   contained display start="Exclude {" end="}" contains=BacString,BacComment,BacFSOpt,BacFS1
syn region    BacFSOpt   contained display  start="Options {" end="}" contains=BacString,BacComment,BacFSOpt1,BacFSOpt2
syn keyword   BacFSOpt1  contained verify signature onefs noatime RegexFile Exclude Wild WildDir WildFile CheckChanges aclsupport
syn match     BacFSOpt2  contained /ignore case/
syn keyword   BacFS1     contained File
syn match     BacFS2     contained /Enable VSS/

" Storage {
syn region   BacSto     display start="Storage {" end="}" contains=BacName,BacComment,BacString,BacSto1,BacSto2
syn keyword  BacSto1	contained Address SDPort Password Device Autochanger
syn match    BacSto2    contained /Media\s*Type/

" Director {
syn region   BacDir     display start="Director {" end="}" contains=BacName,BacComment,BacString,BacDir,BacDir1,BacDir2
syn keyword  BacDir1    contained DIRport QueryFile WorkingDirectory PidDirectory Password Messages
syn match    BacDir2    contained /Maximum\s*Concurrent\s*Jobs/

" Catalog {
syn region   BacCat     display start="Catalog {" end="}" contains=BacName,BacComment,BacString,BacCat1
syn keyword  BacCat1	contained dbname user password dbport

" Job {
syn region   BacJob     display start="Job {" end="^}"     contains=BacJ1,BacJ2,BacString,BacComment,Level,BacC2,BacJ3,BacRun
syn region   BacJobDefs display start="JobDefs {" end="^}" contains=BacJ1,BacJ2,BacString,BacComment,Level,BacC2,BacJ3
syn region   Level      display start="Level =" end="$"    contains=LevelElt

syn keyword  BacJ1      contained Schedule Name Priority Client Pool JobDefs FileSet SpoolData Storage where
syn keyword  BacJ2      contained RunBeforeJob RunAfterJob Type Messages ClientRunAfterJob
syn match    BacJ3      contained /Write Bootstrap/


" RunScript {
syn region   BacRun    contained display start="RunScript {" end="}"  contains=BacR1,BacR2,BacR3,BacR4,BacRW,BacString,BacComment
syn match    BacR1     contained /Runs\s*When/
syn match    BacR2     contained /Runs\s*On\s*Client/
syn match    BacR3     contained /Runs\s*On\s*Failure/
syn keyword  BacR4     contained Command
syn keyword  BacRW     contained After Before Always

" Schedule {
syn region   BacSched     display start="Schedule {" end="^}" contains=BacSR,BacString,BacComment,BacName,BacRun
syn keyword  BacS1	  contained Pool FullPool on at
syn keyword  BacS2        contained sun mon tue wed thu fri sat sunday monday tuesday wednesday thursday friday saturday
syn keyword  BacS3        contained jan  feb  mar  apr  may  jun  jul  aug  sep  oct  nov  dec
syn keyword  BacS4        contained 1st 2nd  3rd  4th  5th  first second  third  fourth  fifth
syn region   BacSR        contained display start="Run = " end="$"  contains=BacS1,BacS2,BacS3,BacS4,LevelElt

syn keyword  BacSpecial   false  true yes no

" Pool
syn region   BacPool      display start="Pool {" end="^}"     contains=BacP1,BacP2,BacP3,BacString,BacComment
syn match    BacP1        contained /Pool\s*Type/
syn match    BacP2        contained /Volume\s*Retention/
syn keyword  BacP3        contained Name AutoPrune Recycle

syn case match
if version >= 508 || !exists("did_screen_syn_inits")
  if version < 508
    let did_screen_syn_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

" Define the default highlighting.

HiLink BacFileSet   Function
HiLink BacFSInc     Function
HiLink BacFSExc     Function
HiLink BacFSOpt     Function
HiLink BacFSOpt1    Keyword
HiLink BacFSOpt2    Keyword
HiLink BacFS1       Keyword
HiLink BacFS2       Keyword

HiLink BacInclude   Include
HiLink BacComment   Comment
HiLink BacComment2  Comment
HiLink BacTodo      Todo
HiLink LevelElt     String
HiLink BacRun       Function

HiLink BacCat       Function
HiLink BacCat1      Keyword

HiLink BacSto       Function
HiLink BacSto1      Keyword
HiLink BacSto2      Keyword

HiLink BacDir       Function
HiLink BacDir1      keyword
HiLink BacDir2      keyword

HiLink BacJob	    Function
HiLink BacJobDefs   Function
HiLink BacJ1        Keyword
HiLink BacJ2        Keyword
HiLink BacJ3        Keyword

HiLink BacClient    Function
HiLink BacC1	    Keyword
HiLink BacC2	    Keyword
HiLink BacC3	    Keyword
HiLink BacC4	    Keyword
HiLink Level        Keyword

HiLink BacSched     Function
HiLink BacS1        Keyword
HiLink BacS2        String
HiLink BacS3        String
HiLink BacS4        String

HiLink BacR1        Keyword
HiLink BacR2        Keyword
HiLink BacR3        Keyword
HiLink BacR4        Keyword
HiLink BacRW        String

HiLink BacPool      Function
HiLink BacP1        Keyword
HiLink BacP2        Keyword
HiLink BacP3        Keyword

HiLink BacName      Keyword
HiLink BacString    String
HiLink BacNumber    Number
HiLink BacCommand   BacCommands
HiLink BacCommands  Keyword
HiLink BacSpecial   Boolean
HiLink BacKey       Function
HiLink Equal        Comment
delcommand HiLink

endif
