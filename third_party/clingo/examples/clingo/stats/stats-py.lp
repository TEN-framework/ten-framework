#script (python)

import json

def main(prg):
    prg.ground([("base", [])])
    prg.solve()
    print (json.dumps(prg.statistics, sort_keys=True, indent=4, separators=(',', ': ')))
    print ("lp.bodies", prg.statistics["problem"]["lp"]["bodies"])
    print ("optimization", prg.statistics["summary"]["costs"])

#end.


