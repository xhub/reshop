#!/usr/bin/env python

import itertools
import json
import sys
from typing import NamedTuple


class PyObj(NamedTuple):
    cname: str
    fn_prefixes: tuple
    fn_ignore: tuple

class FnDecl(NamedTuple):
    ret_type:   str
    name: str
    args: str
    args_nameonly: str
    cret_type: str
    cname: str
    cargs: str

pyObjs = {
    "Model": PyObj("struct rhp_mdl", ("rhp_mdl_", "rhp_"), ("rhp_mdl_free","rhp_mdl_getmpforvar","rhp_mdl_getmpforequ")),
#    "Avar": PyObj("struct rhp_avar"),
#    "Aequ": PyObj("struct rhp_aequ"),
    "MathPrgm": PyObj("struct rhp_mathprgm", ("rhp_mp_",), ("rhp_mp_free",)),
    "NashEquilibrium": PyObj("struct rhp_nash_equilibrium", ("rhp_mpe_", "rhp_nash_"), ("rhp_mpe_free",)),
}

pyMethodsRc = dict(zip(pyObjs.keys(), tuple([] for _ in range(len(pyObjs.keys())))))

pyMethodsRet = dict(zip(pyObjs.keys(), tuple([] for _ in range(len(pyObjs.keys())))))

# print is a python keyword
# solve is manually defined
skips = ("print", "solve")

rename_ret = {"rhp_mathprgm_getobjequ": "rhp_idx", "rhp_mathprgm_getobjequ": "rhp_idx"}
rhp_idx_names = ("ei", "vi", "objequ", "objvar")

with open(sys.argv[1], 'r') as f:
     ast_data = json.load(f)


for pyObj, v in pyObjs.items():
    cname = v.cname
    fn_prefixes = v.fn_prefixes
    methods_rc = pyMethodsRc[pyObj]
    methods_ret = pyMethodsRet[pyObj]
    fn_ignore = v.fn_ignore
    for decl in ast_data['inner']:
        if decl['kind'] != 'FunctionDecl': continue

        inner = decl.get("inner")
        if not inner: continue

        name = decl["name"]

        if name in fn_ignore: continue
        fnargs = tuple(e for e in inner if e["kind"] == "ParmVarDecl")

        if not len(fnargs): continue

        first_arg = fnargs[0]

        if not cname in first_arg['type']['qualType']: continue

        # rhp_idx is useful for typemaps, reinject it
        fnargs_post = []
        for arg in fnargs:
            if arg['name'] in rhp_idx_names:
                arg['type']['qualType'] = arg['type']['qualType'].replace("int", "rhp_idx")

            fnargs_post.append(arg)

        fnargs = fnargs_post

        args = (f"{e['type']['qualType']} {e['name']}" for e in fnargs[1:])


        method_name = name
        for prefix in fn_prefixes:
            method_name = method_name.replace(prefix, "")

        if method_name in skips: continue

        method_args = ", ".join(args)
        methods_argsnameonly = ", ".join(f"{e['name']}" for e in fnargs[1:])

        ret_type = decl['type']['qualType'].split('(')[0].strip()

        # reinject rhp_idx
        ret_type:str = rename_ret.get(name, ret_type)

        if ret_type == "int":
            methods_rc.append(FnDecl(ret_type, method_name, method_args, methods_argsnameonly, ret_type, name, method_args))
        else:
            methods_ret.append(FnDecl(ret_type, method_name, method_args, methods_argsnameonly, ret_type, name, method_args))


need_defines = []
for m in itertools.chain(pyMethodsRc["Model"], pyMethodsRet["Model"]):
    if "rhp_mdl_" not in m.cname:
        need_defines.append(f"#define {m.cname.replace("rhp_", "rhp_mdl_")} {m.cname}")

for m in itertools.chain(pyMethodsRc["MathPrgm"], pyMethodsRet["MathPrgm"]):
    need_defines.append(f"#define {m.cname.replace("rhp_mp_", "rhp_mathprgm_")} {m.cname}")

for m in itertools.chain(pyMethodsRc["NashEquilibrium"], pyMethodsRet["NashEquilibrium"]):
    need_defines.append(f"#define {m.cname.replace("rhp_mpe_", "rhp_nash_equilibrium_")} {m.cname}")

with open("pyobj_methods.i", "w") as meth_file:

    for pyObj, v in pyObjs.items():
        meth_file.write(f"%extend {v.cname} {{\n")

        for m in pyMethodsRc[pyObj]:
            if m.cargs:
                cargs = ", " + m.cargs
                margs = m.cargs
                margs_nameonly = ", " + m.args_nameonly
            else:
                cargs = ""
                margs = "void"
                margs_nameonly = ""

            if m.cname.endswith("named"):
                # rename last argument (name)
                fnname = m.name.replace("_named", "").replace("named", "")
                meth_file.write(f"\t{m.ret_type} {fnname}({margs}) {{ return {m.cname} (self{margs_nameonly}); }};\n")
            else:
                meth_file.write(f"\t{m.ret_type} {m.name}({margs});\n")

        for m in pyMethodsRet[pyObj]:
            if m.cargs:
                cargs = ", " + m.cargs
                margs = m.cargs
            else:
                cargs = ""
                margs = "void"
            meth_file.write(f"\t{m.ret_type} {m.name}({margs});\n")

        meth_file.write("}\n")


    if not need_defines: sys.exit(0)

    meth_file.write("%header %{\n")
    for d in need_defines:
        meth_file.write(d + "\n")

    meth_file.write("%}\n")


with open("rename_named.i", "w") as ren_file:
    for m in itertools.chain((v for v in itertools.chain(*pyMethodsRc.values(), *pyMethodsRet.values()))):
        if m.cname.endswith("named"):
            ren_file.write(f"%rename({m.cname.replace("_named", "").replace("named", "").replace("rhp_","")}) {m.cname};\n")


# named versions should not be created with named


