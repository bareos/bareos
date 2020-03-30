debuglevel = 10


def debugmessage(level, message):
    if level <= debuglevel:
        print(message)
