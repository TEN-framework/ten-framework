#script (python)

import pandas as pd
from collections import OrderedDict

def main(prg):
    prg.ground([("base", [])])
    with prg.solve(yield_=True) as h:
        for m in h:
            writer = pd.ExcelWriter('excel.xlsx', engine='xlsxwriter')
            table = {}
            for s in m.symbols(atoms=True):
                i = 0
                sheet = table.setdefault("{}_{}".format(s.name, len(s.arguments)), OrderedDict())
                for a in s.arguments:
                    sheet.setdefault("Arg {}".format(i), []).append(str(a))
                    i+= 1
            for name, sheet in table.items():
                df = pd.DataFrame(sheet)
                df.to_excel(writer, sheet_name=name)
            writer.save()
            break

#end.
