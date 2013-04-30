" put this file to $HOME/.vim
if exists("have_load_filetypes")
      finish
endif
augroup filetypedetect
      au! BufRead,BufNewFile bacula-dir.conf	setfiletype bacula
augroup END
