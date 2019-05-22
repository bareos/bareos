Send the message to the email addresses that are given as a comma separated list in the address field. Mail messages are grouped together during a job and then sent as a single email message when the job terminates. The advantage of this destination is that you are notified about every Job that runs. However, if you backup mutliple machines every night, the number of email messages can be annoying. Some users use filter programs such as :command:`procmail` to automatically file this
email based on the Job termination code (see :config:option:`dir/messages/MailCommand`\ ).

