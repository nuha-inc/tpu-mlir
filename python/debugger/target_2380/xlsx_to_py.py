# ==============================================================================
#
# Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
#
# TPU-MLIR is licensed under the 2-Clause BSD License except for the
# third-party components.
#
# ==============================================================================

import math
from time import gmtime, strftime
import re
import numpy as np
import pandas
import numpy
from jinja2 import Template

match_illegal = re.compile("[（）& /()]")

ctype_template_str = """
class {{class_name}}_reg(atomic_reg):
    OP_NAME = "{{op_name}}"
    _fields_ = [{% for field, field_length in fields %}
        ("{{field}}", ctypes.c_uint64, {{field_length}}),
        {%- endfor %}
    ]
    {% for key in valid_key %}
    {{key}}: int
    {%- endfor %}

    length: int = {{length}}

    {% for raw, valid in invalid_key %}
    @property
    def {{valid}}(self) -> int:
        return self["{{raw}}"]
    {%- endfor %}
"""


def pd_to_dict(df):
    # filter out invalid recorder
    valid = ~df.iloc[:, 1].isnull()
    df = df[valid].copy()
    fields = list(df.iloc[:, 0])
    fields = [i.replace("des_", "") if isinstance(i, str) else str(i) for i in fields]
    return list(zip(fields, numpy.cumsum(df.iloc[:, 1].astype(int))))


tiu_reg_a2 = "./TPU_TIU_Reg4.6.xlsx"
dma_reg_a2 = "./GDMA_SG2380_DES_REG.xlsx"

bdc_sheet_name = [
    "CONV",
    "sCONV",
    "MM",
    "sMM",
    "MM2",
    "sMM2",
    "CMP",
    "sCMP",
    "SFU",
    "sSFU",
    "VC",
    "sVC",
    "LIN",
    "sLIN",
    "AR",
    "sAR",
    "PorD",
    "sPorD",
    "RQ&DQ",
    "sRQ&sDQ",
    "SG",
    "sSG",
    "SGL",
    "sSGL",
    "CW&BC",
    "sCW&sBC",
    "LAR",
    "SYS",
    "SYSID",
    "SYS_TR_ACC",
]

dma_sheet_name = [
    "DMA_tensor（0x000）",
    "DMA_matrix",
    "DMA_masked_select",
    "DMA_general",
    "DMA_cw_transpose",
    "DMA_nonzero",
    "sDMA_sys",
    "DMA_gather",
    "DMA_scatter",
    "DMA_reverse",
    "DMA_compress",
    "DMA_decompress ",
    "DMA_lossy_compress",
    "DMA_lossy_decompress",
    "DMA_randmask",
    "DMA_tansfer",
]


reg_bdc = pandas.read_excel(tiu_reg_a2, sheet_name=bdc_sheet_name)
reg_dma = pandas.read_excel(dma_reg_a2, sheet_name=dma_sheet_name)

cmd_reg = {}
for k, df in reg_bdc.items():
    cmd_reg[k] = pd_to_dict(df)

for k, df in reg_dma.items():
    cmd_reg[k] = pd_to_dict(df)

file_head = f"""# ==============================================================================
#
# Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
#
# TPU-MLIR is licensed under the 2-Clause BSD License except for the
# third-party components.
#
# ==============================================================================
#
# automatically generated by {__file__}
# time: {strftime('%Y-%m-%d %H:%M:%S', gmtime())}
# this file should not be changed except format.
# tiu_reg_fn: {tiu_reg_a2}
# dma_reg_fn: {dma_reg_a2}

from typing import Dict, Type
import ctypes
from ..target_common import atomic_reg

"""


tail_template_str = """

op_class_dic: Dict[str, Type[atomic_reg]] = {
    {% for cmd_type, class_name in cmd %}
    "{{cmd_type}}": {{class_name}}_reg,
    {%- endfor %}
}
"""

# old_cmd_reg=cmd_reg
# for key in cmd_reg.keys():
#     cmd_reg_def = cmd_reg[key]

#     field_keys, high_bits = zip(*cmd_reg_def)
#     if not all(64 * x in high_bits for x in range(1, high_bits[-1] // 64 + 1)):
#         if key in {"SYS"}:
#             field_keys = list(field_keys)[:10]
#             high_bits = list(high_bits)[:10]
#         elif key in {"SYSID", "SYS_TR_ACC"}:
#             field_keys = list(field_keys)[:8]
#             high_bits = list(high_bits)[:8]
#         print(key)

#     assert all(64 * x in high_bits for x in range(1, high_bits[-1] // 64 + 1)), key
#     cmd_reg[key] = list(zip(field_keys, high_bits))
# if cmd_reg==old_cmd_reg:
#     print("ALL the same")

with open("regdef.py", "w") as fb:
    fb.write(file_head)

    # fb.write("reg_def_obj = ")
    # fb.write(pprint.pformat(cmd_reg, width=80))
    ctype_template = Template(ctype_template_str)
    file_end_template = Template(tail_template_str)

    ctype_py_str = []

    cmds = []
    for key in cmd_reg:
        cmd_reg_def = cmd_reg[key]

        field_keys, high_bits = zip(*cmd_reg_def)
        if not all(
            64 * x in high_bits for x in range(1, math.ceil(high_bits[-1] / 64))
        ):
            if key in {"SYS"}:
                field_keys = list(field_keys)[:11]
                high_bits = list(high_bits)[:11]
            elif key in {"SYSID", "SYS_TR_ACC"}:
                field_keys = list(field_keys)[:8]
                high_bits = list(high_bits)[:8]
            print(key)

        if key != "sLIN":
            assert all(
                64 * x in high_bits for x in range(1, math.ceil(high_bits[-1] / 64))
            )

        bits_width = np.diff(high_bits, prepend=0)
        fields = [(k, l) for k, l in zip(field_keys, bits_width)]

        valid_key = [match_illegal.sub("_", key) for key in field_keys]
        invalid_key = [
            (key, match_illegal.sub("_", key))
            for key in field_keys
            if match_illegal.search(key)
        ]

        # print(match_illegal.sub("_", key))
        ctype_py_str.append(
            ctype_template.render(
                op_name=key,
                class_name=match_illegal.sub("_", key),
                fields=fields,
                valid_key=valid_key,
                invalid_key=invalid_key,
                length=high_bits[-1],
            )
        )
        cmds.append((key, match_illegal.sub("_", key)))

    fb.write(("\n").join(ctype_py_str))
    fb.write(file_end_template.render(cmd=cmds))
