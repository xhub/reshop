import csv
from io import TextIOWrapper, TextIOBase, StringIO
from lxml import etree
import os
from pathlib import Path
import re
import sys
from typing import NamedTuple, Union

doxydir = sys.argv[1]

class FnArg(NamedTuple):
    name: str
    type: str
    direction: str
    descr: str

class FnRet(NamedTuple):
    name: str
    type: str
    descr: str

class FnDecl(NamedTuple):
    name: str
    brief_desc: str
    detailed_desc: str
    ret: list[FnRet]
    args: list[FnArg]
    extra: dict

reshop_argname2ret = ('eout', 'vout', 'mdlout', 'dval', 'ival', 'eidx', 'vidx', 'minf', 'pinf', 'nan')
reshop_argout_simple = (
    ('int *',       'ei'),
    ('int *',       'eidx'),
    ('int **',      'eis'),
    ('int *',       'objequ'),
    ('int *',       'objvar'),
    ('int *',       'vi'),
    ('int *',       'vidx'),
    ('int **',      'vis'),
    ('rhp_idx *',   'ei'),
    ('rhp_idx *',   'eidx'),
    ('rhp_idx **',  'eis'),
    ('rhp_idx *',   'objequ'),
    ('rhp_idx *',   'objvar'),
    ('rhp_idx *',   'vi'),
    ('rhp_idx *',   'vidx'),
    ('rhp_idx **',  'vis'),
    ('double *',    'dual'),
    ('double *',    'edual'),
    ('double *',    'vdual'),
    ('double *',    'duals'),
    ('double *',    'level'),
    ('double *',    'elevel'),
    ('double *',    'vlevel'),
    ('double *',    'levels'),
    ('double *',    'rhs'),
    ('double *',    'cst'),
    ('double *',    'val'),
    ('double *',    'vals'),
    ('double *',    'lb'),
    ('double *',    'ub'),
    ('unsigned *',  'cone'),
    ('unsigned *',  'type'),
    ('unsigned *',  'sense'),
    ('unsigned **', 'grps'),
    ('int *',       'basis_info'),
    ('int *',       'ebasis_info'),
    ('int *',       'vbasis_info'),
    ('int *',       'basis_infos'),
    ('int *',       'modelstat'),
    ('int *',       'solvestat'),
    ('NlNode ***',  'root'),
    ('NlNode ***',  'child'),
    ('OvfDef **',   'ovf_def'),
    ('char **',     'solvername'),
)

reshop_argout_multiple = {
    ('unsigned *', 'len'): ({
        "args": (('rhp_idx **', 'idxs'), ('double **', 'vals')),
        "ret": FnRet(type="LinearEquation", name="", descr="The linear equation")},),
}

types_c2swig = {
    'MathPrgm *': 'MathPrgm',
    'Model *': 'Model',
    'Nash *': 'Nash',
    'struct rhp_nash_equilibrium *': 'Nash',
    'OvfDef *': 'OvfDef',
    'ArcVFData *': 'ArcVFData',
    'struct rhp_mdl *':  'Model',
    'struct rhp_aequ *': 'Equs',
    'struct rhp_avar *': 'Vars',
    'Avar *': 'Vars',
    'Aequ *': 'Equs',
    'SpMat *': 'scipy matrix',
    'bool': 'Bool',
    'char *': 'str',
    'const char *': 'str',
    'short': 'int',
    'unsigned': 'int',
    'size_t': 'int',
    'double': 'float',
    'double *': 'array-like of float',
    'int *': 'array-like of int',
    'NlTree *': 'NlTree',   #Do a better job here
    'LinearEquation': 'LinearEquation',
    'enum rhp_backendtype': 'RhpBackendType',
}

argout_c2swig = {
    'MathPrgm *': 'MathPrgm',
    'Model *': 'Model',
    'Model **': 'Model',
    'Nash *': 'Nash',
    'struct rhp_mdl *':  'Model',
    'struct rhp_aequ *': 'Equs',
    'struct rhp_avar *': 'Vars',
    'Avar *': 'Vars',
    'Aequ *': 'Equs',
    'ArcVFData *': 'ArcVFData',
    'OvfDef **': 'OvfDef',
    'char **': 'str',
    'unsigned *': 'int',
    'unsigned **': 'array of int',
    'double *': 'float',
    'int *': 'BasisStatus',
    'int **': 'array of int',
    'rhp_idx **': 'array of int',
    'NlNode ***': 'NlNode **',   #Do a better job here
    'NlTree *': 'NlTree',   #Do a better job here
}

reshop_argstypechange = (
    {"name": "opt", "ctype": "unsigned char", "swigtype": "Bool"},
    {"name": "bval", "ctype": "unsigned char", "swigtype": "Bool"},
)

reshop_args2nparraySzArr=(
   ('n_val', 'vals'),
   ('size', 'vis'),
   ('size', 'eis'),
   ('size', 'list'),
)

reshop_args2nparrayArr=('lbs', 'ubs', 'level', 'dual', 'level', 'dual')

# TODO: delete?
reshop_ctype2swig={
    'struct rhp_mdl': 'Model',
    'struct rhp_avar': 'Avar',
    'struct rhp_aequ': 'Aequ',
    'struct rhp_mathprgm': 'MathPrgm',
    'struct rhp_nash_equilibrium': 'NashEquilibrium',
}

reshop_ignore = ['rhp_empdag_writeDOT']

equ_addquad = FnDecl(name="equ_addquad", brief_desc="Add a quadratic term to an equation", detailed_desc="", ret=[], args=[], extra={})

reshop_merging = (
    (equ_addquad, ("rhp_equ_addquadrelative", "rhp_equ_addquadabsolute")),
)

def fmt_descr(s: str) -> str:
    if not s:
        return ""

    words = s.strip().split()
    if words:
        words[0] = words[0].capitalize()

        s = " ".join(words)
    if s[-1] != '.':
        return s + '.'
    else:
        return s

def fmt_pyfn(fn: FnDecl) -> str:
    args = fn.args
    args_optional = fn.extra.get('args_optional', ())

    args_str = ", ".join((e.name for e in args))

    if args_optional:
        args_optional_str = ", " + ", ".join((e.name + "=None" for e in args_optional))
    else:
        args_optional_str = ""

    all_args_str = f"{args_str}{args_optional_str}"
    if all_args_str:
        all_args_str += ", /"

    return f"{fn.name}({all_args_str})"


def fmt_as_numpydoc(fn: FnDecl, io: TextIOBase):
    """Outputs the function declaration in the numpy doc format"""
    assert type(fn.brief_desc) == str, f"wrong brief description in {fn}"
    rets = fn.ret
    args = fn.args
    args_optional = fn.extra.get('args_optional', ())

    io.write(fmt_descr(fn.brief_desc)+"\n")

    if fn.detailed_desc:
        io.write("\n"+fn.detailed_desc+"\n")

    if len(args) + len(args_optional):
        io.writelines(rstHeading("Parameters"))

    for arg in args:
        io.write(f"{arg.name} : {arg.type}\n")
        io.write(f"    {fmt_descr(arg.descr)}\n")

    for arg in args_optional:
        io.write(f"{arg.name} : {arg.type}, optional\n")
        io.write(f"    {fmt_descr(arg.descr)}\n")

    if len(rets):
        io.writelines(rstHeading("Returns"))

    for arg in rets:
        if arg.name:
            io.write(f"{arg.name} : {arg.type}\n")
        else:
            io.write(arg.type + "\n")

        io.write(f"    {fmt_descr(arg.descr)}\n")

    if "note" in fn.extra.keys():
        io.write(fn.extra["note"]+"\n")

def fnName_c2swig(name: str) -> str:
    return name.replace("rhp_", "")

def fnDecl_combine(fns: list[FnDecl], fnout: FnDecl) -> FnDecl:
    notes = StringIO("")

    cnt = 1

    for fn in fns:
        pyargs = ",".join(a.name for a in fn.args)
        notes.write(f"**Signature {cnt}: `{fnout.name}({pyargs})`**\n")
        fmt_as_numpydoc(fn, notes)

    if "note" not in fnout.extra.keys(): fnout.extra["note"] = ""

    fnout.extra["note"] += notes.getvalue()

    return fnout

swig_type_drop = ('const', 'restrict')
pattern_keep_star = r"(\s*[*]\s*)|\s"
def type_c2swig(typ: str) -> str:
    typ_ = " ".join(e.strip() for e in re.split(pattern_keep_star, typ.strip()) if (e and e.strip() and e.strip() not in swig_type_drop))
    typ_, cnt = re.subn(r'\* \*', '**', typ_)
    while cnt > 0:
        typ_, cnt = re.subn(r'\* \*', '**', typ_)

    assert "const" not in typ_
    return typ_

def fnArg_c2swig(arg: FnArg) -> FnArg:
    assert arg.direction != "out", f"wrong direction in argument {arg}"
    name = arg.name.strip()
    typ = type_c2swig(arg.type)
    dir = arg.direction
    descr = arg.descr.strip()

    if typ == 'rhp_idx' or typ == 'int':
        if name == 'vi' or name == 'objvar':
            typ = "VariableRef or int"
        elif name == 'ei' or name == 'objequ':
            typ = "EquationRef or int"
        elif name in ('basis_info', 'ebasis_info', 'vbasis_info'): 
            typ = "BasisStatus or int"
        else:
            assert (f"ERROR: Unhandled rhp_idx argument '{name}' in {arg}")

    elif typ == 'rhp_idx *' or typ == 'int *':
        if name == 'vis':
            typ = "array-like of VariableRef or int"
        elif name == 'eis':
            typ = "array-like of EquationRef or int"
        elif name in ('colidx', 'rowidx'):
            typ = "array of indices"
        else:
            print(f"ERROR: Unhandled rhp_idx * argument '{name}' in {arg}")
    else:
        typ = types_c2swig.get(typ, typ)

    return FnArg(name=name, type=typ, direction=dir, descr=descr)


def fnArgOut_c2swig(arg: FnArg) -> FnRet:
    assert arg.direction == "out", f"wrong direction in {arg}"
    name = arg.name.strip()
    descr = arg.descr.strip()
    typ = type_c2swig(arg.type)

    if typ == 'rhp_idx *' or typ == 'int *': 
        if name == 'vi' or name == 'objvar':
            typ = "VariableRef"
        elif name == 'vidx' or name == 'eidx':   # This is for rhp_a(var|equ)_get
            typ = "int"
        elif name == 'modelstat':
            typ = "ModelStatus"
        elif name in ('basis_infos', 'ebasis_info', 'vbasis_info'):
            typ = "array of BasisStatus"
        elif name == 'basis_info':
            typ = "BasisStatus"
        elif name == 'solvestat':
            typ = "SolveStatus"
        elif name == 'ei' or name == 'objequ':
            typ = "EquationRef"
        else:
            typ = ""
    else:
        typ = argout_c2swig.get(typ, "")

    if typ == "":
        print(f"ERROR: unhandled type {arg.type.strip()} in argout {arg}")

    return FnRet(name="", type=typ, descr=descr)

def fnRet_c2swig(ret: FnRet) -> FnRet:
    descr = ret.descr.strip()
    typ = type_c2swig(ret.type)
    assert typ != 'void', f"wrong return type {ret}"

    if typ == 'rhp_idx':
        if 'variable' in descr:
            typ = 'VariableRef'
        elif 'equation' in descr:
            typ = 'EquationRef'
        else:
            print(f"ERROR: unhandle rhp_idx case in {ret}")

    else:
        typ = types_c2swig.get(typ, "")
        if typ == "":
            print(f"ERROR: unhandled type {ret.type.strip()} in ret {ret}")

    return FnRet(name=ret.name.strip(), type=typ, descr=descr)

def fnDecl_c2swig(fn: FnDecl) -> Union[FnDecl,None]:
    "Transforms a C function declaration to a SWIG one"

    if fn.name in reshop_ignore: return None

    name = fnName_c2swig(fn.name)

    fnswig = FnDecl(name=name, brief_desc=fn.brief_desc.strip(), detailed_desc=fn.detailed_desc.strip(), ret=[], args=[], extra={})

    argout_multiple_start = list(reshop_argout_multiple.keys())
    c_args = fn.args
    swig_args = []
    swig_ret = []
    idx = 0
    sz = len(c_args)

    while (idx < sz):
        arg = c_args[idx]
        isArgout = False

        typ = type_c2swig(arg.type)
        if (typ, arg.name) in argout_multiple_start:
            # print(f"DEBUG: match for {(typ, arg.name)}")

            argouts = reshop_argout_multiple[(typ, arg.name)]

            for argout in argouts:
                additional_argouts = argout["args"]
                idx_ = idx+1
                if len(additional_argouts) > sz-idx_:
                    # print(f"DEBUG: not enough args for {(typ, arg.name)}: {sz-idx_} vs {len(additional_argouts)}")
                    continue #quick check
                match = True
                for arg_ in additional_argouts:
                    argN = c_args[idx_]
                    typ_ = type_c2swig(argN.type)
                    if (typ_, argN.name) != arg_:
                        # print(f"DEBUG: no match because of {(typ_, argN.name)} vs {arg_}")
                        match = False
                        break
                    idx_ += 1
                if match:
                    swig_ret.append(argout["ret"])
                    isArgout = True
                    idx = idx_

        if not isArgout:
            swig_args.append(arg)

        idx += 1

    # Go through all arguments and identify the ones that are output
    for arg in swig_args:
        typ = type_c2swig(arg.type)
        if arg.name in reshop_argname2ret or (typ, arg.name) in reshop_argout_simple:
            fnswig.ret.append(fnArgOut_c2swig(arg))
            if arg.direction != 'out':
                print(f"ERROR: in function {fn.name}, argument {arg.name} is considered as output, but direction is '{arg.direction}', should be 'out'")

            continue

        if 'out' == arg.direction:
                print(f"ERROR: in function {fn.name}, argument '{arg.type} {arg.name}' has direction '{arg.direction}' but is not considered as output in the script")

        # Normal arg
        fnswig.args.append(fnArg_c2swig(arg))

    for ret in fn.ret:
        if ret.type == 'int' or ret.type == 'void': #skipping int, it is meant as a return code
            continue
        fnswig.ret.append(fnRet_c2swig(ret))

    for ret in swig_ret:
        fnswig.ret.append(fnRet_c2swig(ret))

    return fnswig



def rstHeading(s: str):
    return ("\n", s+"\n", '-'*len(s) + "\n")


def rstMath(s: str):
    """ s is expected to be of the form $math$"""
    assert s[0] == '$' and s[-1] == '$'
    return f":math:`{s[1:-1].strip().replace('\\', '\\\\')}`"

ignore_tags = ('itemizedlist', 'linebreak', 'simplesect', 'parameterlist')

def processTag(e) -> str:
    if e.tag == 'formula':
        return rstMath(e.text)
    elif e.tag == 'ref':
        return e.text.strip() if e.text else ""
    elif e.tag in ignore_tags:
        return ""
 
    print(f"ERROR: unhandled tag {e.tag} in {e}")
    return "ERROR"

def parse_mixed_text(e) -> str:
    if e is None: return ""
    elts = [e.text if e.text else ""]
    for c in e.iterchildren():
        ctext = processTag(c)
        elts.append(ctext)
        if c.tail:
            elts.append(c.tail.strip())

    return " ".join(elts)

def writeSwigDoc(fns: dict[str,FnDecl]):
    with open(Path("generated").joinpath("reshop_docs.i"), "w") as f:
        f.write(f"// Generated by {Path(sys.argv[0]).name}\n\n")
        for fnName, fn in fns.items():
            assert fnName == fn.name
            if fnName in reshop_ignore: continue
 
            fnSwig = fnDecl_c2swig(fn)
            if not fnSwig: continue
 
            f.write('%feature("autodoc", "')
            f.write(f"{fmt_pyfn(fnSwig)}\n--\n\n")
            fmt_as_numpydoc(fnSwig, f)
            f.write(f'") rhp_{fnSwig.name};\n')
 
        for (fnMerged, fnListName) in reshop_merging:
            fnList = [fns[n] for n in fnListName]
            fnMerged = fnDecl_combine(fnList, fnMerged)
 
            f.write('%feature("autodoc", "')
            fmt_as_numpydoc(fnMerged, f)
            f.write(f'") rhp_{fnMerged.name};\n')

def writeRstPython(fns: dict[str,FnDecl]):
    with open(Path("generated").joinpath("reshop_docs.py"), "w") as f:
        f.write(f"# Generated by {Path(sys.argv[0]).name}\n\n")
        for fnName, fn in fns.items():
            assert fnName == fn.name
            if fnName in reshop_ignore: continue
 
            fnSwig = fnDecl_c2swig(fn)
            if not fnSwig: continue
 
            f.write(f"def {fmt_pyfn(fnSwig)}:\n")
            f.write('    r"""\n')
            fmt_as_numpydoc(fnSwig, f)
            f.write('    """\n\n')

def docstring_pymethods(fns: dict[str,FnDecl]) -> dict[str,str]:
    res = {}
    for fnName, fn in fns.items():
        assert fnName == fn.name
        if fnName in reshop_ignore: continue
 
        fnSwig = fnDecl_c2swig(fn)
        if not fnSwig: continue

        margs = fnSwig.args
        if margs:
            margs.pop(0)
        fnSwig = fnSwig._replace(name=fnName.replace("rhp_mdl_", "").replace("rhp_mp_", "").replace("rhp_", ""), args=margs)
 
        out = StringIO(f"{fmt_pyfn(fnSwig)}\n--\n\n")
        out.seek(0, os.SEEK_END) # Append does not really work with StringIO

        fmt_as_numpydoc(fnSwig, out)
        res[fnName] = out.getvalue()
    return res

def reshop_optional_argnames(n: str) -> tuple:
    if n.endswith("named"):
        return ("name", )
    else:
        return tuple()

def main(fname: str):
    fndecls = {}
    index_fpath = Path(fname)
    fdir = index_fpath.parent

    index_tree = etree.parse(index_fpath)
    publicAPI_tree = etree.parse(fdir.joinpath("group__publicAPI.xml"))

    API_header_node = index_tree.find(f".//compound[@kind='file'][name='reshop.h']")
    file_id = API_header_node.get('refid')
    file_xml_path = fdir.joinpath(f"{file_id}.xml")
    API_header_tree = etree.parse(file_xml_path)

    func_nodes = API_header_tree.findall(f".//memberdef[@kind='function']") + API_header_tree.findall(f".//member[@kind='function']")
    if not func_nodes:
        print(f"ERROR: Could not get any function in {file_xml_path}")

    publicFn = [func_node.findtext("name") for func_node in func_nodes]

    overloadedFn = []
    renamedFn = {}

    funcs = publicAPI_tree.findall(".//sectiondef[@kind='func']/memberdef[@kind='function']")
    for func_node in funcs:

        name = func_node.findtext("name").strip()

        patterns = (f'{name}named', f'{name}_named')

        skipFn = False
        for pattern in patterns:
            if publicAPI_tree.find(f".//sectiondef[@kind='func']/memberdef[@kind='function'][name='{pattern}']") is not None:
                overloadedFn.append(name)
                renamedFn[pattern] = name
                skipFn = True
                break

        if skipFn: continue

        brief_desc = parse_mixed_text(func_node.find("./briefdescription/para"))
        detailed_desc = parse_mixed_text(func_node.find("./detaileddescription/para"))
        return_desc = parse_mixed_text(func_node.find("./detaileddescription/para/simplesect[@kind='return']/para"))
        #return_type = " ".join(c.strip() for c in func_node.find("type").itertext())
        return_type = parse_mixed_text(func_node.find("type"))

        # 5. Extract and print parameters
        params = []
        for param_node in func_node.findall("./detaileddescription/para/parameterlist/parameteritem"):
            pname_xml = param_node.find("./parameternamelist/parametername")
            pname = pname_xml.text
            pdir = pname_xml.attrib.get("direction", "")
            pdesc = param_node.findtext("parameterdescription/para")

            if pname:
                ptype_xml = func_node.find(f"./param[declname='{pname}']")
                if ptype_xml is None:
                    if pname == 'ctr':
                        ptype = 'Model *'
                        pdesc = 'the model'
                        pname = 'mdl'
                    else:
                        print(f"ERROR: could not find type for parameter {pname} in function {name}")
                        ptype = ""
                else:
                    ptype = " ".join(c.strip() for c in ptype_xml.itertext("type"))
                    ptype = parse_mixed_text(ptype_xml.find("type"))
            else:
                print(f"ERROR: in {name}, missing argument name")
                ptype = ""

            params.append(FnArg(name=pname.strip(), type=ptype.strip(), direction=pdir.strip(), descr=pdesc.strip()))
 
        ret = FnRet(name="", type=return_type, descr=return_desc.strip() if return_desc else "")
        fndecl = FnDecl(name=name,
                        brief_desc=brief_desc.strip() if brief_desc else "",
                        detailed_desc=detailed_desc.strip() if detailed_desc else "",
                        args=params,
                        ret=[ret],
                        extra={})
        fndecls[name] = fndecl

    #writeRst(funcs)

    # simple check
    publicFnSet = set(publicFn)
    fnDeclsSet = set(fndecls.keys())

    # The ignored functions are not relevant
    pubNotDecl = publicFnSet - fnDeclsSet - set(reshop_ignore) - set(overloadedFn)
    if pubNotDecl:
        print("ERROR: the following public function(s) are not properly documented")
        for name in sorted(list(pubNotDecl)):
            print(name)

    fnDeclNotPublic = fnDeclsSet - publicFnSet
    if fnDeclNotPublic:
        print("ERROR: the following documented function(s) are not public")
        for name in sorted(list(fnDeclNotPublic)):
            print(name)

    for fnName, fnName_ in renamedFn.items():
        optional_argnames = reshop_optional_argnames(fnName)
        fndecl = fndecls[fnName]
        fnargs = fndecl.args
        optional_fnargs = []
        for argname in optional_argnames:
            for fnarg in fnargs:
                if fnarg.name == argname:
                    optional_fnargs.append(fnarg)
                    break
        if len(optional_argnames) != len(optional_fnargs):
            print(f"ERROR: expected {len(optional_argnames)} optional argument, but only found {len(optional_fnargs)}: {optional_fnargs}")

        for opt_fnarg in optional_fnargs:
            fnargs.remove(opt_fnarg)


        fndecls[fnName_] = fndecls.pop(fnName)._replace(name=fnName_,args=fnargs, extra=fndecl.extra | {'args_optional': optional_fnargs})

    writeSwigDoc(fndecls)
    writeRstPython(fndecls)
    pymethods_str = docstring_pymethods(fndecls)

    with open(Path("generated").joinpath("pyobj_methods_docstring.i.in"), "r") as f:
        pyobj_methods_docstring = f.readlines()

    with open(Path("generated").joinpath("pyobj_methods_docstring.i"), "w") as f:
        f.write(f"// Generated by {Path(sys.argv[0]).name}\n\n")
        f.writelines(line.format(fndecls=pymethods_str) for line in pyobj_methods_docstring)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("ERROR: need to provide one argument: the XML index file")

    script = Path(sys.argv[0])
    with open(script.parent.joinpath("reshop_generators_config.csv"), "r") as f:
        reader = csv.reader(f)

        for row in reader:
            if row and row[0]:
                reshop_ignore.append(row[0])

    main(sys.argv[1])
