#script (python)

from clingo import Control, String, Tuple_

messages = []

def log(code, message):
    messages.append(Tuple_((String(str(code).replace('MessageCode.', '')),
                            String(message.replace(" ", "_")))))

def main(prg):
    ctl = Control(logger=log, message_limit=2)
    ctl.add("base", [], ":- a. :- b. :- c.")
    ctl.ground([("base", [])])

    prg.ground([("base", [])])
    prg.solve()

def msg():
    return messages

#end.

msg(X,Y) :- (X, Y) = @msg().
