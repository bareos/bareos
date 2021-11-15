s#@db_name@#bareos#g
s#@dir_port@#9101#g
s#@fd_port@#9102#g
s#@sd_port@#9103#g
s|@job_email@|root@localhost|g
s|@smtp_host@|localhost|g
s|@uncomment_dbi@|#|g
s|mail = @job_email@|# mail = @job_email@|g
s|mailcommand =|# mailcommand =|g
s|operator = @job_email@|# operator = @job_email@|g
s|operatorcommand =|# operatorcommand =|g
